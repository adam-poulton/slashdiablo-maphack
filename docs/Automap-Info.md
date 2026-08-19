The automap overlay shows a configurable block of text in the corner of the
automap. Each line is one `AutomapInfo` entry in `BH_settings.cfg`, numbered
from 0:

```
AutomapInfo[0]:         Game: %GAMENAME%
AutomapInfo[1]:         Password: %GAMEPASS%
AutomapInfo[2]:         Area lvl: %AREALEVEL%
```

The numbers must be sequential with no gaps. If you comment a line out,
renumber the ones below it, or the remaining lines will not all display.

Any text you write is shown as-is; `%VARIABLE%` placeholders are substituted
each frame. Colors work the same way as elsewhere, so
`AutomapInfo[0]: %GREY%Area lvl: %WHITE%%AREALEVEL%` will do what you expect.

## Kill tracking (as of BH 1.9.10)

BH counts monsters that die near you and reports both raw counts and rates.
Kills are split three ways: **total**, **champion** packs, and **unique**
monsters (super uniques and bosses). A monster counted as a champion is not
also counted as a unique.

| Variable | Meaning |
| --- | --- |
| `%TOTALKILLED%` | Monsters killed this game |
| `%UNIQUEKILLED%` | Unique monsters killed this game |
| `%CHAMPKILLED%` | Champions killed this game |
| `%TOTALKILLSPERMIN%` | Total kills per minute this game |
| `%UNIQUEKILLSPERMIN%` | Unique kills per minute this game |
| `%CHAMPKILLSPERMIN%` | Champion kills per minute this game |
| `%TOTALKILLSPERSEC%` | Total kills per second this game |
| `%UNIQUEKILLSPERSEC%` | Unique kills per second this game |
| `%CHAMPKILLSPERSEC%` | Champion kills per second this game |

Each of these has a previous-game counterpart, so you can keep the last run's
numbers on screen while the current one is still in progress. The naming is not
consistent between the two groups: the raw counts use an underscore and the
rates do not.

| Variable | Meaning |
| --- | --- |
| `%LAST_TOTALKILLED%` | Monsters killed last game |
| `%LAST_UNIQUEKILLED%` | Unique monsters killed last game |
| `%LAST_CHAMPKILLED%` | Champions killed last game |
| `%LASTTOTALKILLSPERMIN%` | Total kills per minute last game |
| `%LASTUNIQUEKILLSPERMIN%` | Unique kills per minute last game |
| `%LASTCHAMPKILLSPERMIN%` | Champion kills per minute last game |
| `%LASTTOTALKILLSPERSEC%` | Total kills per second last game |
| `%LASTUNIQUEKILLSPERSEC%` | Unique kills per second last game |
| `%LASTCHAMPKILLSPERSEC%` | Champion kills per second last game |

A typical setup showing this game's kills with last game's for comparison:

```
AutomapInfo[10]:        Last (U/C/T): %LAST_UNIQUEKILLED% / %LAST_CHAMPKILLED% / %LAST_TOTALKILLED%
AutomapInfo[11]:        Kills (U/C/T): %UNIQUEKILLED% / %CHAMPKILLED% / %TOTALKILLED%
AutomapInfo[12]:        Kills/m (U/C/T): %UNIQUEKILLSPERMIN% / %CHAMPKILLSPERMIN% / %TOTALKILLSPERMIN%
```

### What gets counted

A monster is counted when BH has seen it alive and then sees it dead, within the
rooms loaded around you in the current act. Monsters that die far away, or that
were already dead before you arrived, are not counted. This means the numbers
track what you personally cleared rather than everything that died in the game.

A list of non-monster units — traps, summons, and similar things that would
otherwise inflate the count — is excluded. Rates are computed against elapsed
game time, so they start noisy and settle as a run goes on.

## Experience and pacing

| Variable | Meaning |
| --- | --- |
| `%CHARXP%` / `%CHARXPPERCENT%` | Experience, and percent through the level, **as of joining the game** |
| `%CHARLEVEL%` / `%CHARLEVELPERCENT%` | Character level and percent through it, as of joining the game |
| `%CURRENTCHARXP%` / `%CURRENTCHARXPPERCENT%` | Experience and percent through the level, right now |
| `%CURRENTCHARLEVEL%` / `%CURRENTCHARLEVELPERCENT%` | Character level and percent through it, right now |
| `%LASTXPGAINED%` | Raw experience gained last game |
| `%LASTXPPERCENTGAINED%` | Percent of a level gained last game |
| `%LASTXPPERSEC%` | Percent per second last game |
| `%LASTXPPERSECLONG%` | Same, at full precision |
| `%GAMESTOLVL%` | Estimated games remaining to level, based on the last game |
| `%TIMETOLVL%` | Estimated time remaining to level |
| `%LASTGAMETIME%` | Length of the last game, formatted |
| `%LASTGAMETIMESEC%` | Length of the last game in seconds |

`%GAMESTOLVL%` and `%TIMETOLVL%` extrapolate from your most recent run, so they
swing widely after an unusually fast or slow game, and they reset when you
switch characters.

## Game and session

| Variable | Meaning |
| --- | --- |
| `%GAMENAME%` / `%GAMEPASS%` / `%GAMEDESC%` | Current game name, password, description |
| `%RUNNAME%` | Game name with any trailing digits stripped, so `baal12` and `baal13` share the run name `baal` |
| `%GAMEDIFF%` | Current difficulty |
| `%GAMEIP%` | Game server IP |
| `%PING%` | Current ping |
| `%GAMETIME%` | Elapsed time in the current game |
| `%REALTIME%` | Wall clock time |
| `%JOINDATE%` / `%JOINTIME%` | When you joined the current game |
| `%SESSIONGAMECOUNT%` | Games played this session |
| `%AVGPLAYERCOUNT%` | Most frequently observed player count for the current game |
| `%AREALEVEL%` | Monster level of the area you are in |
| `%LEVEL%` | Name of the area you are in |
| `%CHARNAME%` / `%ACCOUNTNAME%` | Character and account name |
| `%DROPS%` | Items tracked as dropped this run |

`%AREALEVEL%` accounts for whether the character is classic or expansion, so it
reports the value that actually applies to your monsters.

## Overlay position

The overlay is drawn relative to the automap, using centering constants that
assume vanilla resolutions. On other resolutions it can sit noticeably off. Two
settings shift the native terrain and the BH icons together to correct this:

```
Automap Offset X: -36
Automap Offset Y: -4
```

Both default to `0`, and this only applies to 1.13c. The values above were found
to work for HD3 at 1344x700; other setups need their own numbers, found by
trial and error.

## Related

* [Run Tracker](Run-Tracker.md) — writing these same statistics to a file
* [Advanced Item Display](Advanced-Item-Display.md)
