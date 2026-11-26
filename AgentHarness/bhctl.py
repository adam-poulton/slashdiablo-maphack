"""Runs Diablo II for an agent, in a sandbox that cannot reach the player's game.

    python AgentHarness/bhctl.py base                 # (re)build the shared base
    python AgentHarness/bhctl.py create [--bh DLL]    # new sandbox, prints its id
    python AgentHarness/bhctl.py launch ID [-- ARGS]  # start its game, hidden
    python AgentHarness/bhctl.py enter ID             # into a game in Normal
    python AgentHarness/bhctl.py input ID ACTION...   # click, type, press keys
    python AgentHarness/bhctl.py shot ID FILE.png     # screenshot the game
    python AgentHarness/bhctl.py state ID             # the screen, the character
    python AgentHarness/bhctl.py wait ID SCREEN...    # until the game shows one
    python AgentHarness/bhctl.py drawlog ID           # what BH drew in a frame
    python AgentHarness/bhctl.py leave ID             # save and exit the game
    python AgentHarness/bhctl.py status ID            # running, and its windows
    python AgentHarness/bhctl.py close ID             # quit the game as a player
    python AgentHarness/bhctl.py teardown ID          # kill, delete files and key
    python AgentHarness/bhctl.py scenario FILE.json   # all of it, with a report
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
import struct
import subprocess
import sys
import threading
import time
import winreg
import zlib
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
READY_TIMEOUT_SECONDS = 60
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


class HarnessError(Exception):
    """A run cannot go on. The message is for the agent, and says why."""


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
        raise HarnessError(f"no sandbox {run_id}")
    return path


def run_key(run_id):
    return rf"{RUNS_KEY}\{run_id}\HKCU"


def create(bh_dll, settings=None):
    """A new sandbox, with BH's shipped settings except for those given, a
    dict of BH_settings.cfg keys to the values to write for them."""
    for name in BASE_FILES:
        if not (BASE / name).exists():
            raise HarnessError("the base is missing or incomplete: run `bhctl.py base` first")
    if not bh_dll.exists():
        raise HarnessError(f"no BH.dll at {bh_dll}")

    run_id = time.strftime("%Y%m%d-%H%M%S-") + secrets.token_hex(2)
    path = RUNS / run_id
    (path / "Save").mkdir(parents=True)
    for name in BASE_FILES:
        os.link(BASE / name, path / name)
    for name, source in SANDBOX_COPIES.items():
        shutil.copyfile(source, path / name)
    if settings:
        target = path / "BH_settings.cfg"
        target.write_bytes(override_settings(target.read_bytes(), settings))
    write_sgd2_config(path / SGD2_CONFIG)
    shutil.copyfile(bh_dll, path / "BH.dll")
    write_registry(run_id, path)
    (path / SANDBOX_MARKER).write_text(run_id + "\n")
    return run_id


def settings_key(line):
    """The key a line of BH_settings.cfg sets, or None for a comment or a line
    that sets nothing. A key ends at its first colon, after any [subkey]."""
    stripped = line.strip()
    if not stripped or stripped.startswith(b"//"):
        return None
    close = stripped.find(b"]")
    colon = stripped.find(b":", close + 1 if close >= 0 else 0)
    if colon <= 0:
        return None
    return stripped[:colon].strip().decode("latin-1")


def override_settings(text, settings):
    """BH_settings.cfg with each key in settings set to its value, in place
    where the file sets the key and at its end where it does not. Bytes in
    and out, so the rest of the file, encoding and line endings included,
    is left exactly as it was."""
    lines = text.splitlines(keepends=True)
    newline = b"\r\n" if b"\r\n" in text else b"\n"
    remaining = dict(settings)
    for i, line in enumerate(lines):
        key = settings_key(line)
        if key in settings:
            ending = line[len(line.rstrip(b"\r\n")):] or newline
            lines[i] = f"{key}: {settings[key]}".encode("latin-1") + ending
            remaining.pop(key, None)
    if lines and not lines[-1].endswith(b"\n"):
        lines[-1] += newline
    for key, value in remaining.items():
        lines.append(f"{key}: {value}".encode("latin-1") + newline)
    return b"".join(lines)


def shipped_settings_keys():
    return {key for line in SANDBOX_COPIES["BH_settings.cfg"].read_bytes().splitlines()
            if (key := settings_key(line))}


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
        raise HarnessError(f"{GAME_SOURCE / SGD2_CONFIG} does not offer {RESOLUTION}")
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
            raise HarnessError(f"{path} is not a sandbox; not deleting it")
        delete_sandbox(path)
    delete_tree(winreg.HKEY_CURRENT_USER, rf"{RUNS_KEY}\{run_id}")


def sweep():
    """Tears down every run whose launcher has gone, and returns their ids."""
    swept = []
    if RUNS.exists():
        for path in RUNS.iterdir():
            run = read_run(path)
            if run is None:
                if time.time() - path.stat().st_ctime > UNLAUNCHED_GRACE_SECONDS:
                    swept.append(path.name)
            elif not process_alive(run["launcher"]):
                swept.append(path.name)
    for run_id in swept:
        teardown(run_id)
    return swept


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
        raise HarnessError(f"{run_id} is already running")
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
        raise HarnessError(f"launch failed ({process.returncode}): {(path / 'launcher.log').read_text().strip()}")
    run = {"launcher": process.pid, "game": int(line[4:]), "args": game_args, "started": time.time()}
    (path / RUN_FILE).write_text(json.dumps(run, indent=2))
    wait_screen(run_id, ["main_menu"], READY_TIMEOUT_SECONDS)
    return run


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
        except BaseException as error:  # re-raised on the caller's thread
            result["error"] = error
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
    return report


# --- asking the game ----------------------------------------------------------
#
# BH's agent build answers on \\.\pipe\bhagent-<game pid>, one line of JSON in
# and one out per connection (BH/Modules/AgentDriver/AgentDriver.h).

ERROR_PIPE_BUSY = 231
PIPE_RETRY_SECONDS = 0.05


def request(run, body, timeout=15):
    """The driver's answer to one request, or None if its pipe never opened
    within the timeout. A failure it reports is raised as RuntimeError."""
    path = rf"\\.\pipe\bhagent-{run['game']}"
    deadline = time.time() + timeout
    while True:
        try:
            with open(path, "r+b", buffering=0) as pipe:
                pipe.write((json.dumps(body) + "\n").encode())
                data = b""
                while not data.endswith(b"\n"):
                    chunk = pipe.read(1 << 16)
                    if not chunk:
                        break
                    data += chunk
            break
        except FileNotFoundError:
            pass  # not served yet, or between two connections
        except OSError as error:
            if getattr(error, "winerror", None) != ERROR_PIPE_BUSY:
                raise
        if time.time() > deadline:
            return None
        time.sleep(PIPE_RETRY_SECONDS)
    answer = json.loads(data)
    if not answer.get("ok"):
        raise RuntimeError(answer.get("error", "the driver failed without saying why"))
    return answer


def ask(run_id, body, timeout=15):
    run = read_run(run_dir(run_id))
    if not running(run):
        raise HarnessError(f"{run_id} is not running")
    try:
        answer = request(run, body, timeout)
    except RuntimeError as error:
        raise HarnessError(str(error)) from None
    if answer is None:
        raise HarnessError(NO_DRIVER)
    return answer


NO_DRIVER = ("the game's BH answers nothing: it is not the agent build "
             "(msbuild /t:BH /p:Configuration=Release /p:BHAgent=true, giving AgentRelease\\BH.dll)")


def wait_screen(run_id, screens, timeout):
    """Returns the state once the game shows one of the screens. A dialog in
    the game's place is how the game and its mods report failing, so one ends
    the wait at once with its text."""
    run = read_run(run_dir(run_id))
    deadline = time.time() + timeout
    answered = False
    while True:
        if not running(run):
            raise HarnessError(f"the game exited (see {RUNS / run_id / 'launcher.log'})")
        dialogs = [w for w in on_desktop(run_id, lambda d: list_windows(d, run["game"]))
                   if w["visible"] and w["class"] == DIALOG_CLASS]
        if dialogs:
            texts = " / ".join(t for d in dialogs for t in [d["title"], *d.get("text", [])] if t != "OK")
            raise HarnessError(f"the game stopped at a dialog: {texts}")
        remaining = deadline - time.time()
        if remaining <= 0:
            raise HarnessError(f"the game did not reach {' or '.join(screens)} within {timeout}s")
        try:
            state = request(run, {"cmd": "state"}, min(remaining, 2))
            answered = answered or state is not None
        except RuntimeError:
            # Answered, but with no frame to take the request: a game loading
            # or being left.
            state = None
            answered = True
        if not answered and time.time() - run["started"] > DRIVER_GRACE_SECONDS:
            raise HarnessError(NO_DRIVER)
        if state and state["screen"] in screens:
            return state
        time.sleep(0.2)


# BH is loaded, and starts answering, a few seconds into a launch.
DRIVER_GRACE_SECONDS = 30


def state(run_id):
    return ask(run_id, {"cmd": "state"})


def drawlog(run_id):
    """Everything BH drew in one whole frame."""
    return ask(run_id, {"cmd": "drawlog"})


def leave(run_id, timeout):
    """Leaves the game as Save and Exit does, and returns the screen once the
    menus are back: the main menu, in single player."""
    if state(run_id)["screen"] != "game":
        raise HarnessError("not in a game")
    ask(run_id, {"cmd": "exit"})
    return wait_screen(run_id, ["main_menu", "character_list"], timeout)["screen"]


# Where enter clicks, in the menus' 800x600. The class is aimed at the
# Sorceress herself, since the class screen's controls do not include the
# heroes.
SINGLE_PLAYER = (400, 306)
SORCERESS = (615, 300)
OK_BUTTON = (690, 555)
# The heroes walk forward when picked, and the screen takes no name until they
# have.
CLASS_PICK_SECONDS = 2


def enter(run_id, name, timeout):
    """From the main menu or the character list, into a single player game in
    Normal: with the run's character if it has one, else with a new Sorceress
    called name. Returns the game's state."""
    screen = state(run_id)["screen"]
    if screen == "main_menu":
        send_input(run_id, [f"click:{SINGLE_PLAYER[0]},{SINGLE_PLAYER[1]}"])
        screen = wait_screen(run_id, ["new_character", "character_list"], timeout)["screen"]
    if screen == "new_character":
        if not name.isalpha():
            raise HarnessError(f"a character name is letters only, not {name!r}")
        send_input(run_id, [f"click:{SORCERESS[0]},{SORCERESS[1]}", f"wait:{CLASS_PICK_SECONDS}",
                            f"text:{name}", f"click:{OK_BUTTON[0]},{OK_BUTTON[1]}"])
    elif screen == "character_list":
        send_input(run_id, [f"click:{OK_BUTTON[0]},{OK_BUTTON[1]}"])
    else:
        raise HarnessError(f"enter starts from main_menu or character_list, not {screen}")
    return wait_screen(run_id, ["game"], timeout)


