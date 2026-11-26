"""Runs Diablo II for an agent, in a sandbox that cannot reach the player's game.

    python AgentHarness/bhctl.py base                 # (re)build the shared base
    python AgentHarness/bhctl.py create [--bh DLL]    # new sandbox, prints its id
    python AgentHarness/bhctl.py launch ID [-- ARGS]  # start its game, hidden
    python AgentHarness/bhctl.py status ID            # running, and its windows
    python AgentHarness/bhctl.py close ID             # ask the game to quit
    python AgentHarness/bhctl.py teardown ID          # kill, delete files and key
    python AgentHarness/bhctl.py sweep                # tear down every dead run
    python AgentHarness/bhctl.py snapshot FILE        # the player's state, for
    python AgentHarness/bhctl.py compare A B          #   proving a run left it be

Layout, under %BHAGENT_ROOT% (default ~\\.bhagent):

    base\\        one read-only copy of the game's own files, shared by all runs
    runs\\<id>\\  a run's sandbox: hardlinks into base, plus its own BH.dll,
                  configs and Save folder. Deleted by teardown.

The root is outside AppData on purpose. The Claude desktop app is a packaged
app, and Windows quietly moves whatever it and its children write under AppData
into the package's own folder, so a root there would be a different folder
depending on which shell ran this.

Each run's game sees HKCU\\Software\\BHAgent\\runs\\<id>\\HKCU as its whole
HKEY_CURRENT_USER (see AgentShim.cpp), which teardown deletes with the files.

A run is alive exactly while its AgentLaunch is: Game.exe denies every other
process any access to itself, so the launcher is the only thing that can see it.
"""

import argparse
import ctypes
import ctypes.wintypes as wt
import json
import os
import secrets
import shutil
import subprocess
import sys
import threading
import time
import winreg
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
HARNESS_BIN = REPO / "AgentHarness" / "bin" / "Release"
ROOT = Path(os.environ.get("BHAGENT_ROOT", Path.home() / ".bhagent"))
BASE = ROOT / "base"
RUNS = ROOT / "runs"
RUNS_KEY = r"Software\BHAgent\runs"
SANDBOX_MARKER = "BHAgent.sandbox"
RUN_FILE = "run.json"
LIFETIME_SECONDS = 3600
# A sandbox nobody has launched yet is left alone by sweep for this long, so
# that one agent's sweep cannot take another's sandbox between create and launch.
UNLAUNCHED_GRACE_SECONDS = 600

# Where the base is copied from: the dev install, read and never written.
GAME_SOURCE = Path(os.environ.get("BHAGENT_GAME_SOURCE", r"C:\Program Files (x86)\Diablo II GLIDE 2"))

# The game's own files, and nothing of the player's or of the other mods in the
# source install. SlashDiablo.dll, the realm gateway, is left out on purpose:
# D2Template skips a DLL that is not there, and a sandbox never goes online.
BASE_FILES = [
    "Game.exe", "D2.LNG",
    "Bnclient.dll", "D2CMP.dll", "D2Client.dll", "D2Common.dll", "D2DDraw.dll",
    "D2Direct3D.dll", "D2Game.dll", "D2Gdi.dll", "D2gfx.dll", "D2Glide.dll",
    "D2Lang.dll", "D2Launch.dll", "D2MCPClient.dll", "D2Multi.dll", "D2Net.dll",
    "D2sound.dll", "D2Win.dll", "D2Template.dll", "Fog.dll", "Storm.dll",
    "ijl11.dll", "binkw32.dll", "SmackW32.dll",
    "SGD2FreeRes.dll", "SGD2FreeRes.mpq", "SGD2FreeDisplayFix.dll",
    # Never the renderer, since a sandbox runs windowed and D2GL refuses -w.
    # SGD2FreeRes loads it whatever the video mode, to ask which Glide wrapper
    # it is, and stops the game with a dialog when it is missing.
    "glide3x.dll",
    "d2char.mpq", "d2data.mpq", "d2exp.mpq", "d2music.mpq", "d2sfx.mpq",
    "d2speech.mpq", "d2video.mpq", "d2xmusic.mpq", "d2xtalk.mpq", "d2xvideo.mpq",
    "Patch_D2.mpq",
]

