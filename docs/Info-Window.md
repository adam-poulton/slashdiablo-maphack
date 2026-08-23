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

Recipes are grouped, and every group starts folded, so the tab opens as a list
of the kinds of recipe there are rather than as a hundred and fifty rows to
scroll through. `CubeMain.txt` has no
column saying which kind a recipe is, so each is read out of its own row: first
what the recipe does to the item, which is what the recipes anyone hunts for as a
group have in common - `Sockets`, `Repairing`, `Upgrading`, `Rerolling`, and the
four crafting families - and failing that what kind of item it makes, which
gathers the chains up under `Gem` and `Rune`. That last part walks up the `Equiv`
columns of `ItemTypes.txt`, so a heading is whatever the realm calls that item
type and a type the realm adds is gathered under whichever known type it
descends from.

Groups appear in the order `CubeMain.txt` first reaches them, which walks the
cube from the quest recipes through the potions, the gems and the runes to the
crafting and the upgrades, and recipes keep the file's order inside their group,
so a chain reads end to end rather than being broken apart alphabetically.

The left and right arrows fold and unfold groups, from a group or from anything
inside it, exactly as they do on [the Sets tab](#sets-tab). They reach the search
box instead once there is something typed in it.

Searching matches what a recipe makes, what it takes, what it does and the group
it is in, so `perfect ruby` finds both the recipe that makes one and every recipe
that spends one, `caster` finds the nine caster crafting recipes, and `ladder`
finds the recipes only a ladder character can use. A search unfolds whatever it
matched and folds it back up again when the search is cleared.

A recipe names a range of items as often as it names one, so it reads as the
cube reads it: `Unsocketed Normal Weapon + Ral Rune + Amn Rune + Perfect
Amethyst` rather than as a particular sword. Where the recipe forces a prefix or
a suffix, the result is named by it, so the prismatic amulet recipe is listed as
what it actually makes.

The crafting recipes are listed under the family they belong to - `Hit Power`,
`Blood`, `Caster` and `Safety` - rather than all reading as a crafted item, since
that is what they are known as. The family is not something the game itself ever
shows, so it is read out of the description `CubeMain.txt` carries on the row; a
realm that adds a family of its own reads as a crafted item until it is named
here.

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
