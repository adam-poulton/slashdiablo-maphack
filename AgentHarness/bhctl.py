"""Runs Diablo II for an agent, in a sandbox that cannot reach the player's game.

    python AgentHarness/bhctl.py doctor               # what this machine still lacks
    python AgentHarness/bhctl.py base                 # (re)build the shared base
    python AgentHarness/bhctl.py create [--bh DLL]    # new sandbox, prints its id
    python AgentHarness/bhctl.py launch ID [-- ARGS]  # start its game, hidden
    python AgentHarness/bhctl.py enter ID             # into a game in Normal
    python AgentHarness/bhctl.py input ID ACTION...   # click, type, press keys
    python AgentHarness/bhctl.py drop ID --capture F  # an item lands, as recorded
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

What differs from one machine to the next is read from the environment:

    BHAGENT_GAME_SOURCE  the Diablo II 1.13c install the base is copied from,
                         read and never written (default: the install the
                         registry names)
    BHAGENT_ROOT         where the base and the runs are kept (default
                         ~\\.bhagent)
    BHAGENT_PLAYER_DIRS  more folders for snapshot to watch, separated by ;

Layout, under the root:

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

import captures
from viewport import Viewport

REPO = Path(__file__).resolve().parent.parent
HARNESS_BIN = REPO / "AgentHarness" / "bin" / "Release"
AGENT_BH = REPO / "AgentRelease" / "BH.dll"
ROOT = Path(os.environ.get("BHAGENT_ROOT") or Path.home() / ".bhagent")
BASE = ROOT / "base"
RUNS = ROOT / "runs"
RUNS_KEY = r"Software\BHAgent\runs"
SANDBOX_MARKER = "BHAgent.sandbox"
SANDBOX_FILE = "sandbox.json"
RUN_FILE = "run.json"
LIFETIME_SECONDS = 3600
READY_TIMEOUT_SECONDS = 60
# A sandbox nobody has launched yet is left alone by sweep for this long, so
# that one agent's sweep cannot take another's sandbox between create and launch.
UNLAUNCHED_GRACE_SECONDS = 600

GAME_SOURCE_VARIABLE = "BHAGENT_GAME_SOURCE"
GAME_KEY = r"Software\Blizzard Entertainment\Diablo II"


class HarnessError(Exception):
    """A run cannot go on. The message is for the agent, and says why."""


def registered_install():
    """The install the game's own registry key names, or None. The game keeps
    the key under HKEY_CURRENT_USER, and its installer under the 32-bit
    HKEY_LOCAL_MACHINE."""
    for hive, access in ((winreg.HKEY_CURRENT_USER, winreg.KEY_READ),
                         (winreg.HKEY_LOCAL_MACHINE, winreg.KEY_READ | winreg.KEY_WOW64_32KEY)):
        try:
            with winreg.OpenKey(hive, GAME_KEY, 0, access) as key:
                path, kind = winreg.QueryValueEx(key, "InstallPath")
        except OSError:
            continue
        if kind == winreg.REG_SZ and path:
            return Path(path)
    return None


def game_source():
    """Where the base is copied from, read and never written: the install
    BHAGENT_GAME_SOURCE names, else the one the registry does."""
    named = os.environ.get(GAME_SOURCE_VARIABLE)
    source = Path(named) if named else registered_install()
    if source is None:
        raise HarnessError(f"no Diablo II install is known: set {GAME_SOURCE_VARIABLE} to a 1.13c install "
                           "with the Slash Diablo mods (D2Template, SGD2FreeRes)")
    if not (source / "Game.exe").is_file():
        raise HarnessError(f"{source} has no Game.exe: set {GAME_SOURCE_VARIABLE} to a 1.13c install "
                           "with the Slash Diablo mods (D2Template, SGD2FreeRes)")
    return source


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
    "SGD2FreeRes.dll", "SGD2FreeRes.mpq",
    # The Glide wrapper, which is D2GL in a Slash Diablo install, and the
    # renderer only of a run that asks for it. Every run needs the file:
    # SGD2FreeRes loads it whatever the video mode, to ask which Glide wrapper
    # it is, and stops the game with a dialog when it is missing.
    "glide3x.dll",
    "d2char.mpq", "d2data.mpq", "d2exp.mpq", "d2music.mpq", "d2sfx.mpq",
    "d2speech.mpq", "d2video.mpq", "d2xmusic.mpq", "d2xtalk.mpq", "d2xvideo.mpq",
    "Patch_D2.mpq",
]

# Mods D2Template loads when they are there, which not every Slash Diablo
# install has: in the base, and so in every run, when the source install does.
OPTIONAL_FILES = ["SGD2FreeDisplayFix.dll"]

# What D2GL needs beside its DLL: in the base when the source install has it,
# and linked into a run that renders with D2GL.
D2GL_FILES = ["d2gl.mpq"]

# Copied, not linked, because something in the game writes them.
SANDBOX_COPIES = {
    "BH.cfg": REPO / "Packaging" / "BH.cfg",
    "BH_settings.cfg": REPO / "Packaging" / "BH_settings.cfg",
    "buffs.mpq": REPO / "Packaging" / "buffs.mpq",
}

# The in-game resolution a run starts at: the one Slash Diablo players use.
RESOLUTION = "1344x700"
SGD2_CONFIG = "SGD2FreeResolution.json"

# What draws a run's game. The game's own GDI renderer is the same on every
# machine, pixel for pixel. D2GL is what Slash Diablo players see: the game
# through OpenGL, scaled to its window, with D2GL's own text, cursor and map.
RENDERERS = ("gdi", "d2gl")
LAUNCH_ARGS = {"gdi": ["-w", "-ns"], "d2gl": ["-3dfx", "-ns"]}

D2GL_CONFIG = "d2gl.ini"
# The smallest window D2GL makes, whatever its config asks for.
D2GL_MIN_WINDOW = (800, 600)
# What a sandbox needs of D2GL, which a run cannot ask otherwise. Everything
# else is D2GL's own default unless the run sets it. The window is a plain one
# at a known place. The cursor is left free, because the one D2GL would confine
# to its window is the player's. Nothing waits on a monitor's refresh, which a
# desktop nobody is looking at does not have, and the frame rate is capped
# whether or not the window counts as focused, so a run costs the player's own
# game little.
D2GL_SANDBOX_SETTINGS = {
    "Screen.fullscreen": "false",
    "Screen.borderless_window": "true",
    "Screen.centered_window": "false",
    "Screen.window_posx": "0",
    "Screen.window_posy": "0",
    "Screen.unlock_cursor": "true",
    "Screen.auto_minimize": "false",
    "Screen.vsync": "false",
    "Screen.foreground_fps": "true",
    "Screen.foreground_fps_value": "30",
    "Screen.background_fps": "true",
    "Screen.background_fps_value": "30",
    "Feature.skip_intro": "true",
}
D2GL_WINDOW_SETTINGS = ("Screen.window_width", "Screen.window_height")

# The player's own state, which no run may change. Read-only to this script.
PLAYER_KEYS = [
    r"Software\Blizzard Entertainment",
    r"Software\Classes\VirtualStore\MACHINE\SOFTWARE\WOW6432Node\Blizzard Entertainment",
    r"Software\Classes\VirtualStore\MACHINE\SOFTWARE\Blizzard Entertainment",
]
PLAYER_DIRS_VARIABLE = "BHAGENT_PLAYER_DIRS"


def player_dirs():
    """The folders the player's game lives in: the install the registry names,
    the one the base is copied from, their saves, where Windows redirects the
    writes of a game installed under Program Files, and any others
    BHAGENT_PLAYER_DIRS lists."""
    dirs = [registered_install(), Path.home() / "Saved Games" / "Diablo II",
            Path(os.environ["LOCALAPPDATA"]) / "VirtualStore"]
    try:
        dirs.append(game_source())
    except HarnessError:
        pass
    dirs += [Path(p) for p in os.environ.get(PLAYER_DIRS_VARIABLE, "").split(os.pathsep) if p]
    unique = {}
    for path in dirs:
        if path is not None:
            unique.setdefault(os.path.normcase(os.path.normpath(path)), Path(path))
    return list(unique.values())


kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
user32 = ctypes.WinDLL("user32", use_last_error=True)


# --- base --------------------------------------------------------------------

def build_base():
    """Copies the game's files into base once, and again only where the source
    has changed. Every file is marked read-only, so a sandbox that tries to
    write through one of its links fails instead of changing every run."""
    source_dir = game_source()
    missing = [name for name in BASE_FILES if not (source_dir / name).is_file()]
    if missing:
        raise HarnessError(f"{source_dir} is not a Slash Diablo 1.13c install: it lacks {', '.join(missing)}. "
                           f"Set {GAME_SOURCE_VARIABLE} to one that has them")
    wanted = BASE_FILES + [name for name in OPTIONAL_FILES + D2GL_FILES if (source_dir / name).is_file()]
    BASE.mkdir(parents=True, exist_ok=True)
    for name in wanted:
        source = source_dir / name
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
    stray = {p.name for p in BASE.iterdir()} - set(wanted)
    for name in sorted(stray):
        print(f"removing {name}, no longer part of the base")
        clear_readonly(BASE / name)
        (BASE / name).unlink()
    if not set(D2GL_FILES) <= set(wanted):
        print(f"note: {source_dir} has no D2GL ({', '.join(D2GL_FILES)}), so runs render with GDI only")
    print(f"base ready: {BASE}, from {source_dir}")


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


D2S_SIGNATURE = b"\x55\xaa\x55\xaa"
D2S_VERSION = 96  # every version from 1.10 on, 1.13c's included


def character_name(save):
    """The name a character's save file holds, which the game also needs it to
    be called, or why it is not a 1.13c character save."""
    try:
        data = Path(save).read_bytes()
    except OSError as error:
        raise HarnessError(f"cannot read the character {save}: {error.strerror}") from None
    if len(data) < 36 or data[:4] != D2S_SIGNATURE:
        raise HarnessError(f"{save} is not a character save (.d2s)")
    version, size = struct.unpack_from("<II", data, 4)
    if version != D2S_VERSION:
        raise HarnessError(f"{save} is a save from another version of the game ({version}, not {D2S_VERSION})")
    if size != len(data):
        raise HarnessError(f"{save} is cut short: it says {size} bytes and holds {len(data)}")
    name = data[20:36].split(b"\0")[0].decode("latin-1")
    if not 2 <= len(name) <= 15:
        raise HarnessError(f"{save} holds no character name")
    return name


def create(bh_dll, settings=None, character=None, renderer="gdi", window=None, d2gl=None):
    """A new sandbox, with BH's shipped settings except for those given, a
    dict of BH_settings.cfg keys to the values to write for them, and with the
    character in the save file given, if one is. The run plays a copy: the
    file itself is never written.

    The renderer is the game's own GDI unless it is "d2gl", which takes a
    window size, the in-game resolution unless given, and a dict of D2GL's
    own settings as "Section.key" to the values to write for them."""
    if renderer not in RENDERERS:
        raise HarnessError(f"a renderer is one of {', '.join(RENDERERS)}, not {renderer!r}")
    if renderer != "d2gl" and (window or d2gl):
        raise HarnessError("a window size and D2GL's settings are for a run rendered by D2GL (--renderer d2gl)")
    for name in BASE_FILES:
        if not (BASE / name).exists():
            raise HarnessError("the base is missing or incomplete: run `bhctl.py base` first")
    links = BASE_FILES + [name for name in OPTIONAL_FILES if (BASE / name).exists()]
    if renderer == "d2gl":
        for name in D2GL_FILES:
            if not (BASE / name).exists():
                raise HarnessError(f"the base has no D2GL ({name}): run `bhctl.py base` with {GAME_SOURCE_VARIABLE} "
                                   "naming an install that has D2GL")
        links += D2GL_FILES
    if not bh_dll.exists():
        raise HarnessError(f"no BH.dll at {bh_dll}")
    character_file = character and f"{character_name(character)}.d2s"
    d2gl_config = d2gl_ini(window or default_window(), d2gl or {}) if renderer == "d2gl" else None

    run_id = time.strftime("%Y%m%d-%H%M%S-") + secrets.token_hex(2)
    path = RUNS / run_id
    (path / "Save").mkdir(parents=True)
    if character_file:
        shutil.copyfile(character, path / "Save" / character_file)
    for name in links:
        os.link(BASE / name, path / name)
    for name, source in SANDBOX_COPIES.items():
        shutil.copyfile(source, path / name)
    if settings:
        target = path / "BH_settings.cfg"
        target.write_bytes(override_settings(target.read_bytes(), settings))
    write_sgd2_config(path / SGD2_CONFIG)
    if d2gl_config:
        (path / D2GL_CONFIG).write_text(d2gl_config)
    shutil.copyfile(bh_dll, path / "BH.dll")
    write_registry(run_id, path)
    (path / SANDBOX_FILE).write_text(json.dumps({"renderer": renderer}) + "\n")
    (path / SANDBOX_MARKER).write_text(run_id + "\n")
    return run_id


def renderer_of(run_id):
    """What draws the run's game. A sandbox that does not say is one made
    before there was a choice, which GDI drew."""
    try:
        return json.loads((run_dir(run_id) / SANDBOX_FILE).read_text())["renderer"]
    except FileNotFoundError:
        return "gdi"


def default_window():
    width, height = RESOLUTION.split("x")
    return int(width), int(height)


def d2gl_ini(window, settings):
    """A run's d2gl.ini: what a sandbox needs of D2GL, the window's size, and
    the settings given, as "Section.key" to value. D2GL fills in every setting
    the file leaves out with its own default, and rewrites the file whole."""
    width, height = window
    if width < D2GL_MIN_WINDOW[0] or height < D2GL_MIN_WINDOW[1]:
        raise HarnessError(f"D2GL's window is at least {D2GL_MIN_WINDOW[0]}x{D2GL_MIN_WINDOW[1]}, not {width}x{height}")
    values = dict(D2GL_SANDBOX_SETTINGS)
    values.update(zip(D2GL_WINDOW_SETTINGS, (str(width), str(height))))
    for name, value in settings.items():
        section, dot, key = name.partition(".")
        if not dot or not section or not key or any(c in name + value for c in "[]=\r\n"):
            raise HarnessError(f"a D2GL setting is Section.key, as in Feature.hd_text, not {name!r}")
        if name in values:
            raise HarnessError(f"{name} is the sandbox's to set, not a run's")
        values[name] = value
    sections = {}
    for name, value in values.items():
        section, _, key = name.partition(".")
        sections.setdefault(section, []).append(f"{key}={value}")
    return "".join(f"[{section}]\n" + "\n".join(lines) + "\n\n" for section, lines in sections.items())


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
    template = game_source() / SGD2_CONFIG
    try:
        config = json.loads(template.read_text())
    except FileNotFoundError:
        raise HarnessError(f"{template} is missing: the game source is not a Slash Diablo install") from None
    settings = config["SlashGaming Diablo II Free Resolution"]
    resolutions = settings["Ingame Resolutions"]
    if RESOLUTION not in resolutions:
        raise HarnessError(f"{template} does not offer {RESOLUTION}")
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


def launch(run_id, game_args=None, lifetime=LIFETIME_SECONDS):
    """Starts the run's game, with its renderer's arguments unless others are
    given, and returns once it is at the main menu."""
    path = run_dir(run_id)
    if read_run(path) and process_alive(read_run(path)["launcher"]):
        raise HarnessError(f"{run_id} is already running")
    renderer = renderer_of(run_id)
    game_args = game_args or LAUNCH_ARGS[renderer]
    # Without -w the game picks another renderer, and a screenshot of a run
    # taken for GDI's would be read from a window GDI did not draw (capture).
    if renderer == "gdi" and "-w" not in game_args:
        raise HarnessError("a run GDI draws is windowed (-w): for the game through D2GL, "
                           "create the sandbox with --renderer d2gl")
    launcher = HARNESS_BIN / "AgentLaunch.exe"
    if not launcher.exists():
        raise HarnessError(f"no launcher at {launcher}: build it (msbuild /t:AgentLaunch /p:Configuration=Release)")
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


user32.GetWindowDpiAwarenessContext.restype = wt.HANDLE
user32.GetWindowDpiAwarenessContext.argtypes = [wt.HWND]
user32.GetAwarenessFromDpiAwarenessContext.argtypes = [wt.HANDLE]
user32.SetThreadDpiAwarenessContext.restype = wt.HANDLE
user32.SetThreadDpiAwarenessContext.argtypes = [wt.HANDLE]
# DPI_AWARENESS, as GetAwarenessFromDpiAwarenessContext answers it, to the
# DPI_AWARENESS_CONTEXT that asks for it: unaware, system aware, and per
# monitor aware (version 2).
DPI_CONTEXTS = {0: -1, 1: -2, 2: -4}


def see_as(hwnd):
    """Gives this thread the window's own DPI awareness. Windows scales a
    window's size, and the position in a mouse message posted to it, between
    a caller and a window that differ: the game alone is unaware of DPI, and
    D2GL makes it aware. Seen as the window sees itself, a pixel is a pixel."""
    awareness = user32.GetAwarenessFromDpiAwarenessContext(user32.GetWindowDpiAwarenessContext(hwnd))
    if awareness in DPI_CONTEXTS:
        user32.SetThreadDpiAwarenessContext(DPI_CONTEXTS[awareness])


def list_windows(desktop, pid):
    """The game's top-level windows. Windows owns a few of its own on every
    desktop, so the game's are picked out by process. A dialog carries its
    text, which is usually the only account of why the game has stopped."""
    handles = []
    user32.EnumDesktopWindows(desktop, WNDENUMPROC(lambda hwnd, _: handles.append(hwnd) or True), 0)
    handles = [hwnd for hwnd in handles if window_process(hwnd) == pid]
    if handles:
        see_as(handles[0])
    windows = []
    for hwnd in handles:
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


def drop(run_id, packet, offset=(0, 0)):
    """Lands an item packet the server once sent (0x9C, an item on the ground)
    offset from the character, in map tiles, as though the server had sent it
    now: as an item of its own, through BH's item filter. The server knows
    nothing of it, so it cannot be picked up. Returns the item's id and place,
    and whether it landed, which it does not when the filter hides it."""
    return ask(run_id, {"cmd": "drop", "packet": bytes(packet).hex(), "offset": list(offset)})


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


DIFFICULTIES = ("normal", "nightmare", "hell")
DIFFICULTY_BUTTON_WIDTH = 272  # the three buttons on the difficulty screen, and no other button there


def enter(run_id, name, timeout, difficulty="normal"):
    """From the main menu or the character list, into a single player game:
    with the run's character if it has one, else with a new Sorceress called
    name. A character that has reached Nightmare is asked which difficulty,
    and a new one plays Normal. Returns the game's state."""
    if difficulty not in DIFFICULTIES:
        raise HarnessError(f"a difficulty is one of {', '.join(DIFFICULTIES)}, not {difficulty!r}")
    screen = state(run_id)["screen"]
    if screen == "main_menu":
        send_input(run_id, [f"click:{SINGLE_PLAYER[0]},{SINGLE_PLAYER[1]}"])
        screen = wait_screen(run_id, ["new_character", "character_list"], timeout)["screen"]
    if screen == "new_character":
        if not name.isalpha():
            raise HarnessError(f"a character name is letters only, not {name!r}")
        if difficulty != "normal":
            raise HarnessError(f"a new character plays Normal, not {difficulty.title()}")
        send_input(run_id, [f"click:{SORCERESS[0]},{SORCERESS[1]}", f"wait:{CLASS_PICK_SECONDS}",
                            f"text:{name}", f"click:{OK_BUTTON[0]},{OK_BUTTON[1]}"])
    elif screen == "character_list":
        send_input(run_id, [f"click:{OK_BUTTON[0]},{OK_BUTTON[1]}"])
    else:
        raise HarnessError(f"enter starts from main_menu or character_list, not {screen}")
    reached = wait_screen(run_id, ["game", "difficulty"], timeout)
    if reached["screen"] == "game":
        if difficulty != "normal":
            raise HarnessError(f"the character has not reached {difficulty.title()}: the game went straight into Normal")
        return reached
    buttons = sorted((c for c in reached["controls"]
                      if c["type"] == "button" and c["rect"][2] == DIFFICULTY_BUTTON_WIDTH),
                     key=lambda c: c["center"][1])
    button = buttons[DIFFICULTIES.index(difficulty)]
    if button["disabled"]:
        raise HarnessError(f"the character has not reached {difficulty.title()}")
    send_input(run_id, [f"click:{button['center'][0]},{button['center'][1]}"])
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