WM_CLOSE = 0x0010


def close(run_id, timeout):
    """Quits the game the way a player does, leaving any game first, so it
    saves and writes its options as it would for them."""
    run = read_run(run_dir(run_id))
    if not running(run):
        raise HarnessError(f"{run_id} is not running")
    if state(run_id)["screen"] == "game":
        leave(run_id, timeout)

    def post_close(desktop):
        windows = [w for w in list_windows(desktop, run["game"]) if w["visible"]]
        for window in windows:
            user32.PostMessageW(window["hwnd"], WM_CLOSE, 0, 0)
        return len(windows)

    if not on_desktop(run_id, post_close):
        raise HarnessError("the game has no window to close")
    deadline = time.time() + timeout
    while running(run):
        if time.time() > deadline:
            raise HarnessError(f"the game did not exit within {timeout}s")
        time.sleep(0.2)


GAME_WINDOW_CLASS = "Diablo II"
DIALOG_CLASS = "#32770"


def game_window(desktop, pid):
    for window in list_windows(desktop, pid):
        if window["class"] == GAME_WINDOW_CLASS and window["visible"]:
            return window
    raise HarnessError("the game has no window")


# --- driving ------------------------------------------------------------------
#
# Input is posted to the game window. D2 reads the cursor from the messages
# themselves, so a click lands where it is aimed without the real cursor moving.
# Posted keys do not change what GetKeyState reports, so anything the game asks
# of the keyboard directly, rather than through its messages, does not see them.

