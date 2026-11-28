The race timer times a run against a **route**: a list of splits, each reached
by something happening in the game. BH watches for those things and marks the
splits itself, and sets the run beside the best you have run that route. It
shows the run in a small HUD over the game and in a window of its own.

It is a tool of its own. It keeps nothing with the
[session tracker](Session-Tracker.md), and each works with the other switched
off.

## Timing a run

Type `.race`, or press the `Race window` hotkey. The window is one screen: the
routes on the left, and the one you click on the right with `Start` and `Stop`
over its splits. Click a route and press `Start`. `Stop` gives the run up.

The route you click is the route runs are timed on, for the hotkeys and the
chat commands as well as the buttons, and it is remembered.

Tick `Start a run when a new character joins its first game`, under the
splits, and a new character (level 1, no experience) starts a run on that route
by itself, which is where the speedrun leaderboards start their timer. It is
off to begin with, so nothing is timed that you did not start.

A run carries on from game to game until its last split is reached or you stop
it. It belongs to the character it was started on: play another character and
it waits, and carries on when you come back. It does not carry over a restart
of the game client.

Everything the buttons do can be done from chat or a hotkey:

| Command | Hotkey | What it does |
| --- | --- | --- |
| `.run start` | `Start run` | Starts a run on the route picked, in place of any being timed |
| `.run stop` | `Stop run` | Gives the run up |
| `.run split` | `Split` | Marks the split the run is on as reached |
| `.run undo` | `Undo split` | Takes back the split last reached |
| `.run skip` | `Skip split` | Passes over the split the run is on |
| `.route <name>` | | Picks a route, by its id or its name. With no name it lists them |

## The HUD

The race HUD shows the run over the game, at the right of the screen until you
move it. It is drawn while a run is being timed, and for a minute after one is
finished. With no run it draws nothing. It has two views:

* **Splits** - the route's name, then the run split by split: when each was
  reached and how far ahead or behind your best run, the time to beat for the
  ones still to come, and the run's clock over your best time. A long route
  shows the eight splits around the one you are on.
* **Line** - one line: the split the run is on, the run's clock, and how the
  last split stood against your best.

The HUD takes no clicks. Clicking, dragging or scrolling on it does what it
would have done had the HUD not been there, so it cannot be moved or changed by
accident in a fight. To change it you ask:

* `.race hud splits` or `.race hud line` picks a view, as do the `HUD view`
  setting and the `Cycle HUD view` hotkey.
* `.race hud move`, the `Position HUD` hotkey or `position HUD` in the race
  window lets you place it. The HUD is outlined: drag it anywhere, turn the
  mouse wheel over it to step through its views, and press Escape when done.
  With no run going it shows the route picked as it stands before a run, so
  there is something to take hold of.

The HUD remembers the corner or edge it is nearest to, so one placed at the
bottom right stays at the bottom right in another view or at another
resolution. It is not drawn while one of the game's own panels (inventory,
character, stash and so on) covers its half of the screen.

## The race window

`.race`, or the `Race window` hotkey, opens it. It has two parts with a rule
between them.

**The routes**, on the left. Click one to pick it: it is white on a band, and
the rest are gold. A route with a run going on it is marked `live`. Under the
list is what can be done to the routes: `New` makes one, `Copy` makes one from
the route picked, `Edit` changes the route picked, and `Delete` asks to be
clicked again. Once one of the routes BH comes with is gone, `Restore presets`
returns the missing ones and leaves the rest alone.

**The route picked**, on the right. Its name, with `Start` and `Stop` against
the far edge, over its splits:

* While a run is going on it, the run: when each split was reached, how far
  ahead or behind your best run, the time to beat, the best that stretch has
  ever been done, and the level you reached it at. The split the run is on is
  marked `>`. Under the splits are the run's time, your best run, the sum of
  your best stretches, and the best time the run could still come to.
* With no run going on it, the route as it stands before a run, with its times
  to beat.
* Under either, the runs on this route you have stopped or finished since the
  game client started, latest first.

`Start` is greyed out while the route has no splits or you are not in a game,
and `Stop` until a run is being timed. Pick another route while a run is going
and the right side shows that route; the footer says where the run is, and
`Start` would start a new run on the route picked in place of it.

### Changing a route

`Edit`, `New` and `Copy` swap the screen for the route's own, until `Done`:

* **The route**, on the left. Its name is in the box at the top: click in it
  and type to rename the route. Enter, or a click anywhere else, keeps the
  name, and Escape gives it up. Two routes cannot have one name. A route just
  made opens with its name ready to type over. Under the name are the route's
  splits in the order they are run in. Click one to pick it out, and click it
  again to let go of it. `Move up`, `Move down` and `Remove`, under the list,
  act on the split picked out.
* **The splits there are to add**, on the right: one for each act boss, quest
  and quest item, the Cow King and every quest done, under Normal, Nightmare
  and Hell. Click one to add it after the split picked out, or at the end of
  the route where none is. A click on a difficulty folds it away or opens it.
  Type in the box over them to narrow them, by name, by difficulty or by what
  marks them. Type `level 24` there to be offered a split for reaching that
  level.