def viewport_of(run_id, window):
    """Where the canvas lies in the game window of the size given. GDI's
    window is its canvas. D2GL's is whatever size the run asked for, with the
    canvas the game says it is drawing scaled into it."""
    if renderer_of(run_id) != "d2gl":
        return Viewport(window, window)
    canvas = state(run_id).get("size")
    if not canvas:
        raise HarnessError("the game is loading, and has no canvas to aim at or measure: wait for a screen first")
    return Viewport(window, canvas)


# --- driving ------------------------------------------------------------------
#
# Input is posted to the game window. D2 reads the cursor from the messages
# themselves, so a click lands where it is aimed without the real cursor moving.
# Posted keys do not change what GetKeyState reports, so anything the game asks
# of the keyboard directly, rather than through its messages, does not see them.
#
# Positions are given in canvas pixels and posted in window pixels, which under
# D2GL are not the same (viewport.py). The canvas changes size between the
# menus and a game, so each position is placed when it is posted.

WM_KEYDOWN, WM_KEYUP, WM_CHAR, WM_SYSKEYDOWN, WM_SYSKEYUP = 0x100, 0x101, 0x102, 0x104, 0x105
WM_MOUSEMOVE, WM_LBUTTONDOWN, WM_LBUTTONUP, WM_RBUTTONDOWN, WM_RBUTTONUP = 0x200, 0x201, 0x202, 0x204, 0x205
WM_MOUSEWHEEL, WHEEL_DELTA = 0x20A, 120
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
  press:X,Y      left button down at X,Y      release:X,Y  and up: a drag, with move: between
  wheel:X,Y,N    turn the wheel N notches at X,Y, up for positive N
  key:NAME       press and release a key      down:NAME / up:NAME   hold, release
  text:STRING    type STRING as a keyboard would, into whatever has focus
  wait:SECONDS   pause, for the game to react
  shot:FILE.png  screenshot to FILE.png, with any held keys still held
