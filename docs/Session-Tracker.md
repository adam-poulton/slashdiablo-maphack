The session tracker keeps track of a sitting on a character: how long you have
played, how many games, how much experience you have gained and how fast it is
coming, where you spent the time, and how often you died.

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

## Showing it

The session's figures are [automap variables](Automap-Info.md), so they can go
on the automap and in run tracker columns like any other:

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

## Settings

Both are under `Session` on the Display tab of the settings window:

```
Session Tracker: True
Keep Session History: True
```

* **Session tracker** - turns the tracker on. With it off nothing is tracked and
  the variables above are empty.
* **Keep session history** - writes each session to a file of its own.

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
{"t":0,"e":"area","area":109,"town":true}
{"t":0,"e":"players","count":8}
{"t":64000,"e":"exp","exp":2097999999}
{"t":64100,"e":"level","level":88}
{"t":70000,"e":"death"}
{"t":91000,"e":"leave"}
```

| Event | Says |
| --- | --- |
| `session` | Who the session belongs to and when it started (`start`, seconds since 1970) |
| `join` | A game was joined: its name, difficulty (0 Normal, 1 Nightmare, 2 Hell), and the level and experience you came in with |
| `leave` | The game was left |
| `area` | You entered an area (its number in `Levels.txt`), and whether it is a town |
| `players` | The number of players in the game changed |
| `exp` | Your experience total changed. Written at most once a second while it is moving |
| `level` | You reached a level |
| `death` | You died |

A tool reading these files should skip any line or kind of event it does not
know, as later versions will add more.

## Related

* [Automap Info](Automap-Info.md) - every variable that can be shown
* [Run Tracker](Run-Tracker.md) - one CSV row per game
