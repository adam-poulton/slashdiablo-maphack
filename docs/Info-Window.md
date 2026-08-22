Info Window
===========

An in-game reference window. It is a tabbed panel, so lookups that would
otherwise mean alt-tabbing to a wiki can live in the game. It has four tabs,
**Runewords**, **Uniques**, **Sets** and **Recipes**. Click a tab to switch to
it, or use the chat command that belongs to it.

## Opening it

`Info Window: True, VK_NUMPAD9` in `BH_settings.cfg` enables the window and sets
the hotkey. Defaults are on and numpad 9.

* Press the hotkey to open the window, and again to close it.
* **Escape closes it** while it is open, instead of opening the game menu.
* Right-clicking the title bar also closes it.
* Drag the open window by its title bar. The position is remembered in `UI.ini`.
* Closed, it sits as a title bar with the other BH windows near the bottom of
  the screen. Ctrl-click it to reopen, shift-drag it to move it.
* `.info` in chat opens it on whichever tab was last in front.

Closing the window clears where you had got to, so it opens on a clean list
rather than on the last thing you searched for.

## Runewords tab

Every runeword recipe the game allows, read from your `Runes.txt` and
`ItemTypes.txt` in the game MPQ archives when BH starts. `Runes.txt` also carries
dozens of placeholder rows that were never finished; those are skipped, leaving
the 78 released runewords plus the realm's server-side `Plague`.

`.rw <search>` opens the tab with that search already applied; `.runewords` is a
longer alias. Matches are shown in the window only, not repeated into the chat
log.

Searching matches the runeword's name, its rune names and the item types it is
allowed in, so `ber` finds every runeword that takes a Ber rune and `polearm`
every one that can be made in a polearm.
The number of runes in a recipe is the number of sockets the base needs.

### Summary panel

Point at a recipe and it is described beside the window: its runes, the character
level it requires - which is the highest requirement among its runes - the bases
it can go in, and its stats.

The stats are read from the same tables and string files the game itself uses to
describe an item, so the wording matches the finished item in whatever language
your client is installed in.

## Uniques tab

Every unique item the game allows, with what it rolls, read from your
`UniqueItems.txt` in the game MPQ archives when BH starts. The file carries
unreleased and placeholder rows alongside the real ones; only the rows it flags
as enabled are listed.

`.uni <search>` opens the tab with that search already applied; `.uniques` is a
longer alias.

Searching matches the unique's name, its base item and the base's item type, so
`amulet` finds every unique amulet and `diadem` only Griffon's Eye.

### Summary panel

Point at a unique and it is described beside the window: its base item, the
character level it requires, and its stats, rendered exactly as they are on
[the Runewords tab](#summary-panel) - added together, ordered by `descpriority`
and grouped by `dgrp`, since every tab renders stats through the same code.


## Sets tab

Every piece of every set the game allows, grouped under its set, with what the
piece grants and what its set grants around it. Read from your `SetItems.txt` and
`Sets.txt` in the game MPQ archives when BH starts, so the list matches what the
realm actually allows.

`.set <search>` opens the tab with that search already applied; `.sets` is a
longer alias.

Searching matches the piece's name, its base item, the base's item type and its
set's name, so `amulet` finds every set amulet and `tal ra` finds all five pieces of
Tal Rasha's Wrappings. Within a set the pieces are in the game's own order rather
than alphabetical, so a set reads head to toe.

The left and right arrows fold and unfold sets, from a set or from anything
inside it. They reach the search box instead once there is something typed in
it, since that is where they move the caret.

### Summary panel

Point at a piece and it is described beside the window: what it grants on its
own, then, under its set's name, what the set grants. The stats come from the
same tables and string files the game itself uses.

## Recipes tab

Every Horadric Cube recipe the game allows, read from your `CubeMain.txt` in the
game MPQ archives when BH starts. The file carries unfinished and placeholder
recipes alongside the real ones; only the rows it flags as enabled are listed.

`.cube <search>` opens the tab with that search already applied; `.recipes` is a
longer alias.

Recipes are left in the order `CubeMain.txt` gives them, which walks the cube
from the quest recipes through the potions, the gems and the runes to the
crafting and the upgrades, so a chain reads end to end rather than being broken
apart alphabetically.

Searching matches what a recipe makes, what it takes and what it does, so
`perfect ruby` finds both the recipe that makes one and every recipe that spends
one, and `ladder` finds the recipes only a ladder character can use.

A recipe names a range of items as often as it names one, so it reads as the
cube reads it: `Unsocketed Normal Weapon + Ral Rune + Amn Rune + Perfect
Amethyst` rather
than as a particular sword. Where the recipe forces a prefix or a suffix, the
result is named by it, so the prismatic amulet recipe is listed as what it
actually makes.

### Summary panel

Point at a recipe and it is described beside the window: what it makes, what it
is made from, the bonuses the result is guaranteed, and, under those, what else
the recipe does - the sockets it adds, the levels it costs, the item level the
result comes out at, and any condition on using it at all.

The bonuses are read from the same tables and string files the game itself uses
to describe an item, so a crafted item's guaranteed bonuses are worded exactly
as they are on [the Runewords tab](#summary-panel) and read in whatever language
your client is installed in.

The sockets a recipe adds and the levels it costs are shown in words rather than
as bonuses, because that is what the game does with them: it puts them into the
shape of the item instead of giving either a line of its own.