NAME is a letter or digit, F1-F12, NUMPAD0-NUMPAD9, a virtual-key code as 0xNN,
or one of: """ + ", ".join(sorted(k for k in KEYS if not k.startswith(("f", "numpad"))))


POINT_ACTIONS = ("move", "click", "rclick", "press", "release")


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
    held across actions so that a click while Shift is down says so. place
    turns a canvas position into the window's, and shoot takes a screenshot
    to a file."""

    def __init__(self, window, place, shoot):
        self.hwnd = window["hwnd"]
        self.place = place
        self.shoot = shoot
        self.held = set()
        self.pressed = False

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
        if kind in POINT_ACTIONS:
            x, y = self.place(*(int(v) for v in arg.split(",")))
            if kind == "move":
                # A move during a drag says the button is down, as Windows does.
                self.post(WM_MOUSEMOVE, self.modifiers() | (MK_LBUTTON if self.pressed else 0),
                          mouse_param(x, y))
            elif kind == "press":
                self.post(WM_MOUSEMOVE, self.modifiers(), mouse_param(x, y))
                time.sleep(0.08)
                self.post(WM_LBUTTONDOWN, self.modifiers() | MK_LBUTTON, mouse_param(x, y))
                self.pressed = True
            elif kind == "release":
                self.post(WM_MOUSEMOVE, self.modifiers() | MK_LBUTTON, mouse_param(x, y))
                time.sleep(0.08)
                self.post(WM_LBUTTONUP, self.modifiers(), mouse_param(x, y))
                self.pressed = False
            elif kind == "click":
                self.click(x, y, WM_LBUTTONDOWN, WM_LBUTTONUP, MK_LBUTTON)
            else:
                self.click(x, y, WM_RBUTTONDOWN, WM_RBUTTONUP, MK_RBUTTON)
        elif kind == "wheel":
            x, y, notches = (int(v) for v in arg.split(","))
            x, y = self.place(x, y)
            self.post(WM_MOUSEMOVE, self.modifiers(), mouse_param(x, y))
            time.sleep(0.08)
            # The wheel's message carries screen coordinates, unlike the others.
            point = wt.POINT(x, y)
            user32.ClientToScreen(self.hwnd, ctypes.byref(point))
            delta = (notches * WHEEL_DELTA) & 0xFFFF
            self.post(WM_MOUSEWHEEL, (delta << 16) | self.modifiers(),
                      mouse_param(point.x & 0xFFFF, point.y & 0xFFFF))
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
            self.shoot(arg)


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
        window = game_window(desktop, run["game"])
        source = Input(window, lambda x, y: viewport_of(run_id, window["client"]).to_window(x, y),
                       lambda path: shot(run_id, path))
        try:
            for action in actions:
                source.run(action)
                time.sleep(FRAME_SECONDS)
            return then() if then else None
        finally:
            for vk in list(source.held):
                source.key(vk, True)
            if source.pressed:
                source.post(WM_LBUTTONUP, 0, 0)

    return on_desktop(run_id, drive)


