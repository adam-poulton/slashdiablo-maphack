The session tracker keeps track of a sitting on a character: how long you have
played, how many games, how much experience you have gained and how fast it is
coming, where you spent the time, and how often you died. It shows it in a
small HUD over the game and in a window of its own.

It is on by default, and it runs beside the [run tracker](Run-Tracker.md)
without changing it: the run tracker's variables, its settings and its CSV file
are exactly as they were.

## What a session is

A session belongs to one character in one game client. It starts the first time
you join a game on that character and carries on from game to game. Joining a
game on another character starts that character's own session, and coming back
picks the first one up where it left off.

Two clocks run through a session:

* **Session time** is the time since the session started, including the time
  you spend in the lobby between games.
* **Play time** is the time spent inside games only.

Rates are worked out over play time, so waiting for the next game does not drag
your experience per second down.

## The HUD

The HUD shows the session over the game, at the top of the screen until you
move it. It has two views:

* **Line** - one line: session time, the game's number and how long it has run,
  and experience per second over the session.
* **Compact** - three rows: session time and time played, the game's time and
  rate, and the session's rate beside the rate over the last minute.

The HUD takes no clicks. Clicking, dragging or scrolling on it does what it
would have done had the HUD not been there, so it cannot be moved or changed by
accident in a fight. To change it you ask:

* `.hud line` or `.hud compact` picks a view, as do the `HUD view` setting and
  the `Cycle HUD view` hotkey.
* `.hud move`, the `Position HUD` hotkey or `position HUD` in the session
  window lets you place it. The HUD is outlined: drag it anywhere, turn the
  mouse wheel over it to step through its views, and press Escape when done.

The HUD remembers the corner or edge it is nearest to, so one placed at the
bottom right stays at the bottom right in another view or at another
resolution. It is not drawn while one of the game's own panels (inventory,
character, stash and so on) covers its half of the screen.

## The session window

`.session`, or the `Session window` hotkey, opens a window on the session:

* **Overview** - the character, when the session started, session time and time
  played, games and deaths; experience gained, lost to deaths and per second,
  over the session, with lobby time counted, and over the last minute and five;
  levels gained and how long the next level is at this rate; and the game you
  are in.
* **Games** - every game of the session under the kind of run it was, with its
  length, experience, experience per second and level. A run is named for its
  games without the number they end in, so `baal-12` and `baal-13` are both
  `baal`, and a run with two finished games or more opens with what they
  average.
* **Kills** - the areas you fought in, with what died there, how long you
  spent and the experience earned; the kinds of monster killed; and every
  unique monster, super unique and act boss, with when it died and where.
* **Drops** - every drop the item filter pinged: when, the item in the colour
  it was announced in, where it fell, which game, and its ping tier.

The search box filters whichever tab is in front, by any text on the row: a
game's name, an area, a monster, an item.

`.session new`, or the `New session` hotkey, ends the session and starts
another from the game you are in.

### What counts as a kill

A monster counts when BH saw it alive and then saw it dead. The game only tells
your client about monsters near you, and never says who landed the blow, so
these are monsters that died in view: yours and your party's alike, and not the
ones that died across the map. Experience is the exact measure of what you
earned.

Townspeople, mercenaries, summons, walls, doors and bone prisons are left out,
by what the game's own monster table says of them, and so are the tentacles and
the clone Baal throws up during his fight.

### What counts as a drop

An item on the ground that your item filter pings: shown on the map, at or
under your `Ping tiers <=` setting. An item is one drop however often it is
pinged. As with the run tracker, items in a town are left out, and so is
anything a rule marks `%NOTRACK%`. The run tracker's own
`Run Details Ping Level` has no say here, nor this in that.

## Automap variables

The session's figures are also [automap variables](Automap-Info.md), so they
can go on the automap and in run tracker columns like any other:

```
AutomapInfo[18]:        Session: %SESSIONTIME% (%SESSIONPLAYTIME% played)
AutomapInfo[19]:        XP: %SESSIONXPPERSEC% (now %RECENTXPPERSEC%)
```

