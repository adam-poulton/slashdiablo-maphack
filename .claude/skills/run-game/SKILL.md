---
name: run-game
description: Run Diablo II with a BH build yourself, hidden and sandboxed, to check a change in game, see or measure how BH's windows and overlays are drawn, under the game's own renderer or under D2GL as players see it, or reproduce something that only happens in a running game. Writes a report with screenshots as evidence. For putting a build in front of the user to test, use deploy-dev-bh instead.
---

# Run the game

`AgentHarness/bhctl.py` runs single player Diablo II in a sandbox on a hidden
desktop: nothing appears on the user's screen, and nothing reaches their installs,
saves or settings. The game runs the agent build of BH, which says what is on
screen, records everything BH draws, and photographs what D2GL presents. ADRs
0011 to 0015 are why it works this way; `AgentHarness/README.md` is the
reference for every command and the scenario format.

## Steps

1. **Check the machine** when a first command fails, or on a machine the
   harness has not run on: `python AgentHarness\bhctl.py doctor` lists what is
   missing and the command or variable that supplies it. The game install it
   copies from is `BHAGENT_GAME_SOURCE`, or else the one the registry names;
   never write a path into the harness.
2. **Build** the agent build, Release with the driver in it:
   `/t:BH /p:Configuration=Release /p:BHAgent=true`, giving `AgentRelease\BH.dll`.
   If `AgentHarness\bin\Release\AgentLaunch.exe` is missing, also `/t:AgentLaunch`.
   MSBuild is found with vswhere, as in the README.
3. **Write a scenario** for what the change should do: each claim a `check`, on
   `state` for what the game is doing, on `drawn` for what BH shows and exactly
   where, and on `item` for what lies on the ground and the name the item filter
   gave it. A `drop` step lands an item the item capture recorded
   (`BHTests\fixtures\filter-cases.txt` has 1351). A change that needs a
   particular character plays a `character` save from `AgentHarness\fixtures\`;
   when none there fits, ask the user for one rather than making do. Put the
   scenario in your scratchpad, or in `AgentHarness\scenarios\` if it is worth
   running again: `smoke.json` and `item-drops.json` there are worked examples.
   When you do not yet know what to check, look first (below).
4. **Run it**: `python AgentHarness\bhctl.py scenario FILE.json`. It tears its
   run down whatever happens and prints where the report is. When the change
   is one a player would see, run it again with `--renderer d2gl` (below).
5. **Read the report**: `report.md`, then the marked screenshots of each check
   with the Read tool. A failed check lists what BH drew instead, and the
   nearest text outlined in yellow. Fix the code or the scenario, rebuild, and
   run again until every check holds and the screenshots look right, which no
   check settles alone: alignment, clipping, overlap and colour are judged by eye.
6. **Report**: send `report.md` and the screenshots that show the result with
   SendUserFile. An in-game check the report settles is reported as checked by
   you, with its evidence and the renderer it ran under. Anything it cannot
   settle, such as timing, feel or a realm game, stays on the user's list as
   not yet checked.

## Which renderer

- **GDI**, the default, is the game's own renderer, identical on every machine.
  Use it for what BH draws and where.
- **D2GL** (`--renderer d2gl`, or `"renderer": "d2gl"` in the scenario) is what
  Slash Diablo players look at. Use it for how a change looks to them, and for
  anything D2GL has a hand in: scaling, HD text, the HD cursor, the minimap.
  D2GL's features are off, as its defaults have them, until the scenario turns
  them on: `"d2gl": {"Feature.hd_text": "true"}`. `"window": [1612, 840]`
  scales the game up as a player's window does; without it the window is the
  in-game resolution and a screenshot's pixels are the game's.
- A scenario runs under both unless it checks an exact `rect` of text: with HD
  text on, text is a pixel or two wider or narrower than under GDI. Check such
  text by `text` and `inside`.

## Looking first

To explore before writing checks, drive a run by hand:

```powershell
$id = python AgentHarness\bhctl.py create     # --setting "KEY=VALUE", --character FILE.d2s, --renderer d2gl
python AgentHarness\bhctl.py launch $id       # returns at the main menu
python AgentHarness\bhctl.py enter $id        # into a game in Normal, or --difficulty
```

then loop on `state`, `drawlog --out FILE`, `shot FILE.png`, `input` and `drop`, keeping
the files in your scratchpad, and finish with `teardown $id`, every time,
including after a failure. `scenario --keep` leaves a scenario's run up for the
same.

## What a run is like

- **Coordinates** are pixels of the canvas the game draws on: 800x600 in the
  menus, 1344x700 in a game. Clicks, crops, checks and a draw log's rectangles
  are all in them, under either renderer. A D2GL screenshot is the whole
  window, with the canvas scaled into it and centred, so measure positions from
  the draw log and not from such a picture.
- **The cursor** is drawn into every screenshot, and hovering shows tooltips.
  `move:` it somewhere empty first when the picture should show neither.
- **The keyboard** behaves as a real one: `text:` types into the game's boxes
  and BH's alike, and held keys (Alt for ground items, Shift and Ctrl for BH's
  shortcuts) are seen as held, but only within one `input` call or `do` step,
  which releases them at its end. A check that needs a key held says `hold`.
- **Settings** are BH's shipped defaults (`Packaging/BH_settings.cfg`, which
  also lists the hotkeys), the game's defaults and D2GL's, not the user's, and
  the item filter is the shipped `Packaging/BH.cfg`.
- **BH's windows** start collapsed into bars at the bottom left. Ctrl+click a
  bar to open its window (`down:ctrl`, `click:`, `up:ctrl`); numpad 8 also opens
  Settings.
- **The world** is the Rogue Encampment, with nothing on the ground until a
  drop lands there. A dropped item exists only for the client: clicking it
  does nothing, and the filter's own rules decide whether it shows, by the
  character's level among other things.
- **What the draw log holds** is everything BH draws, and nothing the game or
  D2GL draws itself. A `frame` is the game's ornate border art, logged as the
  pixels it covers: one past each side of the box BH framed.
- **Stopping a game** is `teardown`, or the end of a scenario. The user's own
  clients are `Game.exe` too, so a game is never ended by name. A run also ends
  itself after an hour.
- **A command that stops at a dialog** prints the dialog's text and leaves the
  run up for `status` and `shot`; tear it down when done.
