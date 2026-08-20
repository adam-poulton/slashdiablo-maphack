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
always clear where your typing is going. Clear it to list everything again.

Press **enter** to open the first match in the detail pane. Enter is not typed
into the box, so the search text is left alone.

Long lists are paged: `PgUp` and `PgDn`, or the `< Prev` and `Next >` links at
the bottom right, which only appear when there is more than one page. Paging
stops at the first and last page rather than wrapping round, and a link that
would do nothing is greyed out. The footer shows which entries you are looking
at, or just how many matched when they all fit on one page.

### Detail view

Click any row, or press enter to take the first match, and the list is replaced
by that recipe in full: the runes untruncated, the character level it requires
(the highest level requirement among its runes), which bases it can go in, and
its stats. It is laid out like the description on an item — centred text inside a
border drawn to fit it — and long lines are wrapped rather than cut.

The stats are read from the same tables and string files the game itself uses to
describe an item, so the wording matches what you would see on the finished
item, in whatever language your client is installed in.

Stats from more than one source are added together, the way the game adds them
up on the finished item. Infinity gets its crushing blow from two Ber runes and
shows the total, `40% Chance of Crushing Blow`; Last Wish has it on the runeword
*and* on a Ber and shows `60-70%`.

**Stats depend on the base.** A runeword grants its own bonuses, and on top of
those every rune contributes its own, which differ depending on whether it is
socketed into a weapon, a helm or body armour, or a shield. Lines that come out
the same whatever the runeword is made in are listed plainly; only the ones that
differ are tagged with the base they belong to. So a runeword allowed in one
kind of base, like Enigma, has no tags at all, while Spirit reads:

```
+2 to All Skills
+25-35% Faster Cast Rate
...
+75 poison damage over 5 seconds  (Sword)
+3-14 Cold Damage                 (Sword)
Poison Resist +35%                (Any Shield)
Cold Resist +35%                  (Any Shield)
```

The tagged lines come from Tal, Thul, Ort and Amn behaving differently in the
two bases; everything above them is the same either way.

Escape backs out one step at a time: out of the search box, then back to the
list, then it closes the window.

### Chat command

`.rw <search>` opens the window on the Runewords tab with that search already
applied. Matches are shown in the window only, not repeated into the chat log.
`.runewords` is a longer alias.

### Columns

| Column   | Contents                                                          |
| -------- | ----------------------------------------------------------------- |
| Runeword | The runeword's name, in gold, turning white as you point at it to show it opens. |
| Runes    | The runes in socket order. The number of runes is the number of sockets the base needs. |

Which bases a runeword can go in is in its detail view rather than the list, but
searching still matches on it, so `shield` still finds every runeword that can be
made in one. A name too long for its column is cut off with `..`.

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
the game files, so both the recipe and its bonuses are listed in the source, in
[RunewordTab.cpp](../BH/Modules/Info/RunewordTab.cpp). What its runes contribute
still comes from the game's own data, so only the runeword's own bonuses are
written out. If a future patch ships it in `Runes.txt`, the file's version is
used instead.
