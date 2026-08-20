Info Window
===========

An in-game reference window. It is a tabbed panel, so lookups that would
otherwise mean alt-tabbing to a wiki can live in the game. Today it has one tab,
**Runewords**.

## Opening it

`Info Window: True, VK_NUMPAD9` in `BH_settings.cfg` enables the window and sets
the hotkey. Defaults are on and numpad 9.

* Press the hotkey to open the window, and again to close it.
* **Escape closes it** while it is open, instead of opening the game menu. If a
  search box has focus, the first escape drops that focus and the second closes
  the window.
* Right-clicking the title bar also closes it.
* Drag the open window by its title bar. The position is remembered in `UI.ini`.
* Closed, it sits as a title bar with the other BH windows near the bottom of
  the screen. Ctrl-click it to reopen, shift-drag it to move it.
* `.info` in chat opens it.

## Runewords tab

Every runeword recipe the game allows, searchable.

The recipes are read from your `Runes.txt` and `ItemTypes.txt` in the game MPQ
archives when BH starts, so the list matches what the realm actually allows.
`Runes.txt` also carries dozens of placeholder rows that were never finished;
those are skipped, leaving the 78 released runewords plus the server-side
additions listed below.

### Searching

Click the search box and type. The list filters as you type, matching on
runeword name, rune name and item type, so all of these work:

| Typing     | Shows                                              |
| ---------- | -------------------------------------------------- |
| `enig`     | Enigma                                             |
| `ber`      | every runeword that uses a Ber rune                |
| `shield`   | every runeword that can go in a shield             |
| `polearm`  | every runeword that can go in a polearm            |

While the box has focus it fills in solid, gains a second border and shows a
blinking cursor; unfocused it is translucent and shows a dimmed hint, so it is
always clear where your typing is going. Press escape to leave the box, or clear
it to list everything again.

Long lists are paged: `PgUp` and `PgDn`, or the `< Prev` and `Next >` links at
the bottom right. The footer shows which entries you are looking at.

### Chat command

`.rw <search>` opens the window on the Runewords tab with that search already
applied. Matches are shown in the window only, not repeated into the chat log.
`.runewords` is a longer alias.

### Columns

| Column     | Contents                                                        |
| ---------- | --------------------------------------------------------------- |
| Runeword   | The runeword's name.                                            |
| Runes      | The runes in socket order. The number of runes is the number of sockets the base needs. |
| Item Types | The bases it can be made in, and any excluded types in brackets. |

Entries too long for their column are cut off with `..`.

### Names that differ from the game files

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

### Server-side recipes

`Plague` (Cham + Fal + Um, any weapon) is enabled by the realm rather than by
the game files, so it is added to the list explicitly. If a future patch ships
it in `Runes.txt`, the file's version is used instead.
