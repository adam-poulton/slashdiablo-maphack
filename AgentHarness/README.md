# Agent harness

Runs Diablo II for an agent, single player only, in a sandbox that cannot reach
the player's installs, saves, settings, realm accounts, screen, mouse or
keyboard. Why it is built this way is ADR 0011.

## What a machine needs

- Windows 10 1809 or later, Visual Studio with MSBuild, and Python 3.11 or
  later. The harness uses nothing outside Python's standard library.
- A Diablo II 1.13c install with the Slash Diablo mods (D2Template and
  SGD2FreeRes), to copy the game's files from. It is read and never written.
- For runs drawn by D2GL: D2GL in that install (`glide3x.dll` and `d2gl.mpq`),
  and a GPU.

What differs from one machine to the next is read from the environment, and
nothing in the harness names a path of anyone's:

| Variable | Is | Default |
|---|---|---|
| `BHAGENT_GAME_SOURCE` | the install the base is copied from | the install the game's registry key names |
| `BHAGENT_ROOT` | where the base and the runs are kept | `~\.bhagent` |
| `BHAGENT_PLAYER_DIRS` | more folders for `snapshot` to watch, separated by `;` | none |

Set them where the shell that runs `bhctl` gets its environment: for an agent,
the `env` block of `.claude/settings.local.json`, which is not committed.

```powershell
python AgentHarness\bhctl.py doctor
```

says what the machine has of all this and what to do about what it lacks, and
exits 0 when a run can start.

## Building

```powershell
$msb = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1
& $msb /t:AgentLaunch /p:Configuration=Release /m BH.sln                    # -> AgentHarness\bin\Release
& $msb /t:BH /p:Configuration=Release /p:BHAgent=true /m BH.sln             # -> AgentRelease\BH.dll
```

A sandbox runs the agent build of BH, Release with the driver that answers
`state`, `wait`, `drawlog`, `drop`, `session`, `shot` and `leave` (ADR 0012). It builds
into folders of its own, so it and Release can both be built without either
rebuilding the other.

## Running

```powershell
python AgentHarness\bhctl.py base                  # once, and after the game files change
$id = python AgentHarness\bhctl.py create          # takes AgentRelease\BH.dll, or --bh <path>
python AgentHarness\bhctl.py launch $id            # hidden, returns at the main menu
python AgentHarness\bhctl.py enter $id             # a new Sorceress, into Normal
python AgentHarness\bhctl.py input $id key:numpad8 wait:0.5 text:Name
python AgentHarness\bhctl.py drop $id --capture BHTests\fixtures\filter-cases.txt --code rin --offset 3,2
python AgentHarness\bhctl.py session $id BHTests\fixtures\session-baal-runs.jsonl   # show a recorded session
python AgentHarness\bhctl.py wait $id main_menu    # until the game shows that screen
python AgentHarness\bhctl.py state $id             # the screen and its controls, or the character and the items around it
python AgentHarness\bhctl.py drawlog $id           # everything BH drew in one frame
python AgentHarness\bhctl.py shot $id out.png      # --crop X,Y,W,H for part of it
python AgentHarness\bhctl.py leave $id             # save and exit the game
python AgentHarness\bhctl.py teardown $id          # always, when done
```

`create --setting "KEY=VALUE"`, repeatable, writes a line of `BH_settings.cfg`
in place of the shipped one. `input --help` and `drop --help` list what they
take. The `run-game` skill is the walkthrough for agents, from building to
reporting.

## Renderers

A run's game is drawn by one of two renderers, chosen when the sandbox is made:

| `create --renderer` | Draws | For |
|---|---|---|
| `gdi`, the default | the game's own windowed renderer, the same on every machine | what BH draws and where: layout, text, colour |
| `d2gl` | the game through D2GL, as Slash Diablo players see it (ADR 0015) | how it looks to a player: scaling, D2GL's HD text, cursor and map over BH's drawing |

```powershell
$id = python AgentHarness\bhctl.py create --renderer d2gl --window 1612x840 --d2gl "Feature.hd_text=true"
```

- `--window WIDTHxHEIGHT` is the window D2GL scales the game into, at least
  800x600. It defaults to the in-game resolution, 1344x700, where a pixel of a
  screenshot taken in a game is a pixel of the game.
