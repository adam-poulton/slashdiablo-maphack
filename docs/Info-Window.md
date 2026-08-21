Info Window
===========

An in-game reference window. It is a tabbed panel, so lookups that would
otherwise mean alt-tabbing to a wiki can live in the game. It has two tabs,
**Runewords** and **Uniques**. Click a tab to switch to it, or use the chat
command that belongs to it.

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
* `.info` in chat opens it on whichever tab was last in front.

Closing the window clears where you had got to, so it opens on a clean list
rather than on the last thing you searched for.

## Colours

Item names are drawn in the colour their rarity gives them in the game, so a
unique reads gold and a rune orange, the same as they would in your inventory.
Two things the window shows are not item qualities but are still drawn as a
rarity: a runeword takes the gold of the item it makes, and a rune the orange the
game gives it. Both are classified as rarities in
[ItemRarity.h](../BH/ItemRarity.h) rather than coloured by hand wherever they
appear, so anything added later reads the same way.

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

Clicking into the box empties whatever was in it, so a new search is just click
and type rather than backspacing over the old one. That also puts the whole list
back, and leaves a runeword you were reading.

While the box has focus it fills in solid, gains a second border and shows a
blinking cursor; unfocused it is translucent and shows a dimmed hint, so it is
always clear where your typing is going.

Press **enter** to select the first match, which puts its summary up. Enter is
not typed into the box, so the search text is left alone.

Long lists scroll: the mouse wheel, the scrollbar in the list's right hand
gutter, or `PgUp`/`PgDn`, `Home` and `End` on the keyboard. The footer shows
which entries you are looking at, or just how many matched when they all fit.

### Summary panel

Point at any row and that recipe is described in full in a panel beside the
window: the runes untruncated, the character level it requires (the highest level
requirement among its runes), which bases it can go in, and its stats. It is laid
out like the description on an item - centred text inside a border drawn to fit
it - and long lines are wrapped rather than cut. The arrow keys move the
selection, which the panel follows when the mouse is elsewhere.

The stats are read from the same tables and string files the game itself uses to
describe an item, so the wording matches what you would see on the finished
item, in whatever language your client is installed in.

Stats from more than one source are added together, the way the game adds them
up on the finished item. Infinity gets its crushing blow from two Ber runes and
shows the total, `40% Chance of Crushing Blow`; Last Wish has it on the runeword
*and* on a Ber and shows `60-70%`.

**The lines are in the game's own order.** Item tooltips are not ordered by when
a stat was rolled; every stat carries a `descpriority` in `ItemStatCost.txt` and
the game shows them highest first. The same number is used here, so Spirit reads
all skills, then faster cast rate, then faster hit recovery, down to magic
absorb, exactly as it does on the shield.

**Stats the game shows as one line are shown as one line.** The four resistances
collapse into `All Resistances +30`, and the four attributes into
`+15 to all Attributes`, but only when every one of them is granted at the same
value - which is the condition the game itself applies, so an item with three
resistances at +20 and the fourth at +30 still lists all four. Which stats group
this way, and how the combined line reads, come from the `dgrp` columns of
`ItemStatCost.txt` rather than from a rule written into BH, so a realm that adds
a group gets it for free.

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

Escape backs out one step at a time: out of the search box, then it closes the
window.

### Chat command

`.rw <search>` opens the window on the Runewords tab with that search already
applied. Matches are shown in the window only, not repeated into the chat log.
`.runewords` is a longer alias.

### Columns

| Column   | Contents                                                          |
| -------- | ----------------------------------------------------------------- |
| Runeword | The runeword's name, in the gold the game draws the item it makes in, turning white as you point at it. |
| Runes    | The runes in socket order, in the orange the game draws a rune in. The number of runes is the number of sockets the base needs. |

Which bases a runeword can go in is in its summary rather than the list, but
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

## Uniques tab

Every unique item the game allows, searchable, with what it rolls.

The items are read from your `UniqueItems.txt` in the game MPQ archives when BH
starts, so the list matches what the realm actually allows. The file carries
unreleased and placeholder rows alongside the real ones; only the rows it flags
as enabled are listed.

### Searching

Click the search box and type. The list filters as you type, matching on the
unique's name, its base item and the base's item type, so all of these work:

| Typing   | Shows                                   |
| -------- | --------------------------------------- |
| `shako`  | Harlequin Crest                         |
| `mara`   | Mara's Kaleidoscope                     |
| `amulet` | every unique amulet                     |
| `diadem` | every unique built on a Diadem          |

Everything else works as it does on the Runewords tab: clicking into the box
empties it, enter selects the first match, and the list scrolls with the wheel,
the scrollbar, or `PgUp`/`PgDn`, `Home` and `End`.

### Summary panel

Point at any row and the item is described beside the window: its name, its base
item, the character level it requires, and its stats. The stats are read from the
same tables and string files the game itself uses to describe an item, so the
wording matches what you would see on the item, in whatever language your client
is installed in. Stats the item grants more than once are added together, the
lines are ordered by the same `descpriority` the game orders them by, and the
resistances and attributes collapse into their combined lines under the same
conditions - see [the Runewords tab](#summary-panel) for the detail, since both
tabs render their stats through the same code.

The values are the ranges the item can roll rather than what a particular copy
rolled, so an Arkaine's Valor reads `+1-2 to All Skills`.

### Chat command

`.uni <search>` opens the window on the Uniques tab with that search already
applied. Matches are shown in the window only, not repeated into the chat log.
`.uniques` is a longer alias.

### Columns

| Column | Contents                                                      |
| ------ | ------------------------------------------------------------- |
| Unique | The unique's name, in unique gold.                             |
| Base   | The item it is built on, in the same gold, since the game draws the whole of a unique's name in one colour. Both columns brighten together as you point at the row. |

A few uniques share a name across several bases - the Rainbow Facet jewels - so
they appear once per base, next to each other, and the base column is what tells
them apart. A name too long for its column is cut off with `..`.