# Copied, not linked, because something in the game writes them.
SANDBOX_COPIES = {
    "BH.cfg": REPO / "Packaging" / "BH.cfg",
    "BH_settings.cfg": REPO / "Packaging" / "BH_settings.cfg",
    "buffs.mpq": REPO / "Packaging" / "buffs.mpq",
}

# The in-game resolution a run starts at: the one Slash Diablo players use.
RESOLUTION = "1344x700"
SGD2_CONFIG = "SGD2FreeResolution.json"

# The player's own state, which no run may change. Read-only to this script.
PLAYER_KEYS = [
    r"Software\Blizzard Entertainment",
    r"Software\Classes\VirtualStore\MACHINE\SOFTWARE\WOW6432Node\Blizzard Entertainment",
    r"Software\Classes\VirtualStore\MACHINE\SOFTWARE\Blizzard Entertainment",
]
PLAYER_DIRS = [
    Path(r"C:\Program Files (x86)\Diablo II GLIDE"),
    Path(r"C:\Program Files (x86)\Diablo II GLIDE 2"),
    Path.home() / "Saved Games" / "Diablo II",
    Path(os.environ["LOCALAPPDATA"]) / "VirtualStore" / "Program Files (x86)",
]

kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
user32 = ctypes.WinDLL("user32", use_last_error=True)


# --- base --------------------------------------------------------------------

def build_base():
    """Copies the game's files into base once, and again only where the source
    has changed. Every file is marked read-only, so a sandbox that tries to
    write through one of its links fails instead of changing every run."""
    BASE.mkdir(parents=True, exist_ok=True)
    for name in BASE_FILES:
        source = GAME_SOURCE / name
        target = BASE / name
        stat = source.stat()
        if target.exists():
            have = target.stat()
            if have.st_size == stat.st_size and int(have.st_mtime) == int(stat.st_mtime):
                continue
            clear_readonly(target)
        print(f"copying {name} ({stat.st_size // (1 << 20)} MB)")
        shutil.copy2(source, target)
        set_readonly(target)
    stray = {p.name for p in BASE.iterdir()} - set(BASE_FILES)
    for name in sorted(stray):
        print(f"removing {name}, no longer part of the base")
        clear_readonly(BASE / name)
        (BASE / name).unlink()
    print(f"base ready: {BASE}")


def set_readonly(path):
    os.chmod(path, 0o444)


def clear_readonly(path):
    os.chmod(path, 0o666)


# --- sandboxes ----------------------------------------------------------------

def run_dir(run_id):
    path = RUNS / run_id
    if not (path / SANDBOX_MARKER).exists():
        sys.exit(f"no sandbox {run_id}")
    return path


def run_key(run_id):
    return rf"{RUNS_KEY}\{run_id}\HKCU"


def create(bh_dll):
    for name in BASE_FILES:
        if not (BASE / name).exists():
            sys.exit("the base is missing or incomplete: run `bhctl.py base` first")
    if not bh_dll.exists():
        sys.exit(f"no BH.dll at {bh_dll}")

    run_id = time.strftime("%Y%m%d-%H%M%S-") + secrets.token_hex(2)
    path = RUNS / run_id
    (path / "Save").mkdir(parents=True)
    for name in BASE_FILES:
        os.link(BASE / name, path / name)
    for name, source in SANDBOX_COPIES.items():
        shutil.copyfile(source, path / name)
    write_sgd2_config(path / SGD2_CONFIG)
    shutil.copyfile(bh_dll, path / "BH.dll")
    write_registry(run_id, path)
    (path / SANDBOX_MARKER).write_text(run_id + "\n")
    print(run_id)


def write_sgd2_config(target):
    """SGD2FreeRes keeps the in-game resolution in its own config, as an index
    into its list counted from 0, and falls back to 640x480 on an index it does
    not have. The source install's config is the template, so the metadata
    matches the SGD2FreeRes that reads it, but its index is the player's and is
    not carried over."""
    config = json.loads((GAME_SOURCE / SGD2_CONFIG).read_text())
    settings = config["SlashGaming Diablo II Free Resolution"]
    resolutions = settings["Ingame Resolutions"]
    if RESOLUTION not in resolutions:
        sys.exit(f"{GAME_SOURCE / SGD2_CONFIG} does not offer {RESOLUTION}")
    settings["Ingame Resolution Mode"] = resolutions.index(RESOLUTION)
    target.write_text(json.dumps(config, indent=4) + "\n")