- `--d2gl "SECTION.KEY=VALUE"`, repeatable, writes a setting of `d2gl.ini`.
  Everything else is D2GL's own default, not the player's: HD text, the HD
  cursor and the minimap are all off until a run turns them on
  (`Feature.hd_text=true`, `Feature.hd_cursor=true`, `Feature.mini_map=true`).
- What a sandbox needs of D2GL is not a run's to set: the window's place and
  style, the unlocked cursor, vertical sync and the frame rate cap.

Positions are the same under both. Everything `bhctl` takes or reports is in
pixels of the canvas, which is what the game draws on: 800x600 in the menus and
1344x700 in a game. Under GDI the window is the canvas. D2GL scales the canvas
to fit its window and centres it, and `bhctl` does the same arithmetic: a click
lands on the canvas pixel named, `shot --crop` cuts the canvas rectangle named,
and a check's outline is drawn where the screenshot shows the thing. A
screenshot itself is the whole window, so one of the menus has bars beside the
canvas, and one taken in a larger window is larger than the canvas.

With D2GL's HD text on, the game measures text in D2GL's fonts. Text BH draws
is then a pixel or two wider or narrower than under GDI, and the draw log says
so: check such text by `text` and `inside`, and keep an exact `rect` for a
scenario that runs under one renderer.

## Characters and drops

