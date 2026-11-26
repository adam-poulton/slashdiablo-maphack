---
name: run-game
description: Run Diablo II with a BH build yourself, hidden and sandboxed, to check a change in game, see or measure how BH's windows and overlays are drawn, or reproduce something that only happens in a running game. Writes a report with screenshots as evidence. For putting a build in front of the user to test, use deploy-dev-bh instead.
---

# Run the game

`AgentHarness/bhctl.py` runs single player Diablo II in a sandbox on a hidden
desktop: nothing appears on the user's screen, and nothing reaches their installs,
saves or settings. The game runs the agent build of BH, which says what is on
screen and records everything BH draws. ADRs 0011 to 0013 are why it works this
way; `AgentHarness/README.md` is the reference for every command and the
scenario format.

## Steps

1. **Build** the agent build, Release with the driver in it:
   `/t:BH /p:Configuration=Release /p:BHAgent=true`, giving `AgentRelease\BH.dll`.
   If `AgentHarness\bin\Release\AgentLaunch.exe` is missing, also `/t:AgentLaunch`.
   MSBuild is found with vswhere, as in the README. A missing base is reported
   by the first command, with what to run.
2. **Write a scenario** for what the change should do: each claim a `check`, on
   `state` for what the game is doing and on `drawn` for what BH shows and
   exactly where. Put it in your scratchpad, or in `AgentHarness\scenarios\` if
   it is worth running again. `scenarios\smoke.json` is a worked example. When
   you do not yet know what to check, look first (below).
3. **Run it**: `python AgentHarness\bhctl.py scenario FILE.json`. It tears its
   run down whatever happens and prints where the report is.
4. **Read the report**: `report.md`, then the marked screenshots of each check
   with the Read tool. A failed check lists what BH drew instead, and the
   nearest text outlined in yellow. Fix the code or the scenario, rebuild, and
   run again until every check holds and the screenshots look right, which no
   check settles alone: alignment, clipping, overlap and colour are judged by eye.
5. **Report**: send `report.md` and the screenshots that show the result with
   SendUserFile. An in-game check the report settles is reported as checked by
   you, with its evidence. Anything it cannot settle, such as timing, feel or
   what only D2GL draws, stays on the user's list as not yet checked.

## Looking first

To explore before writing checks, drive a run by hand:

```powershell
$id = python AgentHarness\bhctl.py create     # --setting "KEY=VALUE" to change one of BH's settings
python AgentHarness\bhctl.py launch $id       # returns at the main menu
python AgentHarness\bhctl.py enter $id        # into a game in Normal
```

then loop on `state`, `drawlog --out FILE`, `shot FILE.png` and `input`, keeping
the files in your scratchpad, and finish with `teardown $id`, every time,
including after a failure. `scenario --keep` leaves a scenario's run up for the
same.

## What a run is like

- **Coordinates** are pixels in the game window's client area: 800x600 in the
  menus, 1344x700 in a game. A draw log's rectangles are in the same pixels.
- **The cursor** is drawn into every screenshot, and hovering shows tooltips.
  `move:` it somewhere empty first when the picture should show neither.
- **The keyboard** behaves as a real one: `text:` types into the game's boxes
  and BH's alike, and held keys (Alt for ground items, Shift and Ctrl for BH's
  shortcuts) are seen as held, but only within one `input` call or `do` step,
  which releases them at its end. A check that needs a key held says `hold`.
- **Settings** are BH's shipped defaults (`Packaging/BH_settings.cfg`, which
  also lists the hotkeys) and the game's defaults, not the user's.
- **What the draw log holds** is everything BH draws, and nothing the game draws
  itself. A `frame` is the game's ornate border art drawn around a rectangle BH
  gives it, so the art's own pixels can stray from that rectangle.
- **Stopping a game** is `teardown`, or the end of a scenario. The user's own
  clients are `Game.exe` too, so a game is never ended by name. A run also ends
  itself after an hour.
- **A command that stops at a dialog** prints the dialog's text and leaves the
  run up for `status` and `shot`; tear it down when done.