def validate_action(action):
    """Raises for an action Input.run would refuse, before any is sent."""
    kind, _, arg = action.partition(":")
    if kind in POINT_ACTIONS:
        try:
            x, y = (int(v) for v in arg.split(","))
        except ValueError:
            raise HarnessError(f"{action!r}: expected {kind}:X,Y") from None
    elif kind == "wheel":
        try:
            x, y, notches = (int(v) for v in arg.split(","))
        except ValueError:
            raise HarnessError(f"{action!r}: expected wheel:X,Y,NOTCHES") from None
    elif kind in ("key", "down", "up"):
        virtual_key(arg)
    elif kind == "wait":
        try:
            float(arg)
        except ValueError:
            raise HarnessError(f"{action!r}: expected wait:SECONDS") from None
    elif kind == "shot":
        if not arg.lower().endswith(".png"):
            raise HarnessError(f"{action!r}: expected shot:FILE.png")
    elif kind != "text" or not arg:
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
    """The window's client area as GDI drew it, read back as top-down BGRA.
    Only for a game GDI renders: a window OpenGL draws has nothing behind its
    DC on a desktop nobody is looking at, and the read returns the player's
    own screen."""
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


FRAME_FILE = "frame.bgra"
BYTES_PER_PIXEL = 4


def presented_frame(run_id):
    """The whole window as D2GL is about to show it, read by the driver from
    inside the game, as (width, height, top-down BGRA)."""
    path = run_dir(run_id) / f"{secrets.token_hex(4)}-{FRAME_FILE}"
    try:
        width, height = ask(run_id, {"cmd": "shot", "path": str(path)})["size"]
        pixels = path.read_bytes()
    finally:
        path.unlink(missing_ok=True)
    if len(pixels) != width * height * BYTES_PER_PIXEL:
        raise HarnessError(f"the driver wrote {len(pixels)} bytes for a {width}x{height} frame")
    return width, height, pixels