WM_KEYDOWN, WM_KEYUP, WM_CHAR, WM_SYSKEYDOWN, WM_SYSKEYUP = 0x100, 0x101, 0x102, 0x104, 0x105
WM_MOUSEMOVE, WM_LBUTTONDOWN, WM_LBUTTONUP, WM_RBUTTONDOWN, WM_RBUTTONUP = 0x200, 0x201, 0x202, 0x204, 0x205
MK_LBUTTON, MK_RBUTTON, MK_SHIFT, MK_CONTROL = 0x1, 0x2, 0x4, 0x8
VK_SHIFT, VK_CONTROL, VK_MENU = 0x10, 0x11, 0x12
MAPVK_VK_TO_VSC = 0
user32.MapVirtualKeyW.argtypes = [wt.UINT, wt.UINT]
user32.VkKeyScanW.argtypes = [wt.WCHAR]
user32.VkKeyScanW.restype = ctypes.c_short

KEYS = {
    "backspace": 0x08, "tab": 0x09, "enter": 0x0D, "shift": VK_SHIFT, "ctrl": VK_CONTROL, "alt": VK_MENU,
    "pause": 0x13, "esc": 0x1B, "space": 0x20, "pageup": 0x21, "pagedown": 0x22, "end": 0x23,
    "home": 0x24, "left": 0x25, "up": 0x26, "right": 0x27, "down": 0x28, "insert": 0x2D, "delete": 0x2E,
    **{f"f{n}": 0x6F + n for n in range(1, 13)},
    **{f"numpad{n}": 0x60 + n for n in range(10)},
    "multiply": 0x6A, "add": 0x6B, "subtract": 0x6D, "decimal": 0x6E, "divide": 0x6F,
}
EXTENDED_KEYS = {0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x2D, 0x2E}

