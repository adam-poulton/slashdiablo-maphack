The run tracker (added in BH 1.9.10) appends one row per game to a CSV file, so
you can review your runs afterwards in a spreadsheet. It records timing,
experience, kill counts and the items that dropped.

It is off by default.

## Settings

All of these live in `BH_settings.cfg`:

```
Run Details Ping Level: 4
Run Details On Join: True, None
Save Run Details: True, None
Save Run Details Location: ./data/%CHARNAME%.csv
```

* **Save Run Details** — write a row when you leave a game. This is the master
  switch; with it off nothing is written.
* **Run Details On Join** — show the previous run's summary when you join the
  next game.
* **Save Run Details Location** — where to write. The path is relative to your
  Diablo II directory and accepts the same `%VARIABLE%` tokens as everything
  else, so `./data/%CHARNAME%.csv` gives each character its own file. Missing
  directories are created.
* **Run Details Ping Level** — only items at or below this ping level are
  recorded in the `%DROPS%` column. Raising it records more of what dropped;
  lowering it keeps the column to the things you actually care about.

## Choosing columns

Each `Run Details[...]` line defines one column. The text in brackets is the
column heading, and the value is written into the cell:

```
Run Details[Date]: "%JOINDATE%"
Run Details[Run Name]: "%RUNNAME%"
Run Details[Run Length (sec)]: "%LASTGAMETIMESEC%"
Run Details[XP Gained]: "%LASTXPPERCENTGAINED%"
Run Details[Total Kills]: "%TOTALKILLED%"
Run Details[Drops]: " %DROPS%"
```

Any [automap variable](Automap-Info.md) can be used. Quote values that might
contain a comma — item names in `%DROPS%` certainly will — or the CSV will not
line up.

The header row is written only when the file is first created. If you change
your columns later, the existing file keeps its old header, so rename or move it
to start a fresh one.

Note that the "last game" variables are the right ones for most columns. A row
is written as you leave a game, at which point the run that just finished is the
"last" one, and `%LASTGAMETIMESEC%` and `%LASTXPPERCENTGAINED%` refer to it.
Kill counts are the exception — `%TOTALKILLED%` and friends still hold the
finishing values at that moment.

## Excluding items from the drop column

Add `%NOTRACK%` to an `ItemDisplay` rule to keep matching items out of the
`%DROPS%` column while still showing them normally in game:

    ItemDisplay[tsc]: %NAME%%NOTRACK%

Items that drop in town are never tracked, so vendor and stash activity does not
pollute the record.

## Notes

* If the file is open in Excel or another program that locks it, BH cannot
  append and will warn you rather than losing the row silently. Close the file
  and the next run writes normally.
* `%RUNNAME%` strips trailing digits from the game name, so `baal12`, `baal13`
  and so on group under a single `baal` run name — useful for filtering a
  session's worth of the same run.

## Related

* [Automap Info](Automap-Info.md) — the same statistics shown live on the automap
* [Advanced Item Display](Advanced-Item-Display.md) — `%NOTRACK%` and ping levels