def cut(pixels, width, rect):
    """The rectangle x, y, w, h of a top-down BGRA picture that wide."""
    x, y, w, h = rect
    stride = width * BYTES_PER_PIXEL
    return b"".join(pixels[row * stride + x * BYTES_PER_PIXEL:row * stride + (x + w) * BYTES_PER_PIXEL]
                    for row in range(y, y + h))


class Picture:
    """A screenshot: top-down BGRA of the size given, of a game window of the
    size given, with where the canvas lies in that window and where in the
    window the picture's top left corner is."""

    def __init__(self, width, height, pixels, window, viewport, origin):
        self.width = width
        self.height = height
        self.pixels = pixels
        self.window = window
        self.viewport = viewport
        self.origin = origin

    def place(self, rect):
        """Where in the picture a rectangle of the canvas is, as [x, y, w, h]."""
        if self.viewport is None:
            raise HarnessError("the picture is of a game that was loading, which has no canvas to place anything on")
        x, y, w, h = self.viewport.rect_to_window(rect)
        return [x - self.origin[0], y - self.origin[1], w, h]


def grab(run_id, crop=None):
    """A picture of the game window's client area, or of the crop X,Y,W,H of
    the canvas."""
    run = read_run(run_dir(run_id))
    if not running(run):
        raise HarnessError(f"{run_id} is not running")
    d2gl = renderer_of(run_id) == "d2gl"
    if d2gl:
        width, height, frame = presented_frame(run_id)
    else:
        width, height = on_desktop(run_id, lambda desktop: game_window(desktop, run["game"])["client"])
    try:
        viewport = viewport_of(run_id, (width, height))
    except HarnessError:
        # A game that is loading has no canvas, and is still worth a picture.
        if crop:
            raise
        viewport = None
    area = [0, 0, width, height]
    if crop:
        cx, cy, cw, ch = crop
        canvas_w, canvas_h = viewport.canvas
        if cx < 0 or cy < 0 or cw <= 0 or ch <= 0 or cx + cw > canvas_w or cy + ch > canvas_h:
            raise HarnessError(f"crop {cx},{cy},{cw},{ch} is outside the {canvas_w}x{canvas_h} canvas")
        area = viewport.rect_to_window(crop)
    x, y, w, h = area
    if d2gl:
        pixels = cut(frame, width, area) if crop else frame
    else:
        pixels = on_desktop(run_id, lambda desktop: capture(game_window(desktop, run["game"])["hwnd"], x, y, w, h))
    return Picture(w, h, pixels, [width, height], viewport, (x, y))