ACTIONS_HELP = """actions, run in order:
  move:X,Y       move the cursor to client coordinates X,Y
  click:X,Y      left click at X,Y            rclick:X,Y   right click
  key:NAME       press and release a key      down:NAME / up:NAME   hold, release
  text:STRING    type STRING as a keyboard would, into whatever has focus
  wait:SECONDS   pause, for the game to react
  shot:FILE      screenshot to FILE.png, with any held keys still held
NAME is a letter or digit, F1-F12, NUMPAD0-NUMPAD9, a virtual-key code as 0xNN,
or one of: """ + ", ".join(sorted(k for k in KEYS if not k.startswith(("f", "numpad"))))


def virtual_key(name):
    lowered = name.lower()
    if lowered in KEYS:
        return KEYS[lowered]
    if len(name) == 1 and name.isalnum():
        return ord(name.upper())
    if lowered.startswith("0x"):
        try:
            code = int(lowered, 16)
        except ValueError:
            code = 0
        if 0 < code < 0xFF:
            return code
    raise HarnessError(f"unknown key {name!r}")


def mouse_param(x, y):
    return (y << 16) | (x & 0xFFFF)


def key_param(vk, releasing, alt_held):
    scan = user32.MapVirtualKeyW(vk, MAPVK_VK_TO_VSC)
    param = 1 | (scan << 16)
    if vk in EXTENDED_KEYS:
        param |= 1 << 24
    if alt_held:
        param |= 1 << 29
    if releasing:
        param |= (1 << 30) | (1 << 31)
    return param


