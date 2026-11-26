---
name: run-game
description: Run Diablo II with a BH build yourself, hidden and sandboxed, to drive it, read its state and take screenshots. Use to check a change in game, to see or measure how BH's windows and overlays are drawn, or to reproduce something that only happens in a running game. For putting a build in front of the user to test, use deploy-dev-bh instead.
---

# Run the game

`AgentHarness/bhctl.py` runs single player Diablo II in a sandbox on a hidden
desktop: nothing appears on the user's screen, and nothing reaches their installs,
saves or settings. The game runs the agent build of BH, which answers questions
about the game. ADRs 0011 and 0012 are why it works this way.

## Steps

1. **Build** the agent build, Release with the driver in it:
   `/t:BH /p:Configuration=Release /p:BHAgent=true`, giving `AgentRelease\BH.dll`.
   If `AgentHarness\bin\Release\AgentLaunch.exe` is missing, also `/t:AgentLaunch`.
   MSBuild is found with vswhere, as in `AgentHarness/README.md`.
2. **Start a run.**
   ```powershell
   $id = python AgentHarness\bhctl.py create     # says to run `base` first if the base is missing
   python AgentHarness\bhctl.py launch $id       # returns at the main menu
   ```
3. **Get into a game.** A new run has no characters. This makes a Sorceress and
   enters Normal in the Rogue Encampment:
   ```powershell
   python AgentHarness\bhctl.py input $id click:400,306 wait:2 click:615,300 wait:2 text:Agent click:690,555
   python AgentHarness\bhctl.py wait $id game
   ```
   A character name is letters only. Once one exists, Single Player opens the
   character list (`wait $id character_list`), and `click:690,555` enters with it.
4. **Drive and look**, as a loop, choosing the cheapest way to see:
   - `state` says what is on screen. Out of a game: the screen's name and every
     control with the centre to click. In a game: the character, area,
     difficulty and open panels.
   - `drawlog` is everything BH drew in one frame: each text with its exact
     rectangle, font and colour, and each box and line. Check what BH shows, and
     where, from this rather than from a picture.
   - `shot FILE.png` (with `--crop X,Y,W,H`) is for how it looks; read the PNG
     with the Read tool.
   - `input` acts; `input --help` lists every action. `wait $id SCREEN` holds
     until the game gets there.
   Keep screenshots and draw logs in your scratchpad.
5. **Tear down**, every time, including after a failure:
   `python AgentHarness\bhctl.py teardown $id`.
6. **Report.** Send the screenshots that show the result to the user with
   SendUserFile. An in-game check the state, draw log or screenshots settle is
   reported as checked by you, with the evidence. Anything they cannot settle,
   such as timing, feel or what only D2GL draws, stays on the user's list as not
   yet checked.

## What a run is like

- **Coordinates** are pixels in the game window's client area: 800x600 in the
  menus, 1344x700 in a game.
- **The cursor** is drawn into every screenshot, and hovering shows tooltips.
  `move:` it somewhere empty first when the picture should show neither.
- **The keyboard** behaves as a real one: `text:` types into the game's boxes
  and BH's alike, and held keys (Alt for ground items, Shift and Ctrl for BH's
  shortcuts) are seen as held, but only within one `input` call, which releases
  them at its end. Take the screenshot inside it with a `shot:FILE` action.
- **Settings** are BH's shipped defaults (`Packaging/BH_settings.cfg`, which
  also lists the hotkeys) and the game's defaults, not the user's.
- **Leaving a game** is `leave`, which saves, as Save and Exit does, and returns
  to the main menu. `close` leaves any game and quits.
- **Stopping a game** is `teardown`. The user's own clients are `Game.exe` too,
  so a game is never ended by name. A run also ends itself after an hour.
- **A command that stops at a dialog** prints the dialog's text and leaves the
  run up for `status` and `shot`; tear it down when done.
