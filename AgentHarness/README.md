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
python AgentHarness\bhctl.py input $id click:400,306 wait:2 key:esc text:Name
python AgentHarness\bhctl.py wait $id game         # until the game shows that screen
python AgentHarness\bhctl.py state $id             # the screen and its controls, or the character
python AgentHarness\bhctl.py drawlog $id           # everything BH drew in one frame
python AgentHarness\bhctl.py shot $id out.png      # --crop X,Y,W,H for part of it
python AgentHarness\bhctl.py leave $id             # save and exit the game
python AgentHarness\bhctl.py teardown $id          # always, when done
```

`input --help` lists every action. The `run-game` skill is the walkthrough for
agents, from building to reporting.

`snapshot` and `compare` record the player's registry keys and game folders, to
show a run changed none of them.

## Rules

- A run is torn down when its work is done. `sweep` removes any whose launcher
  has gone, and the launcher ends its game after an hour regardless.
- The harness reads the dev install to build the base and never writes to either
  install. It never connects to a realm and holds no credentials.
- Only `bhctl` and `AgentLaunch` stop a game. Never kill a `Game.exe` by name:
  the player's own clients are `Game.exe` too.
