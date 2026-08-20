Runeword Lookup
===============

An in-game window that lists every runeword recipe the game data defines, so you
do not need to alt-tab to a wiki to check which runes go into a word or what
base it can be made in.

The recipes are read from your `Runes.txt` and `ItemTypes.txt` in the game MPQ
archives when BH starts, so the list matches the runewords the realm actually
allows. `Runes.txt` also carries dozens of placeholder rows that were never
finished; those are skipped, leaving the 78 released runewords plus the
server-side additions listed below.

## Names that differ from the game files

Six recipes were renamed before release and the game files still carry the
working title. The window shows the released name:

| Name in `Runes.txt` | Shown as         |
| ------------------- | ---------------- |
| The Beast           | Beast            |
| Bound by Duty       | Chains of Honor  |
| Doomsayer           | Doom             |
| Exile's Path        | Exile            |
| Widowmaker          | Grief            |
| Winter              | Voice of Reason  |

## Server-side recipes

`Plague` (Cham + Fal + Um, any weapon) is enabled by the realm rather than by
the game files, so it is added to the list explicitly. If a future patch ships
it in `Runes.txt`, the file's version is used instead.

## Opening it

`Runeword Lookup: True, VK_NUMPAD9` in `BH_settings.cfg` enables the feature and
sets the hotkey. Defaults are on and numpad 9.

* Press the hotkey to open the window, and again to close it.
* **Escape closes it** while it is open, instead of opening the game menu. If
  the search box has focus, the first escape drops that focus and the second
  closes the window.
* Right-clicking the title bar also closes it.
* Drag the open window by its title bar. The position is remembered in `UI.ini`.
* Closed, it sits as a title bar with the other BH windows near the bottom of
  the screen. Ctrl-click it to reopen, shift-drag it to move it.
* `.rw` in chat also opens the window.

## Searching

Click the search box and type. While it has focus it fills in solid, gains a
second border and shows a blinking cursor; unfocused it is translucent with
dimmed text, so it is always clear where your typing is going.

The list filters as you type, matching on runeword name, rune name and item
type, so all of these work:

| Typing     | Shows                                              |
| ---------- | -------------------------------------------------- |
| `enig`     | Enigma                                             |
| `ber`      | every runeword that uses a Ber rune                |
| `shield`   | every runeword that can go in a shield             |
| `polearm`  | every runeword that can go in a polearm            |

Press escape to leave the search box, or clear it to list everything again.

Long lists are paged: `PgUp` and `PgDn`, or the `< Prev` and `Next >` links at
the bottom right, move between pages. The footer shows which entries you are
looking at.

## Chat commands

* `.rw` — open the lookup window.
* `.rw <search>` — same search as the box, and the first ten matches are also
  printed to the chat window so they stay in your message history.

`.runewords` works as a longer alias for `.rw`.

## Columns

| Column     | Contents                                                        |
| ---------- | --------------------------------------------------------------- |
| Runeword   | The runeword's name.                                            |
| Runes      | The runes in socket order. The number of runes is the number of sockets the base needs. |
| Item Types | The bases it can be made in, and any excluded types in brackets. |

Entries too long for their column are cut off with `..`; the full text is
printed by `.rw <search>` in chat.
