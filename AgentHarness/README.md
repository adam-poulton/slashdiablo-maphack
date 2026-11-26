# Agent harness

Runs Diablo II for an agent, single player only, in a sandbox that cannot reach
the player's installs, saves, settings, realm accounts, screen, mouse or
keyboard. Why it is built this way is ADR 0011.

## Building

```powershell
$msb = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1
& $msb /t:AgentLaunch /p:Configuration=Release /m BH.sln                    # -> AgentHarness\bin\Release
& $msb /t:BH /p:Configuration=Release /p:BHAgent=true /m BH.sln             # -> AgentRelease\BH.dll
```

A sandbox runs the agent build of BH, Release with the driver that answers
`state`, `wait`, `drawlog` and `leave` (ADR 0012). It builds into folders of its
own, so it and Release can both be built without either rebuilding the other.

## Running

```powershell
python AgentHarness\bhctl.py base                  # once, and after the game files change
$id = python AgentHarness\bhctl.py create          # takes AgentRelease\BH.dll, or --bh <path>
python AgentHarness\bhctl.py launch $id            # hidden, returns at the main menu
python AgentHarness\bhctl.py enter $id             # a new Sorceress, into Normal
python AgentHarness\bhctl.py input $id key:numpad8 wait:0.5 text:Name
python AgentHarness\bhctl.py wait $id main_menu    # until the game shows that screen
python AgentHarness\bhctl.py state $id             # the screen and its controls, or the character
python AgentHarness\bhctl.py drawlog $id           # everything BH drew in one frame
python AgentHarness\bhctl.py shot $id out.png      # --crop X,Y,W,H for part of it
python AgentHarness\bhctl.py leave $id             # save and exit the game
python AgentHarness\bhctl.py teardown $id          # always, when done
```

`create --setting "KEY=VALUE"`, repeatable, writes a line of `BH_settings.cfg`
in place of the shipped one. `input --help` lists every action. The `run-game`
skill is the walkthrough for agents, from building to reporting.

## Scenarios

A scenario is a check of BH in a running game, written down so that it runs the
same way every time and reports what it found (ADR 0013):

```powershell
python AgentHarness\bhctl.py scenario AgentHarness\scenarios\smoke.json
```

It creates and launches a run, goes through the steps, tears the run down, and
writes `report.md` and `report.json` to `AgentHarness\reports\<time>-<name>\`
(or `--out DIR`), with a screenshot for every check. It exits 0 when every check
holds, 1 when one does not, and 2 when the scenario could not finish. `--keep`
leaves the run up afterwards and prints its id.

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
- `start` is `game` (the default: a new Sorceress named `name`, default
  `Agent`, in Normal) or `main_menu`.
- `settings` maps `BH_settings.cfg` keys to values, as `create --setting` does.
  A key the shipped file does not set is added, and the report notes it.

Each step names one kind:

| Step | Does |
|---|---|
| `{"do": [ACTION, ...]}` | input, as `bhctl input` takes it |
| `{"wait": SCREEN or [SCREEN, ...], "timeout": 60}` | until the game shows one |
| `{"check": "what should be true", ...}` | a check, below |
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
- `"hold": [KEY, ...]` keeps keys held down while the check runs.

What a check matched is outlined in magenta on its screenshot. A text check
that matched nothing lists what read most like it, outlined in yellow, which is
usually the right text a pixel or a colour away.

`AgentHarness\scenarios\` holds scenarios worth running again. One written to
check a single change can stay in a scratch folder.

## Tests

The harness's logic that needs no game is tested with
`python -m unittest discover -s AgentHarness`.

`snapshot` and `compare` record the player's registry keys and game folders, to
show a run changed none of them.

## Rules

- A run is torn down when its work is done. `sweep` removes any whose launcher
  has gone, and the launcher ends its game after an hour regardless.
- The harness reads the dev install to build the base and never writes to either
  install. It never connects to a realm and holds no credentials.
- Only `bhctl` and `AgentLaunch` stop a game. Never kill a `Game.exe` by name:
  the player's own clients are `Game.exe` too.