A run plays a new level 1 Sorceress unless it is given a character:
`create --character FILE.d2s` plays a copy of that save, which `enter` picks
from the character list (`--difficulty nightmare` or `hell` for one that has
reached it). The file itself is never written. Characters worth playing again
are kept in `AgentHarness\fixtures\`:

| Save | Character |
|---|---|
| `Daicos.d2s` | level 99 Sorceress, Nightmare finished, so Hell is open |
| `Adam.d2s` | level 96 Necromancer, Hell finished |

`drop` lands an item next to the character as though the server had just sent
it: an item packet BH's item capture recorded ("Capture Item Drops: True"
writes `item-captures.txt` beside BH's configs), given a unit of its own and a
place `--offset` tiles away. It goes through BH's packet hook like any drop, so
it is named, coloured, noted and put on the map by the run's item filter, and
an item the filter hides never lands. `BHTests\fixtures\filter-cases.txt` holds
1351 recorded drops. The server knows nothing of a dropped item, so clicking
it does nothing.

`state` lists the items on the ground with the name the game shows for each,
which is the name the item filter gave it (ADR 0014).

`session` puts a session log up in place of the session being played, for the
session HUD and window to show: a log as BH's session tracker writes them to
`BH_sessions`, such as `BHTests\fixtures\session-baal-runs.jsonl`, eight games
with a death and a level gained. A run only ever stands in the Rogue Encampment,
so this is how a session with a history is looked at. The log's clock stands at
its last event, so what is drawn does not change from one frame to the next,
and it stays up until a game is joined.

## Scenarios

A scenario is a check of BH in a running game, written down so that it runs the
same way every time and reports what it found (ADR 0013):

```powershell
python AgentHarness\bhctl.py scenario AgentHarness\scenarios\smoke.json
python AgentHarness\bhctl.py scenario AgentHarness\scenarios\smoke.json --renderer d2gl
```

It creates and launches a run, goes through the steps, tears the run down, and
writes `report.md` and `report.json` to `AgentHarness\reports\<time>-<name>\`
(or `--out DIR`), with a screenshot for every check. It exits 0 when every check
holds, 1 when one does not, and 2 when the scenario could not finish. `--keep`
leaves the run up afterwards and prints its id. `--renderer` draws the game
with that renderer whatever the scenario says, which is how one scenario is run
under both.

```json
{
  "title": "The settings hotkey opens BH's settings",
  "start": "game",
  "name": "Agent",
  "settings": {"Show Settings": "True, VK_F10"},
  "steps": [
    {"do": ["key:f10"]},
    {"check": "the window opens at the top left", "drawn": {"text": "Settings", "rect": [4, 3, 52, 10]}}
  ]
}
```

- `title` says what the scenario shows.
- `start` is `game` (the default) or `main_menu`.
- `character` is a character save to play, as `create --character` takes it,
  and `difficulty` the one to play it in (`normal`, the default, `nightmare` or
  `hell`). Without one the game is a new Sorceress named `name`, default
  `Agent`, in Normal.
- `settings` maps `BH_settings.cfg` keys to values, as `create --setting` does.
  A key the shipped file does not set is added, and the report notes it.
- `renderer` is `gdi` (the default) or `d2gl`. With `d2gl`, `window` is
  `[width, height]` and `d2gl` maps `d2gl.ini` settings to values, as
  `create --window` and `--d2gl` take them:
  `"d2gl": {"Feature.hd_text": "true"}`.

Paths in a scenario are from the repository's root unless absolute.

Each step names one kind:

| Step | Does |
|---|---|
| `{"do": [ACTION, ...]}` | input, as `bhctl input` takes it |
| `{"wait": SCREEN or [SCREEN, ...], "timeout": 60}` | until the game shows one |
| `{"check": "what should be true", ...}` | a check, below |
| `{"drop": {"capture": FILE, "code": CODE, "nth": N}, "offset": [dx, dy]}` | a recorded item lands, as `bhctl drop` lands it; `"name"` picks by name, and `{"packet": HEX}` gives the packet itself |
| `{"session": FILE}` | a recorded session goes up, as `bhctl session` puts it |
| `{"shot": NAME, "crop": [x, y, w, h]}` | a screenshot for the report |
| `{"drawlog": NAME}` | a draw log for the report |
| `{"leave": true}` | save and exit the game |
| `{"enter": true}` | back into a game, with the run's character |

A check holds when every part it has does, and is tried again until it does or
its `timeout` (default 5 seconds) runs out, so it needs no `wait:` before it.
Whatever the outcome, its screenshot goes in the report.

- `"state": {PATH: VALUE, ...}`: each dotted path into `bhctl state`'s answer
  equals its value, as in `{"screen": "game", "game.panels": []}`. A list takes
  an index: `"size.0": 1344`.
- `"drawn": {FIELD: VALUE, ...}`: BH drew an item with every field given. The
  fields are `kind` (`text`, `box`, `frame` or `line`), `text` (all of it, colour
  codes aside), `contains` (part of it), `font`, `color`, `rect` (exactly
  `[x, y, w, h]`; a line's is the box around it) and `inside` (wholly within
  `[x, y, w, h]`). With `"count": N` exactly N items match, and `"count": 0`
  means nothing like it was drawn.
- `"item": {FIELD: VALUE, ...}`: an item on the ground has every field given:
  `code`, `name` (all of it as the game shows it, colour codes aside),
  `contains` (part of the name) and `id`. `count` works as for `drawn`, so
  `{"item": {"code": "aqv"}, "count": 0}` says the filter hid the arrows. A
  check has `drawn` or `item`, not both.
- `"hold": [KEY, ...]` keeps keys held down while the check runs: `["alt"]`
  shows the items' names in its screenshot.

What a check matched is outlined in magenta on its screenshot. A text check
that matched nothing lists what read most like it, outlined in yellow, which is
usually the right text a pixel or a colour away.

`AgentHarness\scenarios\` holds scenarios worth running again: `smoke.json`
for BH loading and its settings, `item-drops.json` and
`item-drops-high-level.json` for the item filter on recorded drops, for a new
character and for Daicos in Hell, `session.json` for the session HUD and
window on a recorded session, and `session-run.json` for a timed run against
its best, on the recorded race in `BHTests\fixtures\session-race.jsonl`. One written to check a single change can stay
in a scratch folder.

## Tests

The harness's logic that needs no game is tested with
`python -m unittest discover -s AgentHarness`.

`snapshot` and `compare` record the player's registry keys and game folders, to
show a run changed none of them.

## Rules

- A run is torn down when its work is done. `sweep` removes any whose launcher
  has gone, and the launcher ends its game after an hour regardless.
- The harness reads the game source to build the base and never writes to any
  install. It never connects to a realm and holds no credentials.
- Only `bhctl` and `AgentLaunch` stop a game. Never kill a `Game.exe` by name:
  the player's own clients are `Game.exe` too.
- A screenshot of a run D2GL draws comes from the driver, never from the
  window: read from outside, such a window gives back the player's own screen.