Every change is written to `BH_routes.json` as it is made. A run being timed
keeps the splits it started with. A split for an area or a particular monster,
one that wants several things at once, or one you mark by hand is written in
the file, as below; the window shows such a split and can move or remove it.
If BH cannot read the file, the window leaves it alone and says so, and the
routes BH comes with stand in until the file is mended.

## Routes

BH comes with these, and writes them to `BH_routes.json` beside
`BH_settings.cfg` the first time it runs. Change them or add your own in the
race window, or in the file itself:

| Id | Route |
| --- | --- |
| `bosses-normal` | Andariel, Duriel, Mephisto, Diablo and Baal in Normal |
| `bosses-hell` | The same five in Normal, Nightmare and Hell: fifteen splits |
| `milestones-normal` | The bosses and the steps between them: Radament, the Horadric Cube, the staff, the amulet, the Summoner, Khalim's Will, Travincal, Izual, the Hellforge, Shenk, Anya and the Ancients |
| `act1` to `act5` | One act's boss alone, as the per-act leaderboards time it |
| `completionist-normal` | Every quest, the Cow King and level 40, in any order |

### The routes file

A split is reached when any one of the things under its `when` happens, or
every one of them where the split says `"all": true`:

```
{
  "id": "mine",
  "name": "My route",
  "splits": [
    { "name": "Andariel", "difficulty": "normal",
      "when": [ { "boss": 156 }, { "quest": 6, "mask": 3 } ] },
    { "name": "Cube", "when": [ { "item": "box" } ] },
    { "name": "Level 24", "when": [ { "level": 24 } ] },
    { "name": "Something only I can judge" }
  ]
}
```

| In `when` | The split is reached when |
| --- | --- |
| `{"boss": N}` | A monster of that kind dies in view: its row in `MonStats.txt` |
| `{"superunique": N}` | That super unique dies in view: its row in `SuperUniques.txt` |
| `{"quest": N, "mask": M}` | Any of those bits of the quest's state is set. Without a mask, the quest is done |
| `{"area": N}` | That area is entered: its number in `Levels.txt` |
| `{"level": N}` | The character reaches that level |
| `{"item": "code"}` | An item with that code comes into the character's keeping |
| `{"allQuests": true}` | Every quest of the difficulty is done |

`difficulty` (`normal`, `nightmare` or `hell`) ties a split to things that
happen in that difficulty; without it any will do. A split with no `when` is one
you mark yourself with `.run split`.

The act bosses are given both their death and their quest, because the game
only tells your client about monsters near you: a boss your party kills across
the map is caught by the quest a moment later. Splits are reached in whatever
order the game reaches them, and one that was already so when the run began (a
level already passed) is passed over.

## Your best

Each run is compared with the fastest you have finished its route: at every
split the HUD and the race window show how far ahead (green) or behind (red) you
are, and mark in gold a stretch between two splits done faster than you have
ever done it.

Bests are kept in `BH_runs.json` beside `BH_settings.cfg`, apart for each
route, each character class, and softcore and hardcore. A run you stop still
counts as an attempt and still sets a best stretch.

A best belongs to the splits it was run on. Change a route's splits and its
best is no longer compared with; the first run on the changed route starts its
bests over. Renaming a route changes nothing.

`Compare runs on` picks the clock: `Time in game`, the one chosen to begin
with, leaves out time spent between games, which is how the leaderboards time
a run. `Real time` counts everything since the run began. Both are always kept,
so changing the setting loses nothing.

## Automap variables

The run's figures are also [automap variables](Automap-Info.md), so they can go
on the automap and in run tracker columns like any other. Both are empty while
no run is being timed:

| Variable | Meaning |
| --- | --- |
| `%RUNTIME%` | The clock of the run being timed |
| `%RUNSPLIT%` | The split the run is on |

## Settings

Under `Race` on the Display tab of the settings window:

```
Race Timer: True
Race HUD: True
Race HUD View: 1
Race HUD Background: 1
Race Auto Start: False
Race Route: bosses-normal
Race Compare Clock: 1
```

* **Race timer** - turns the timer on. With it off no run is timed, the HUD is
  not drawn and the variables above are empty.
* **Race HUD** - shows the HUD while there is a run to show.
* **HUD view** - `Line` (0) or `Splits` (1).
* **HUD background** - what the HUD's text is drawn on: `None` (0),
  `Translucent` (1) or `Solid` (2).
* **Start a run with a new character** - times a run on the route picked from
  a new character's first game. It is the box under the splits in the race
  window.
* **Route** - the id of the route runs are timed on: the one picked in the race
  window. Where it names no route, the first one is picked.
* **Compare runs on** - `Real time` (0) or `Time in game` (1).

And the hotkeys, under `Race` and `Panel hotkeys` on the Input tab, none of
them bound until you bind them:

```
Show Race: True, None
Race Run Start: None
Race Run Stop: None
Race Run Split: None
Race Run Undo: None
Race Run Skip: None
Race HUD Cycle View: None
Race HUD Position: None
```

Where the HUD was placed is kept in `UI.ini`, with the windows' positions.

## Related

* [Session Tracker](Session-Tracker.md) - time played, games, experience, kills and drops
* [Automap Info](Automap-Info.md) - every variable that can be shown
* [Chat Commands](Chat-Commands.md) - `.race`, `.run` and `.route` among the rest