def shot(run_id, out, crop=None):
    picture = grab(run_id, crop)
    write_png(out, picture.width, picture.height, picture.pixels)
    return {"path": str(Path(out).resolve()), "size": [picture.width, picture.height], "window": picture.window}


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
    for root in player_dirs():
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


def window_size(text):
    width, by, height = text.lower().partition("x")
    if not by or not width.isdigit() or not height.isdigit():
        raise argparse.ArgumentTypeError(f"expected WIDTHxHEIGHT, as in 1612x840, not {text!r}")
    return int(width), int(height)


# --- this machine -------------------------------------------------------------

# FileDispositionInfoEx, which teardown deletes a sandbox's links with.
MIN_WINDOWS_BUILD = 17763


def doctor():
    """What a run needs of this machine, each with whether it is there and
    what to do when it is not. Returns whether every need is met; what only
    the D2GL renderer needs is reported and not counted."""
    needs = []

    def need(met, what, remedy, required=True):
        needs.append((bool(met), required, what, remedy))

    build = sys.getwindowsversion().build
    need(build >= MIN_WINDOWS_BUILD, f"Windows build {build}", "Windows 10 1809 or later is needed")
    source = None
    try:
        source = game_source()
        named = "from " + (GAME_SOURCE_VARIABLE if os.environ.get(GAME_SOURCE_VARIABLE) else "the registry")
        missing = [name for name in BASE_FILES + [SGD2_CONFIG] if not (source / name).is_file()]
        need(not missing, f"game source {source} ({named})",
             f"it lacks {', '.join(missing)}: set {GAME_SOURCE_VARIABLE} to a 1.13c install with the Slash Diablo mods")
    except HarnessError as error:
        need(False, "game source", str(error))
    has_d2gl = source is not None and all((source / name).is_file() for name in D2GL_FILES)
    need(has_d2gl, "D2GL in the game source, for --renderer d2gl",
         f"the source has no {', '.join(D2GL_FILES)}: runs render with GDI only", required=False)
    need(all((BASE / name).exists() for name in BASE_FILES), f"base {BASE}", "run `bhctl.py base`")
    need(not has_d2gl or all((BASE / name).exists() for name in D2GL_FILES), "D2GL in the base",
         "run `bhctl.py base`", required=False)
    need(all((HARNESS_BIN / name).exists() for name in ("AgentLaunch.exe", "AgentShim.dll")),
         f"launcher {HARNESS_BIN}", "build it: msbuild /t:AgentLaunch /p:Configuration=Release BH.sln")
    need(AGENT_BH.exists(), f"agent build {AGENT_BH}",
         "build it: msbuild /t:BH /p:Configuration=Release /p:BHAgent=true BH.sln")
    for met, required, what, remedy in needs:
        label = "ok" if met else "MISSING" if required else "absent"
        print(f"{label:8}{what}" + ("" if met else f"\n        {remedy}"))
    return all(met for met, required, _, _ in needs if required)