def write_registry(run_id, path):
    """The whole of what the run's game sees as HKEY_CURRENT_USER. Only what
    the game needs to find itself: its options start at the game's defaults."""
    game = rf"{run_key(run_id)}\Software\Blizzard Entertainment\Diablo II"
    with winreg.CreateKey(winreg.HKEY_CURRENT_USER, game) as key:
        winreg.SetValueEx(key, "InstallPath", 0, winreg.REG_SZ, f"{path}\\")
        winreg.SetValueEx(key, "Save Path", 0, winreg.REG_SZ, f"{path}\\Save\\")
        winreg.SetValueEx(key, "NewSavePath", 0, winreg.REG_SZ, f"{path}\\Save\\")
        winreg.SetValueEx(key, "UseCmdLine", 0, winreg.REG_DWORD, 0)


def delete_tree(root, subkey):
    try:
        with winreg.OpenKey(root, subkey, 0, winreg.KEY_ALL_ACCESS) as key:
            while True:
                try:
                    child = winreg.EnumKey(key, 0)
                except OSError:
                    break
                delete_tree(root, rf"{subkey}\{child}")
        winreg.DeleteKey(root, subkey)
    except FileNotFoundError:
        pass


# A hardlink shares its attributes with base, so clearing read-only to delete
# one would unprotect the file for every run. FileDispositionInfoEx deletes it
# as it stands (Windows 10 1809 and later).
FILE_DISPOSITION_INFO_EX = 21
FILE_DISPOSITION_FLAG_DELETE = 0x1
FILE_DISPOSITION_FLAG_POSIX_SEMANTICS = 0x2
FILE_DISPOSITION_FLAG_IGNORE_READONLY_ATTRIBUTE = 0x10
DELETE = 0x00010000
OPEN_EXISTING = 3
FILE_SHARE_ALL = 0x7
INVALID_HANDLE = wt.HANDLE(-1).value

kernel32.CreateFileW.restype = wt.HANDLE
kernel32.CreateFileW.argtypes = [wt.LPCWSTR, wt.DWORD, wt.DWORD, ctypes.c_void_p, wt.DWORD, wt.DWORD, wt.HANDLE]
kernel32.SetFileInformationByHandle.argtypes = [wt.HANDLE, ctypes.c_int, ctypes.c_void_p, wt.DWORD]
kernel32.CloseHandle.argtypes = [wt.HANDLE]


def delete_file(path):
    handle = kernel32.CreateFileW(str(path), DELETE, FILE_SHARE_ALL, None, OPEN_EXISTING, 0, None)
    if handle == INVALID_HANDLE:
        raise ctypes.WinError(ctypes.get_last_error())
    try:
        flags = wt.DWORD(FILE_DISPOSITION_FLAG_DELETE | FILE_DISPOSITION_FLAG_POSIX_SEMANTICS
                         | FILE_DISPOSITION_FLAG_IGNORE_READONLY_ATTRIBUTE)
        if not kernel32.SetFileInformationByHandle(handle, FILE_DISPOSITION_INFO_EX,
                                                   ctypes.byref(flags), ctypes.sizeof(flags)):
            raise ctypes.WinError(ctypes.get_last_error())
    finally:
        kernel32.CloseHandle(handle)


def delete_sandbox(path):
    for child in sorted(path.rglob("*"), key=lambda p: len(p.parts), reverse=True):
        if child.is_dir():
            child.rmdir()
        else:
            delete_file(child)
    path.rmdir()


def teardown(run_id):
    path = RUNS / run_id
    run = read_run(path)
    if running(run):
        stop(run["launcher"])
    if path.exists():
        if not (path / SANDBOX_MARKER).exists():
            sys.exit(f"{path} is not a sandbox; not deleting it")
        delete_sandbox(path)
    delete_tree(winreg.HKEY_CURRENT_USER, rf"{RUNS_KEY}\{run_id}")
    print(f"torn down {run_id}")


def sweep():
    if RUNS.exists():
        for path in RUNS.iterdir():
            run = read_run(path)
            if run is None:
                if time.time() - path.stat().st_ctime > UNLAUNCHED_GRACE_SECONDS:
                    teardown(path.name)
            elif not process_alive(run["launcher"]):
                teardown(path.name)