class Input:
    """Posts one action at a time to the game window, keeping the modifier keys
    held across actions so that a click while Shift is down says so."""

    def __init__(self, window):
        self.hwnd = window["hwnd"]
        self.client = window["client"]
        self.held = set()

    def post(self, message, wparam, lparam):
        if not user32.PostMessageW(self.hwnd, message, wparam, lparam):
            raise ctypes.WinError(ctypes.get_last_error())

    def modifiers(self):
        return (MK_SHIFT if VK_SHIFT in self.held else 0) | (MK_CONTROL if VK_CONTROL in self.held else 0)

    def key(self, vk, releasing):
        # Windows sends keys pressed with Alt down, and Alt itself, as system
        # keys, and marks each with whether Alt is down once it has happened.
        alt_after = (vk != VK_MENU and VK_MENU in self.held) or (vk == VK_MENU and not releasing)
        system = vk == VK_MENU or VK_MENU in self.held
        if releasing:
            message = WM_SYSKEYUP if system else WM_KEYUP
        else:
            message = WM_SYSKEYDOWN if system else WM_KEYDOWN
        self.post(message, vk, key_param(vk, releasing, alt_after))
        (self.held.discard if releasing else self.held.add)(vk)

    def type(self, char):
        """As a keyboard types it: the key, with Shift held for a capital. The
        game's message loop turns the key into the character, as it does for a
        keyboard (the agent build makes that see Shift), and BH's boxes read the
        key itself. A character no key types is posted as it is."""
        scan = user32.VkKeyScanW(char)
        if scan == -1:
            self.post(WM_CHAR, ord(char), 1)
        else:
            vk, shift = scan & 0xFF, bool(scan & 0x100) and VK_SHIFT not in self.held
            if shift:
                self.key(VK_SHIFT, False)
            self.key(vk, False)
            self.key(vk, True)
            if shift:
                self.key(VK_SHIFT, True)
        time.sleep(0.03)

    def click(self, x, y, down, up, button):
        self.post(WM_MOUSEMOVE, self.modifiers(), mouse_param(x, y))
        time.sleep(0.08)
        self.post(down, self.modifiers() | button, mouse_param(x, y))
        time.sleep(0.08)
        self.post(up, self.modifiers(), mouse_param(x, y))

    def run(self, action):
        """One action, as validate_action has already accepted it."""
        kind, _, arg = action.partition(":")
        if kind in ("move", "click", "rclick"):
            x, y = (int(v) for v in arg.split(","))
            if kind == "move":
                self.post(WM_MOUSEMOVE, self.modifiers(), mouse_param(x, y))
            elif kind == "click":
                self.click(x, y, WM_LBUTTONDOWN, WM_LBUTTONUP, MK_LBUTTON)
            else:
                self.click(x, y, WM_RBUTTONDOWN, WM_RBUTTONUP, MK_RBUTTON)
        elif kind == "key":
            vk = virtual_key(arg)
            self.key(vk, False)
            time.sleep(0.05)
            self.key(vk, True)
        elif kind in ("down", "up"):
            self.key(virtual_key(arg), kind == "up")
        elif kind == "text":
            for char in arg:
                self.type(char)
        elif kind == "wait":
            time.sleep(float(arg))
        elif kind == "shot":
            width, height = self.client
            write_png(arg, width, height, capture(self.hwnd, 0, 0, width, height))


# A frame at the game's 25 fps, so each action is seen before the next.
FRAME_SECONDS = 0.04