DROP_HELP = """The packet is one BH's item capture recorded ("Capture Item Drops: True" in
BH_settings.cfg writes item-captures.txt): BHTests\\fixtures\\filter-cases.txt
holds 1351 of them. The item lands where the offset puts it, named and coloured
by the run's item filter, or not at all if the filter hides it."""


def drop_packet(args):
    """The packet the drop command was given, or the one it picked from a capture."""
    if args.packet is not None:
        if args.code or args.name or args.nth:
            raise HarnessError("--code, --name and --nth pick from a --capture")
        try:
            return bytes.fromhex(args.packet)
        except ValueError:
            raise HarnessError("--packet is the item packet as hex") from None
    try:
        return captures.pick(captures.read_drops(args.capture), args.code, args.name, args.nth)["packet"]
    except captures.CaptureError as error:
        raise HarnessError(str(error)) from None


def emit(value):
    print(value if isinstance(value, str) else json.dumps(value, indent=2, ensure_ascii=False))


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("doctor")
    commands.add_parser("base")
    create_cmd = commands.add_parser("create")
    create_cmd.add_argument("--bh", type=Path, default=AGENT_BH)
    create_cmd.add_argument("--setting", type=setting, action="append", default=[], metavar="KEY=VALUE",
                            help="a BH_settings.cfg line to write in place of the shipped one; repeatable")
    create_cmd.add_argument("--character", type=Path, metavar="FILE.d2s",
                            help="play a copy of this character, for enter to pick from the character list")
    create_cmd.add_argument("--renderer", choices=RENDERERS, default="gdi",
                            help="what draws the game: its own GDI (the default), or D2GL as players see it")
    create_cmd.add_argument("--window", type=window_size, metavar="WIDTHxHEIGHT",
                            help=f"with --renderer d2gl: the window D2GL scales the game into (default {RESOLUTION})")
    create_cmd.add_argument("--d2gl", type=setting, action="append", default=[], metavar="SECTION.KEY=VALUE",
                            help="with --renderer d2gl: a d2gl.ini setting, as in Feature.hd_text=true; repeatable")
    launch_cmd = commands.add_parser("launch")
    launch_cmd.add_argument("id")
    launch_cmd.add_argument("--lifetime", type=float, default=LIFETIME_SECONDS,
                            help="seconds before the launcher ends the game regardless")
    enter_cmd = commands.add_parser("enter")
    enter_cmd.add_argument("id")
    enter_cmd.add_argument("--name", default="Agent", help="for a new character: letters only")
    enter_cmd.add_argument("--difficulty", choices=DIFFICULTIES, default="normal")
    enter_cmd.add_argument("--timeout", type=float, default=60)
    drop_cmd = commands.add_parser("drop", epilog=DROP_HELP, formatter_class=argparse.RawDescriptionHelpFormatter)
    drop_cmd.add_argument("id")
    source = drop_cmd.add_mutually_exclusive_group(required=True)
    source.add_argument("--packet", metavar="HEX", help="the item packet itself")
    source.add_argument("--capture", type=Path, metavar="FILE", help="an item capture to take a drop from")
    drop_cmd.add_argument("--code", help="with --capture: a drop of this item code")
    drop_cmd.add_argument("--name", help="with --capture: a drop with this in its name")
    drop_cmd.add_argument("--nth", type=int, default=0, help="with --capture: which of those, from 0")
    drop_cmd.add_argument("--offset", type=lambda s: tuple(int(v) for v in s.split(",")), default=(0, 0),
                          metavar="DX,DY", help="from the character, in map tiles")
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
    scenario_cmd.add_argument("--bh", type=Path, default=AGENT_BH)
    scenario_cmd.add_argument("--renderer", choices=RENDERERS,
                              help="draw the game with this, whatever the scenario says")
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
        if args.command == "doctor":
            sys.exit(0 if doctor() else 1)
        elif args.command == "base":
            build_base()
        elif args.command == "create":
            emit(create(args.bh, dict(args.setting), args.character, args.renderer, args.window, dict(args.d2gl)))
            for key, _ in args.setting:
                if key not in shipped_settings_keys():
                    print(f"note: {key!r} is not in the shipped BH_settings.cfg; it was added, "
                          "and does nothing unless BH reads it", file=sys.stderr)
        elif args.command == "launch":
            emit(launch(args.id, game_args, args.lifetime))
        elif args.command == "enter":
            emit(enter(args.id, args.name, args.timeout, args.difficulty)["game"])
        elif args.command == "drop":
            emit(drop(args.id, drop_packet(args), args.offset))
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
            report = scenario.run_scenario(args.file, args.out, args.bh, args.keep, args.renderer)
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