# --- running ----------------------------------------------------------------

def read_run(path):
    try:
        return json.loads((path / RUN_FILE).read_text())
    except FileNotFoundError:
        return None


def desktop_name(run_id):
    return f"BHAgent-{run_id}"


def launch(run_id, game_args, lifetime):
    path = run_dir(run_id)
    if read_run(path) and process_alive(read_run(path)["launcher"]):
        sys.exit(f"{run_id} is already running")
    launcher = HARNESS_BIN / "AgentLaunch.exe"
    log = open(path / "launcher.log", "w")
    process = subprocess.Popen(
        [str(launcher), "--sandbox", str(path), "--registry", run_key(run_id),
         "--desktop", desktop_name(run_id), "--lifetime", str(int(lifetime)), "--", *game_args],
        stdout=subprocess.PIPE, stderr=log, text=True,
        creationflags=subprocess.CREATE_NEW_PROCESS_GROUP | subprocess.CREATE_NO_WINDOW)
    line = process.stdout.readline().strip()
    if not line.startswith("pid="):
        process.wait()
        log.close()
        sys.exit(f"launch failed ({process.returncode}): {(path / 'launcher.log').read_text().strip()}")
    run = {"launcher": process.pid, "game": int(line[4:]), "args": game_args, "started": time.time()}
    (path / RUN_FILE).write_text(json.dumps(run, indent=2))
    print(json.dumps(run))


PROCESS_QUERY_LIMITED_INFORMATION = 0x1000
STILL_ACTIVE = 259
kernel32.OpenProcess.restype = wt.HANDLE
kernel32.GetExitCodeProcess.argtypes = [wt.HANDLE, ctypes.POINTER(wt.DWORD)]


def process_alive(pid):
    """For AgentLaunch, which is ours to open. Never for Game.exe, which no
    other process may open, so this would always answer that it had exited."""
    handle = kernel32.OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, False, pid)
    if not handle:
        return False
    try:
        code = wt.DWORD()
        return bool(kernel32.GetExitCodeProcess(handle, ctypes.byref(code))) and code.value == STILL_ACTIVE
    finally:
        kernel32.CloseHandle(handle)


def running(run):
    return run is not None and process_alive(run["launcher"])


SYNCHRONIZE = 0x00100000
EVENT_MODIFY_STATE = 0x0002
WAIT_OBJECT_0 = 0
kernel32.OpenEventW.restype = wt.HANDLE
kernel32.OpenEventW.argtypes = [wt.DWORD, wt.BOOL, wt.LPCWSTR]
kernel32.SetEvent.argtypes = [wt.HANDLE]
kernel32.WaitForSingleObject.argtypes = [wt.HANDLE, wt.DWORD]


def stop(launcher, timeout_ms=30000):
    """Ends a run's game and returns once it has gone, which is what frees the
    sandbox's files: AgentLaunch only exits after the game has. A launcher that
    does not answer is killed, which ends the game through its job."""
    handle = kernel32.OpenProcess(SYNCHRONIZE, False, launcher)
    if not handle:
        return
    try:
        event = kernel32.OpenEventW(EVENT_MODIFY_STATE, False, f"Local\\BHAgentStop-{launcher}")
        if event:
            kernel32.SetEvent(event)
            kernel32.CloseHandle(event)
            if kernel32.WaitForSingleObject(handle, timeout_ms) == WAIT_OBJECT_0:
                return
        subprocess.run(["taskkill", "/PID", str(launcher), "/F"], capture_output=True)
        kernel32.WaitForSingleObject(handle, timeout_ms)
    finally:
        kernel32.CloseHandle(handle)


GENERIC_ALL = 0x10000000
GW_CHILD = 5
GW_HWNDNEXT = 2
WNDENUMPROC = ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)
user32.OpenDesktopW.restype = wt.HANDLE
user32.SetThreadDesktop.argtypes = [wt.HANDLE]
user32.CloseDesktop.argtypes = [wt.HANDLE]
user32.EnumDesktopWindows.argtypes = [wt.HANDLE, WNDENUMPROC, wt.LPARAM]
user32.GetWindow.restype = wt.HWND
user32.GetWindow.argtypes = [wt.HWND, wt.UINT]
user32.PostMessageW.argtypes = [wt.HWND, wt.UINT, wt.WPARAM, wt.LPARAM]