def send_input(run_id, actions, then=None):
    """Runs the actions in order, then then() if given, and returns what it
    returns. Keys the actions hold down stay down through then(), and are
    released at the end."""
    for action in actions:
        validate_action(action)
    run = read_run(run_dir(run_id))
    if not running(run):
        raise HarnessError(f"{run_id} is not running")

    def drive(desktop):
        source = Input(game_window(desktop, run["game"]))
        try:
            for action in actions:
                source.run(action)
                time.sleep(FRAME_SECONDS)
            return then() if then else None
        finally:
            for vk in list(source.held):
                source.key(vk, True)

    return on_desktop(run_id, drive)


def validate_action(action):
    """Raises for an action Input.run would refuse, before any is sent."""
    kind, _, arg = action.partition(":")
    if kind in ("move", "click", "rclick"):
        try:
            x, y = (int(v) for v in arg.split(","))
        except ValueError:
            raise HarnessError(f"{action!r}: expected {kind}:X,Y") from None
    elif kind in ("key", "down", "up"):
        virtual_key(arg)
    elif kind == "wait":
        try:
            float(arg)
        except ValueError:
            raise HarnessError(f"{action!r}: expected wait:SECONDS") from None
    elif kind not in ("text", "shot") or not arg:
        raise HarnessError(f"unknown action {action!r}\n{ACTIONS_HELP}")


# --- seeing -------------------------------------------------------------------

SRCCOPY = 0x00CC0020
DIB_RGB_COLORS = 0
gdi32 = ctypes.WinDLL("gdi32", use_last_error=True)
user32.GetDC.restype = wt.HANDLE
user32.GetDC.argtypes = [wt.HWND]
user32.ReleaseDC.argtypes = [wt.HWND, wt.HANDLE]
gdi32.CreateCompatibleDC.restype = wt.HANDLE
gdi32.CreateCompatibleDC.argtypes = [wt.HANDLE]
gdi32.CreateCompatibleBitmap.restype = wt.HANDLE
gdi32.CreateCompatibleBitmap.argtypes = [wt.HANDLE, ctypes.c_int, ctypes.c_int]
gdi32.SelectObject.restype = wt.HANDLE
gdi32.SelectObject.argtypes = [wt.HANDLE, wt.HANDLE]
gdi32.BitBlt.argtypes = [wt.HANDLE, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int,
                         wt.HANDLE, ctypes.c_int, ctypes.c_int, wt.DWORD]
gdi32.GetDIBits.argtypes = [wt.HANDLE, wt.HANDLE, wt.UINT, wt.UINT, ctypes.c_void_p, ctypes.c_void_p, wt.UINT]
gdi32.DeleteObject.argtypes = [wt.HANDLE]
gdi32.DeleteDC.argtypes = [wt.HANDLE]


class BITMAPINFOHEADER(ctypes.Structure):
    _fields_ = [("biSize", wt.DWORD), ("biWidth", wt.LONG), ("biHeight", wt.LONG), ("biPlanes", wt.WORD),
                ("biBitCount", wt.WORD), ("biCompression", wt.DWORD), ("biSizeImage", wt.DWORD),
                ("biXPelsPerMeter", wt.LONG), ("biYPelsPerMeter", wt.LONG), ("biClrUsed", wt.DWORD),
                ("biClrImportant", wt.DWORD)]


def capture(hwnd, x, y, width, height):
    """The window's client area as drawn, read back as top-down BGRA."""
    source = user32.GetDC(hwnd)
    memory = gdi32.CreateCompatibleDC(source)
    bitmap = gdi32.CreateCompatibleBitmap(source, width, height)
    previous = gdi32.SelectObject(memory, bitmap)
    try:
        if not gdi32.BitBlt(memory, 0, 0, width, height, source, x, y, SRCCOPY):
            raise ctypes.WinError(ctypes.get_last_error())
        header = BITMAPINFOHEADER(ctypes.sizeof(BITMAPINFOHEADER), width, -height, 1, 32, 0, 0, 0, 0, 0, 0)
        pixels = ctypes.create_string_buffer(width * height * 4)
        if gdi32.GetDIBits(memory, bitmap, 0, height, pixels, ctypes.byref(header), DIB_RGB_COLORS) != height:
            raise ctypes.WinError(ctypes.get_last_error())
        return pixels.raw
    finally:
        gdi32.SelectObject(memory, previous)
        gdi32.DeleteObject(bitmap)
        gdi32.DeleteDC(memory)
        user32.ReleaseDC(hwnd, source)


