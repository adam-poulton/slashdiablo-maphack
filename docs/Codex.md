Codex
=====

An in-game reference window. It is a tabbed panel, so lookups that would
otherwise mean alt-tabbing to a wiki can live in the game. It has five tabs,
**Runewords**, **Uniques**, **Sets**, **Recipes** and **Bases**. Click a tab to
switch to it, `Tab` / `Shift + Tab` to cycle or use the chat commands.

## Opening it

`Codex: True, VK_NUMPAD9` in `BH_settings.cfg` enables the window and sets
the hotkey. Defaults are on and numpad 9. The hotkey is also bindable from the
settings window, under **Panel hotkeys** on the Input tab.

* Press the hotkey to open the window, and again to close it.
* **Escape closes it** while it is open, instead of opening the game menu.
* Right-clicking the title bar also closes it.
* Drag the open window by its title bar. The position is remembered in `UI.ini`.
* Closed, it sits as a title bar with the other BH windows near the bottom of
  the screen. Ctrl-click it to reopen, shift-drag it to move it.
* `.codex` in chat opens it on whichever tab was last in front.

Closing the window clears where you had got to, so it opens on a clean list
rather than on the last thing you searched for. That includes any conditions you
had set.

## Searching by what something grants

Searching matches words. To ask what something *grants* - every unique that can
roll thirty faster cast rate, say - use the filter button on the right of the
search box.

Clicking it drops a row of conditions under the search box. A condition is a
stat, a comparison and a number, and the **+ Add condition** line under the last
row adds another. Conditions are combined with the search and with each other,
so every one of them has to be satisfied: `Faster Cast Rate > 20` and
`Fire Resist > 30` finds what rolls both, not either.

* The stat box is a search of its own. Type into it and pick from the list that
  drops below. The names are the game's own words for each stat; where two would
  otherwise read alike the one measured as a percentage says so, so flat Cold
  Absorb and `Cold Absorb %` are separate entries.
* Only the stats something in the tab actually grants are offered, so a
  condition can always be satisfied by something. Which stats those are changes
  with the tab.
* **Leave the number empty** and the condition matches anything granting that
  stat at all, whatever it rolls. That is the quickest way to ask what has
  Crushing Blow, or what can spawn with sockets.
* `>` and `<` are the wording the item filter uses, and mean the same: `>` is
  more than, and it is answered on the best roll. `= 30` finds anything that can
  roll exactly thirty, which for a range means thirty falls inside it.
* A condition does nothing until it names a stat, so adding a row does not empty
  the list while you are still filling it in.
* The `x` at the end of a row takes it away, and empties it when it is the only
  row left - which is how you call off the filter without closing the window.

Conditions are shared between the Runewords, Uniques and Sets tabs, as the
search box is, so you can ask the same question of each in turn. The Recipes and
Bases tabs do not offer them and the button is not drawn there.

Clicking the button again puts the rows away **but leaves them applied**, which
is how you get the room back on a small window without losing the query. The
button's funnel turns gold whenever something is being filtered on, and the
footer says how many conditions are in force.

There are things a condition cannot find, all of them deliberate and all of them
listed in `docs/adr/0005`: amounts granted per character level, poison damage,
and a handful of properties the tables give no stat to. A search for life does
not find Harlequin Crest, whose life is granted per level.

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

Every Horadric Cube recipe the game allows, read from game MPQ archives when BH starts.
The file carries unfinished and placeholder recipes alongside the real ones;
 only the rows it flags as enabled are listed.

`.cube <search>` opens the tab with that search already applied; `.recipe` and
`.recipes` are longer aliases for it.

Recipes are grouped, and appear in the order `CubeMain.txt` first reaches them,
recipes keep the file's order inside their group.

Searching matches what a recipe makes, what it takes, what it does and the group
it is in, so `perfect ruby` finds both the recipe that makes one and every recipe
that spends one, `caster` finds the nine caster crafting recipes, and `ladder`
finds the recipes only a ladder character can use.

The crafting recipes are grouped under the family they belong to - `Hit Power`,
`Blood`, `Caster` and `Safety` - where available.

### Summary panel

Point at a recipe and it is described beside the window: what it makes, what it
is made from, the bonuses the result is guaranteed, and, under those, what else
the recipe does - the sockets it adds, the levels it costs, the item level the
result comes out at, and any condition on using it at all.

The bonuses are read from the same tables and string files the game itself uses
for localisation, where possible.

## Bases tab

Every base item the game drops, before anything is made of it, read from your
`Weapons.txt`, `Armor.txt`, `Misc.txt` and `ItemTypes.txt` in the game MPQ
archives when BH starts. The files carry quest pieces and rows that were never
finished alongside the real ones; only the rows flagged as spawnable are
listed.

`.base <search>` opens the tab with that search already applied; `.bases` is a
longer alias.

Bases are grouped by item type, and groups appear in the order the tables first reach them,
which walks the weapons, then the armour, then the jewellery, gems and runes.
Inside a group the bases run normal, then exceptional, then elite, and each tier in the
order it starts dropping.

Searching matches the base's name, its item type, its tier and its three letter
code, so `elite` finds every elite base, `circlet` all four circlets, and `uap`
the Shako.

### Summary panel

Point at a base and it is described beside the window, as the game describes it
in your inventory: its damage or its defense, its durability, and what a
character needs to use it. Under those are the two things the game only ever
shows once an item has been made of it:

* **Attack speed**, on weapons. `Weapons.txt` holds it as a modifier rather than
  as a rate, so it counts down - a weapon at `[-30]` swings faster than one at `[0]`.
* **Sockets**, as the range an item made on the base can roll.
