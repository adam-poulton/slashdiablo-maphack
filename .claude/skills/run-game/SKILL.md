---
name: run-game
description: Run Diablo II with a BH build yourself, hidden and sandboxed, to drive it and take screenshots. Use to check a change in game, to see how BH's windows and overlays look, or to reproduce something that only happens in a running game. For putting a build in front of the user to test, use deploy-dev-bh instead.
---

# Run the game

`AgentHarness/bhctl.py` runs single player Diablo II in a sandbox on a hidden
desktop: nothing appears on the user's screen, and nothing reaches their installs,
saves or settings. ADR 0011 is why it works this way.

## Steps

1. **Build.** BH Release (`/t:BH`, giving `Release\BH.dll`) and, if
   `AgentHarness\bin\Release\AgentLaunch.exe` is missing, `/t:AgentLaunch`. MSBuild
   is found with vswhere, as in `AgentHarness/README.md`.
2. **Start a run.**
   ```powershell
   $id = python AgentHarness\bhctl.py create     # says to run `base` first if the base is missing
   python AgentHarness\bhctl.py launch $id       # returns at the main menu
   ```
   `create --bh <path>` runs a BH.dll other than `Release\BH.dll`.
3. **Get into a game.** A new run has no characters. This makes a Sorceress and
   enters Normal in the Rogue Encampment:
   ```powershell
   python AgentHarness\bhctl.py input $id click:400,307 wait:2 click:615,300 wait:2 text:Agent click:690,555 wait:8
   ```
   A character name is letters only. Once one exists, Single Player opens the
   character list instead, and `click:690,555` (OK) enters with it.
4. **Drive and look**, as a loop: `input` to act, `shot` to see, the Read tool on
   the PNG to look at it. Every action is listed by
   `python AgentHarness\bhctl.py input --help`. Keep screenshots in your
   scratchpad, and use `shot --crop X,Y,W,H` to look closely at a window.
5. **Tear down**, every time, including after a failure:
   `python AgentHarness\bhctl.py teardown $id`.
6. **Report.** Send the screenshots that show the result to the user with
   SendUserFile. An in-game check the screenshots settle is reported as checked
   by you, with the screenshot. Anything they cannot settle, such as timing, feel
   or what only D2GL draws, stays on the user's list as not yet checked.

## What a run is like

- **Coordinates** are pixels in the game window's client area: 800x600 in the
  menus, 1344x700 in a game. Take a screenshot before aiming a click.
- **The cursor** is drawn into every screenshot, and hovering shows tooltips.
  `move:` it somewhere empty before a screenshot that should show neither.
- **Settings** are BH's shipped defaults (`Packaging/BH_settings.cfg`, which
  also lists the hotkeys) and the game's defaults, not the user's.
- **Keys** reach the game and BH as messages. Something read straight from the
  keyboard state, rather than from messages, does not see them; if a held key
  appears to do nothing, say so rather than concluding the feature is broken.
- **Leaving a game** is `key:esc` and Save and Exit. Closing the window only
  works from the menus.
- **Stopping a game** is `teardown`. The user's own clients are `Game.exe` too,
  so a game is never ended by name. A run also ends itself after an hour.
- **A launch that stops at a dialog** prints the dialog's text and leaves the
  run up for `status` and `shot`; tear it down when done.
