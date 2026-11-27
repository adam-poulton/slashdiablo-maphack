# Agents run the game in a sandbox of their own

An agent that needs to see BH working starts a Diablo II of its own: a sandbox
folder built from a shared copy of the game, a registry that is the sandbox's
alone, and a Windows desktop nobody is looking at. It drives the game with
window messages and reads it back with screenshots, and nothing it does can
reach the player's installs, saves, settings, realm accounts, screen, mouse or
keyboard. It plays single player only.

The pieces are `AgentHarness/`: `bhctl.py`, which builds and tears down
sandboxes and launches them, `AgentLaunch.exe`, which starts one game and lives
exactly as long as it, and `AgentShim.dll`, which the launcher loads into the
game before the game's own code runs.

## What a sandbox shares and what it owns

Every sandbox is hardlinks into one read-only copy of the game's own files, the
base, so a run costs a few megabytes and a second or two instead of two
gigabytes. The base files are marked read-only, and because a hardlink shares
its attributes with its target, a sandbox that tried to write through one would
fail rather than change every run. That is also why teardown deletes the links
with `FILE_DISPOSITION_FLAG_IGNORE_READONLY_ATTRIBUTE` instead of clearing the
attribute first, which would unprotect the file for every other run.

What the game writes is the sandbox's own: BH and its configs, the Save folder,
SGD2FreeRes's config and the logs. Teardown deletes the folder and the
sandbox's registry key together, and `sweep` tears down any run whose launcher
has gone.

The base comes from the install `BHAGENT_GAME_SOURCE` names, or else the one
the game's registry key does, minus `SlashDiablo.dll`, the realm gateway, which
D2Template skips when it is missing, and plus D2GL's `glide3x.dll`, which is
the renderer only of a run that asks for it (ADR 0015). SGD2FreeRes loads it
whatever the video mode, to ask which Glide wrapper it is, and stops the game
with a dialog when it is absent.

The root is `~\.bhagent`, deliberately outside AppData. The Claude desktop app
is a packaged app, and Windows moves whatever it and its children write under
AppData into the package's own folder, so a root there would be one folder from
an agent and another from the player's shell.

## The registry is redirected from the first instruction

The game keeps its save path, its options and its realm gateways under
HKEY_CURRENT_USER, in the same key whichever install runs, and writes them back.
Left alone, a sandboxed game writes the player's `CmdLine` and replaces their
gateway list, Slash Diablo included, with the stock Blizzard one. Both were seen
in the sandbox's key during the spike.

`AgentShim.dll` calls `RegOverridePredefKey` to make
`HKCU\Software\BHAgent\runs\<id>\HKCU` the whole of HKEY_CURRENT_USER for that
one process. It has to be loaded by the launcher, into the suspended process, as
an APC on the main thread: the loader runs it after the game's static DLLs are
initialised and before `Game.exe`'s entry point. BH could not do this itself.
D2Template, and so BH, is loaded by D2Win only after the MPQs are open, from
`Game.exe`'s `WinMain`, by which time the game has already been to the
registry. The shim fails closed: without its key it ends the process, and the
launcher ends the game if the shim never reports in.

HKEY_LOCAL_MACHINE is not redirected, and is only read. The one value that
matters is `VideoConfig\Render`, which the video test writes and which selects
the renderer when no flag does; on this machine it selects Direct3D, which
cannot start full screen on a hidden desktop. A sandbox always runs `-w`, which
puts the game on its GDI renderer whatever HKLM says.

## Nothing is ever on the player's screen

`AgentLaunch` creates a desktop for the run in the player's window station and
starts the game on it. Nothing on that desktop reaches the player's monitor or
taskbar, and the game never takes focus from them.

Windowed GDI renders there as it would anywhere, including at the 1344x700
Slash Diablo players use, which SGD2FreeRes supports in GDI mode. A plain
`BitBlt` from the game window's DC, by a thread attached to that desktop,
returns the frame as drawn: the menus, the town, and BH's own overlays. Input is
window messages posted the same way. D2 takes the cursor's position from posted
`WM_MOUSEMOVE`s rather than from the real cursor, so clicks land where they are
aimed, and `WM_CHAR` types into its edit boxes.

A window on another desktop answers window calls as though its handle were
invalid, so every one of them, from listing windows to posting a click, goes
through a thread that has first called `SetThreadDesktop`.

## Only the launcher can see the game

`Game.exe` adds a deny entry to its own process, so no other process can open
it, down to `PROCESS_QUERY_LIMITED_INFORMATION`. The launcher holds the one
handle that can, the one `CreateProcess` returned before the game locked itself
away, and so the launcher stands for the game: it exits only once the game has,
and a run is alive exactly while its launcher is. The game runs in a job that
ends it when the launcher ends, however that happens. Setting
`Local\BHAgentStop-<launcher pid>` ends it on request, and a lifetime, an hour
unless given, ends it regardless, so a run nobody tears down cannot outlive the
afternoon.

## Considered options

Patching the registry for the length of a run, writing the sandbox's values into
the player's key and restoring them afterwards, was rejected. It races any
client the player has open, and a run that dies halfway leaves the player's
settings wrong.

Windows Sandbox was kept as a fallback rather than chosen. It isolates
everything, but it needs an administrator to switch it on, its window appears on
the player's desktop, and it would need the game copied in for every run.

Rendering with D2GL, as players do, was rejected for runs at first, on the
grounds that D2GL refuses `-w` and that OpenGL on a desktop nobody is looking
at is not something to depend on. Neither held when tried: ADR 0015 adds D2GL
as a renderer a run can ask for. GDI remains the default.

Capturing frames from inside the game was not needed for GDI. It is how a run
drawn by D2GL is photographed (ADR 0015), where `BitBlt` is not enough.

## Consequences

A run sees the game at the game's defaults, not the player's. Anything that
depends on a player setting has to be set in the run.

Closing the window only quits the game from its menus. In a game it is ignored,
and leaving cleanly is Esc and Save and Exit, which is the driver's to do.

The `Game.exe` deny entry means nothing outside the launcher can list the
game's modules or read its memory. What a test needs to know about the game it
learns from inside it, which is what the driver in BH is for.