| Variable | Meaning |
| --- | --- |
| `%SESSIONTIME%` | Time since the session started |
| `%SESSIONPLAYTIME%` | Time spent in games this session |
| `%SESSIONXPGAINED%` | Experience gained this session, less any lost to deaths |
| `%SESSIONXPPERSEC%` | Experience per second over the session's play time |
| `%SESSIONXPPERSECLONG%` | Same, at full precision |
| `%RECENTXPPERSEC%` | Experience per second over the last minute of play |
| `%SESSIONLEVELS%` | Levels gained this session |
| `%SESSIONDEATHS%` | Deaths this session |
| `%SESSIONKILLS%` | Monsters that died in view this session |
| `%SESSIONDROPS%` | Drops the item filter pinged this session |

## Settings

Under `Session` on the Display tab of the settings window:

```
Session Tracker: True
Keep Session History: True
Session HUD: True
Session HUD View: 0
Session HUD Background: 1
```

* **Session tracker** - turns the tracker on. With it off nothing is tracked,
  the HUD is not drawn and the variables above are empty.
* **Keep session history** - writes each session to a file of its own.
* **Session HUD** - shows the HUD.
* **HUD view** - `Line` (0) or `Compact` (1).
* **HUD background** - what the HUD's text is drawn on: `None` (0),
  `Translucent` (1) or `Solid` (2).

And the hotkeys, under `Session` and `Panel hotkeys` on the Input tab, none of
them bound until you bind them:

```
Show Session: True, None
Session HUD Cycle View: None
Session HUD Position: None
Session New: None
```

Where the HUD was placed is kept in `UI.ini`, with the windows' positions.

## Session files

With history kept, each session is written to the `BH_sessions` folder beside
`BH_settings.cfg`, in a file named for when it started, the character and the
client that wrote it:

```
BH_sessions/20261127-224014-Sorc_One-21672.jsonl
```

The file is a log: one line of JSON for each thing that happened, added as it
happens, so a session is on disk even if the game crashes. `t` is milliseconds
since the session started and `e` is the kind of event.

```
{"t":0,"e":"session","v":1,"start":1795779614,"account":"...","realm":"...","char":"Sorc_One","class":1,"hardcore":false,"expansion":true,"bh":"1.10.1"}
{"t":0,"e":"join","game":"baal-12","diff":2,"level":87,"exp":2097310703}
{"t":0,"e":"area","area":109,"town":true,"name":"Harrogath"}
{"t":0,"e":"players","count":8}
{"t":41000,"e":"area","area":131,"town":false,"name":"Throne of Destruction"}
{"t":64000,"e":"exp","exp":2097999999}
{"t":64000,"e":"kills","area":131,"class":571,"name":"Minion of Destruction","tier":"normal","n":4}
{"t":64100,"e":"level","level":88}
{"t":66000,"e":"notable","area":131,"class":571,"name":"Lister the Tormentor","kind":"Minion of Destruction","tier":"superunique","su":65}
{"t":67000,"e":"drop","id":77,"name":"Ist Rune","code":"r24","quality":2,"color":8,"ping":1,"area":131,"areaName":"Throne of Destruction"}
{"t":70000,"e":"death"}
{"t":91000,"e":"leave"}
```

| Event | Says |
| --- | --- |
| `session` | Who the session belongs to and when it started (`start`, seconds since 1970) |
| `join` | A game was joined: its name, difficulty (0 Normal, 1 Nightmare, 2 Hell), and the level and experience you came in with |
| `leave` | The game was left |
| `area` | You entered an area (its number in `Levels.txt`), its name, and whether it is a town |
| `players` | The number of players in the game changed |
| `exp` | Your experience total changed. Written at most once a second while it is moving |
| `level` | You reached a level |
| `death` | You died |
| `kills` | `n` monsters of one kind (`class`, their row in `MonStats.txt`) and one `tier` (`normal`, `minion` or `champion`) died in view in an area. Written at most once a second |
| `notable` | One unique monster died in view: its own `name`, the `kind` of monster it is, its `tier` (`unique`, `superunique` or `boss`), and for a super unique its row in `SuperUniques.txt` as `su` |
| `drop` | The item filter pinged an item: its unit `id`, name, item `code`, `quality`, the text `color` it was announced in, the `ping` tier of the rule, and where it fell |

A tool reading these files should skip any line or kind of event it does not
know, as later versions will add more.

## Related

* [Automap Info](Automap-Info.md) - every variable that can be shown
* [Run Tracker](Run-Tracker.md) - one CSV row per game
* [Chat Commands](Chat-Commands.md) - `.session` and `.hud` among the rest