def on_desktop(run_id, work):
    """Runs work() on a thread attached to the run's desktop and returns what it
    returns. Window calls on another desktop's windows fail as if the handle
    were invalid, so everything that touches the game's windows goes through
    here. A fresh thread, because a thread that owns a window cannot move."""
    result = {}

    def body():
        desktop = user32.OpenDesktopW(desktop_name(run_id), 0, False, GENERIC_ALL)
        if not desktop:
            result["error"] = ctypes.WinError(ctypes.get_last_error())
            return
        try:
            if not user32.SetThreadDesktop(desktop):
                result["error"] = ctypes.WinError(ctypes.get_last_error())
                return
            result["value"] = work(desktop)
        finally:
            user32.CloseDesktop(desktop)

    thread = threading.Thread(target=body)
    thread.start()
    thread.join()
    if "error" in result:
        raise result["error"]
    return result["value"]


def window_text(hwnd):
    text = ctypes.create_unicode_buffer(1024)
    user32.GetWindowTextW(hwnd, text, 1024)
    return text.value


def window_class(hwnd):
    name = ctypes.create_unicode_buffer(256)
    user32.GetClassNameW(hwnd, name, 256)
    return name.value


user32.GetWindowThreadProcessId.argtypes = [wt.HWND, ctypes.POINTER(wt.DWORD)]


def window_process(hwnd):
    owner = wt.DWORD()
    user32.GetWindowThreadProcessId(hwnd, ctypes.byref(owner))
    return owner.value


def list_windows(desktop, pid):
    """The game's top-level windows. Windows owns a few of its own on every
    desktop, so the game's are picked out by process. A dialog carries its
    text, which is usually the only account of why the game has stopped."""
    handles = []
    user32.EnumDesktopWindows(desktop, WNDENUMPROC(lambda hwnd, _: handles.append(hwnd) or True), 0)
    windows = []
    for hwnd in handles:
        if window_process(hwnd) != pid:
            continue
        rect = wt.RECT()
        user32.GetClientRect(hwnd, ctypes.byref(rect))
        window = {"hwnd": hwnd, "title": window_text(hwnd), "class": window_class(hwnd),
                  "visible": bool(user32.IsWindowVisible(hwnd)), "client": [rect.right, rect.bottom]}
        if window["class"] == "#32770":
            child = user32.GetWindow(hwnd, GW_CHILD)
            texts = []
            while child:
                if window_text(child):
                    texts.append(window_text(child))
                child = user32.GetWindow(child, GW_HWNDNEXT)
            window["text"] = texts
        windows.append(window)
    return windows


def status(run_id):
    run = read_run(run_dir(run_id))
    report = {"id": run_id, "running": running(run), **(run or {})}
    if report["running"]:
        windows = on_desktop(run_id, lambda desktop: list_windows(desktop, run["game"]))
        report["windows"] = [w for w in windows if w["visible"]]
    print(json.dumps(report, indent=2))


WM_CLOSE = 0x0010


def close(run_id, timeout):
    """Asks the game to quit the way closing its window does, so it saves and
    writes its options as it would for a player."""
    run = read_run(run_dir(run_id))
    if not running(run):
        sys.exit(f"{run_id} is not running")

    def post_close(desktop):
        windows = [w for w in list_windows(desktop, run["game"]) if w["visible"]]
        for window in windows:
            user32.PostMessageW(window["hwnd"], WM_CLOSE, 0, 0)
        return len(windows)

    if not on_desktop(run_id, post_close):
        sys.exit("the game has no window to close")
    deadline = time.time() + timeout
    while running(run):
        if time.time() > deadline:
            sys.exit(f"the game did not exit within {timeout}s")
        time.sleep(0.2)
    print(f"{run_id} exited")


# --- the player's state -------------------------------------------------------

class FILETIME(ctypes.Structure):
    _fields_ = [("low", wt.DWORD), ("high", wt.DWORD)]


advapi32 = ctypes.WinDLL("advapi32", use_last_error=True)
advapi32.RegQueryInfoKeyW.argtypes = [wt.HKEY] + [ctypes.c_void_p] * 10 + [ctypes.POINTER(FILETIME)]