def write_png(path, width, height, bgra):
    rgb = bytearray(width * height * 3)
    rgb[0::3], rgb[1::3], rgb[2::3] = bgra[2::4], bgra[1::4], bgra[0::4]
    stride = width * 3
    rows = b"".join(b"\0" + rgb[y * stride:(y + 1) * stride] for y in range(height))

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))

    Path(path).parent.mkdir(parents=True, exist_ok=True)
    Path(path).write_bytes(b"\x89PNG\r\n\x1a\n"
                           + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
                           + chunk(b"IDAT", zlib.compress(bytes(rows), 6)) + chunk(b"IEND", b""))


def grab(run_id, crop=None):
    """The game window's client area, or the crop X,Y,W,H of it, as
    (width, height, top-down BGRA, the window's [width, height])."""
    run = read_run(run_dir(run_id))
    if not running(run):
        raise HarnessError(f"{run_id} is not running")

    def read(desktop):
        window = game_window(desktop, run["game"])
        width, height = window["client"]
        x, y, w, h = crop or (0, 0, width, height)
        if x < 0 or y < 0 or w <= 0 or h <= 0 or x + w > width or y + h > height:
            raise HarnessError(f"crop {x},{y},{w},{h} is outside the {width}x{height} window")
        return w, h, capture(window["hwnd"], x, y, w, h), [width, height]

    return on_desktop(run_id, read)


def shot(run_id, out, crop=None):
    w, h, pixels, client = grab(run_id, crop)
    write_png(out, w, h, pixels)
    return {"path": str(Path(out).resolve()), "size": [w, h], "window": client}


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


def setting(text):
    key, equals, value = text.partition("=")
    if not equals or not key.strip():
        raise argparse.ArgumentTypeError(f"expected KEY=VALUE, as in \"Show Settings=True, VK_NUMPAD8\", not {text!r}")
    return key.strip(), value.strip()


