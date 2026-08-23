Chat Commands
=============

BH answers chat commands typed with a leading dot, in game and in the chat
channel. `.help` lists them.

## Finding them

`.help` prints every command BH answers, a line per module:

```
BH: .help (.commands) .reload .save
Info: .info .rw (.runewords) .uni (.uniques) .set (.sets)
Info: .cube (.recipe, .recipes)
```

A command in brackets is another name for the one in front of it, not a command
of its own, so `.recipe` and `.recipes` both open what `.cube` opens.

The game keeps a command list of its own, which is what it shows when you mistype
one - `commands: .claim`. BH's commands are not in it and cannot be added to it:
the list, the dispatch and that help text all live inside `D2Client.dll`, with no
table to register against. So BH answers for its own instead, and a command BH
does not recognise is met with a pointer to `.help`.

BH sees every command before the game does, and has no way to ask the game
whether a command is one of its own, so the commands the game answers are named
in BH rather than discovered. `.claim` is the one the game advertises. A realm
that adds more of its own would need adding to `kGameCommands` in
[`BH/Modules/ModuleManager.cpp`](../BH/Modules/ModuleManager.cpp), or typing one
would be answered with a pointer to `.help` on top of the command working
perfectly well.

Commands only print in game. BH's printing goes through the game's chat log,
which does not exist in the chat channel, so `.help` typed there is swallowed
without an answer.

## BH

| Command | Also | What it does |
| --- | --- | --- |
| `.help` | `.commands` | Lists every command BH answers. |
| `.reload` | | Rereads `BH.cfg` and `BH_settings.cfg` from disk. |
| `.save` | | Writes the current settings back to `BH_settings.cfg`. |

## Info

The [Info window](Info-Window.md), and each of its tabs. Every one of these takes
an optional search, so `.uni griffon` opens the Uniques tab with `griffon`
already typed in its search box.

| Command | Also | What it does |
| --- | --- | --- |
| `.info` | | Opens the window on whichever tab was last in front. |
| `.rw` | `.runewords` | Opens the Runewords tab. |
| `.uni` | `.uniques` | Opens the Uniques tab. |
| `.set` | `.sets` | Opens the Sets tab. |
| `.cube` | `.recipe`, `.recipes` | Opens the Recipes tab. |

## Adding one

A module lists the commands it answers from `GetCommands()`, and reads which one
was typed from `GetInvokedCommand()` while handling the input. Listing them is
what puts them in `.help`; there is nowhere else they are written down, so a
command left off the list is a command nobody can find.

Each entry is a `ChatCommand`: the name to print, and the other names that reach
the same command. Matching searches all of them, so an alias costs nothing beyond
saying that is what it is.