def key_written(key):
    written = FILETIME()
    advapi32.RegQueryInfoKeyW(key.handle, *([None] * 10), ctypes.byref(written))
    return (written.high << 32) | written.low


def snapshot_key(subkey, into):
    try:
        key = winreg.OpenKey(winreg.HKEY_CURRENT_USER, subkey, 0, winreg.KEY_READ)
    except FileNotFoundError:
        return
    with key:
        values = {}
        children = []
        i = 0
        while True:
            try:
                name, data, kind = winreg.EnumValue(key, i)
            except OSError:
                break
            values[name] = [kind, data.hex() if isinstance(data, bytes) else data]
            i += 1
        i = 0
        while True:
            try:
                children.append(winreg.EnumKey(key, i))
            except OSError:
                break
            i += 1
        into[subkey] = {"written": key_written(key), "values": values}
    for child in children:
        snapshot_key(rf"{subkey}\{child}", into)


def snapshot_dir(root, into):
    if not root.exists():
        return
    for path in root.rglob("*"):
        try:
            stat = path.stat()
        except OSError:
            continue
        into[str(path)] = [stat.st_size, stat.st_mtime_ns]


def snapshot(out):
    state = {"registry": {}, "files": {}}
    for subkey in PLAYER_KEYS:
        snapshot_key(subkey, state["registry"])
    for root in PLAYER_DIRS:
        snapshot_dir(root, state["files"])
    Path(out).write_text(json.dumps(state, indent=1))
    print(f"{len(state['registry'])} keys, {len(state['files'])} files -> {out}")


def compare(a, b):
    before = json.loads(Path(a).read_text())
    after = json.loads(Path(b).read_text())
    changed = 0
    for section in ("registry", "files"):
        old, new = before[section], after[section]
        for name in sorted(set(old) | set(new)):
            if old.get(name) != new.get(name):
                changed += 1
                kind = "added" if name not in old else "removed" if name not in new else "changed"
                print(f"{section} {kind}: {name}")
                if section == "registry" and kind == "changed":
                    for value in sorted(set(old[name]["values"]) | set(new[name]["values"])):
                        if old[name]["values"].get(value) != new[name]["values"].get(value):
                            print(f"    {value}: {old[name]['values'].get(value)} -> {new[name]['values'].get(value)}")
    print("no changes" if not changed else f"{changed} changes")
    return changed


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("base")
    create_cmd = commands.add_parser("create")
    create_cmd.add_argument("--bh", type=Path, default=REPO / "Release" / "BH.dll")
    launch_cmd = commands.add_parser("launch")
    launch_cmd.add_argument("id")
    launch_cmd.add_argument("--lifetime", type=float, default=LIFETIME_SECONDS,
                            help="seconds before the launcher ends the game regardless")
    for name in ("status", "teardown"):
        commands.add_parser(name).add_argument("id")
    close_cmd = commands.add_parser("close")
    close_cmd.add_argument("id")
    close_cmd.add_argument("--timeout", type=float, default=30)
    commands.add_parser("sweep")
    commands.add_parser("snapshot").add_argument("file")
    compare_cmd = commands.add_parser("compare")
    compare_cmd.add_argument("a")
    compare_cmd.add_argument("b")
    # Everything after "--" is the game's, split off by hand: argparse's
    # REMAINDER also swallows the options that follow a positional.
    argv = sys.argv[1:]
    game_args = argv[argv.index("--") + 1:] if "--" in argv else []
    args = parser.parse_args(argv[:argv.index("--")] if "--" in argv else argv)

    if args.command == "base":
        build_base()
    elif args.command == "create":
        create(args.bh)
    elif args.command == "launch":
        launch(args.id, game_args or ["-w", "-ns"], args.lifetime)
    elif args.command == "status":
        status(args.id)
    elif args.command == "close":
        close(args.id, args.timeout)
    elif args.command == "teardown":
        teardown(args.id)
    elif args.command == "sweep":
        sweep()
    elif args.command == "snapshot":
        snapshot(args.file)
    elif args.command == "compare":
        sys.exit(1 if compare(args.a, args.b) else 0)


if __name__ == "__main__":
    main()