def emit(value):
    print(value if isinstance(value, str) else json.dumps(value, indent=2, ensure_ascii=False))


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("base")
    create_cmd = commands.add_parser("create")
    create_cmd.add_argument("--bh", type=Path, default=REPO / "AgentRelease" / "BH.dll")
    create_cmd.add_argument("--setting", type=setting, action="append", default=[], metavar="KEY=VALUE",
                            help="a BH_settings.cfg line to write in place of the shipped one; repeatable")
    launch_cmd = commands.add_parser("launch")
    launch_cmd.add_argument("id")
    launch_cmd.add_argument("--lifetime", type=float, default=LIFETIME_SECONDS,
                            help="seconds before the launcher ends the game regardless")
    enter_cmd = commands.add_parser("enter")
    enter_cmd.add_argument("id")
    enter_cmd.add_argument("--name", default="Agent", help="for a new character: letters only")
    enter_cmd.add_argument("--timeout", type=float, default=60)
    input_cmd = commands.add_parser("input", epilog=ACTIONS_HELP, formatter_class=argparse.RawDescriptionHelpFormatter)
    input_cmd.add_argument("id")
    input_cmd.add_argument("actions", nargs="+")
    shot_cmd = commands.add_parser("shot")
    shot_cmd.add_argument("id")
    shot_cmd.add_argument("file", type=Path)
    shot_cmd.add_argument("--crop", type=lambda s: tuple(int(v) for v in s.split(",")), metavar="X,Y,W,H")
    for name in ("status", "teardown", "state"):
        commands.add_parser(name).add_argument("id")
    wait_cmd = commands.add_parser("wait")
    wait_cmd.add_argument("id")
    wait_cmd.add_argument("screens", nargs="+", metavar="screen",
                          help="game, loading, main_menu, character_list, new_character, difficulty or menu")
    wait_cmd.add_argument("--timeout", type=float, default=60)
    drawlog_cmd = commands.add_parser("drawlog")
    drawlog_cmd.add_argument("id")
    drawlog_cmd.add_argument("--out", type=Path)
    leave_cmd = commands.add_parser("leave")
    leave_cmd.add_argument("id")
    leave_cmd.add_argument("--timeout", type=float, default=60)
    close_cmd = commands.add_parser("close")
    close_cmd.add_argument("id")
    close_cmd.add_argument("--timeout", type=float, default=30)
    scenario_cmd = commands.add_parser("scenario")
    scenario_cmd.add_argument("file", type=Path)
    scenario_cmd.add_argument("--out", type=Path, help="the report's folder (default AgentHarness\\reports\\<time>-<name>)")
    scenario_cmd.add_argument("--bh", type=Path, default=REPO / "AgentRelease" / "BH.dll")
    scenario_cmd.add_argument("--keep", action="store_true", help="leave the run up afterwards, for a closer look")
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

    try:
        if args.command == "base":
            build_base()
        elif args.command == "create":
            emit(create(args.bh, dict(args.setting)))
            for key, _ in args.setting:
                if key not in shipped_settings_keys():
                    print(f"note: {key!r} is not in the shipped BH_settings.cfg; it was added, "
                          "and does nothing unless BH reads it", file=sys.stderr)
        elif args.command == "launch":
            emit(launch(args.id, game_args or ["-w", "-ns"], args.lifetime))
        elif args.command == "enter":
            emit(enter(args.id, args.name, args.timeout)["game"])
        elif args.command == "input":
            send_input(args.id, args.actions)
        elif args.command == "shot":
            emit(shot(args.id, args.file, args.crop))
        elif args.command == "status":
            emit(status(args.id))
        elif args.command == "state":
            emit(state(args.id))
        elif args.command == "wait":
            emit(wait_screen(args.id, args.screens, args.timeout)["screen"])
        elif args.command == "drawlog":
            frame = drawlog(args.id)
            if args.out:
                args.out.write_text(json.dumps(frame, indent=1, ensure_ascii=False), encoding="utf-8")
                emit({"path": str(args.out.resolve()), "items": len(frame["items"])})
            else:
                emit(frame)
        elif args.command == "leave":
            emit(leave(args.id, args.timeout))
        elif args.command == "close":
            close(args.id, args.timeout)
            emit(f"{args.id} exited")
        elif args.command == "scenario":
            import scenario
            report = scenario.run_scenario(args.file, args.out, args.bh, args.keep)
            emit({"result": report["result"], "report": str(Path(report["out"]) / "report.md"),
                  **({"error": report["error"]} if "error" in report else {}),
                  **({"run": report["run"]} if args.keep and "run" in report else {})})
            sys.exit({"passed": 0, "failed": 1}.get(report["result"], 2))
        elif args.command == "teardown":
            teardown(args.id)
            emit(f"torn down {args.id}")
        elif args.command == "sweep":
            for run_id in sweep():
                emit(f"torn down {run_id}")
        elif args.command == "snapshot":
            snapshot(args.file)
        elif args.command == "compare":
            sys.exit(1 if compare(args.a, args.b) else 0)
    except HarnessError as error:
        print(error, file=sys.stderr)
        # A scenario's 1 means a check failed, so one that could not run is 2.
        sys.exit(2 if args.command == "scenario" else 1)


if __name__ == "__main__":
    # Through the module scenario.py imports, so that there is one HarnessError
    # class and not a second one in __main__ that its errors do not belong to.
    import bhctl
    bhctl.main()
