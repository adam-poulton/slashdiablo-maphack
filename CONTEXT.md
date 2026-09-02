# slashdiablo-maphack

A Diablo II mod that judges every item the game shows or is about to show, and
decides what the player sees of it. Most of the language below is the item
filter's, which is where most of the project's own vocabulary lives. The rest is
the catalogue's, which is what the game's own tables are read into.

## Language

### Judging an item

**Rule**:
One line of item display configuration: the conditions an item must satisfy and
the action to take if it does.
_Avoid_: filter, filter line, entry, pattern

**Condition**:
One question a rule asks, about the item or about the world it dropped into.
_Avoid_: predicate, check, test, clause

**Action**:
What a rule wants done with an item that satisfies it: the name to show, the
description, the marks on the automap, the tier, whether to stop.
_Avoid_: result, output, effect

**Walk**:
Reading one list of rules against one item, in the order the rules were written,
until a rule says to stop.
_Avoid_: scan, pass, sweep, loop

**Verdict**:
The whole of what the rules make of one item: its name, its description, its
marks on the automap, and whether it is blocked.
_Avoid_: decision, outcome, judgement

**Match**:
The part of a verdict the map, do-not-block and ignore lists settle between
them. Narrower than a verdict, which also carries the name and the description.
_Avoid_: hit

### What a rule reads

**Item facts**:
Everything a condition can ask about the item itself, said the same way whether
the item exists yet or not.
_Avoid_: item data, item info, properties

**Filter context**:
Everything a condition can ask that is not about the item: which class is
playing, how far they have got, the difficulty, where they are standing, how
much they have asked to hide.
_Avoid_: environment, state, world state

**Live item**:
An item that already exists in the world, which is to say a game unit. It can be
asked things a described item cannot, such as its price.
_Avoid_: real item, unit item, in-game item

**Packet item**:
An item the server has described but which does not exist yet. It is the only
kind that can be stopped before the player ever sees it.
_Avoid_: dropped item, incoming item, ground item

**Catalogue item**:
A source said as item facts, at one of the rolls it can take. An item nobody has
dropped and nobody will, described from the tables alone, so that a rule can be
walked against it without a game.
_Avoid_: fake item, sample item, hypothetical item

### What the player is shown

**Blocked**:
Hidden from the player entirely, by stopping the packet that would have
announced the item. Only a packet item can be blocked; an item already in the
world can only be marked as one the rules would have hidden.
_Avoid_: filtered, ignored, suppressed, hidden

**Kept**:
Named or marked by some rule, and so not blocked however many later rules would
hide it.
_Avoid_: whitelisted, allowed, shown

**Tier**:
How eagerly a rule's automap mark and chat notification are wanted, set against
a threshold the player chooses. A tier never decides whether an item is blocked.
_Avoid_: ping level, priority, importance

**Filter level**:
How much of what drops the player has asked to hide, which rules read to say
what should disappear at each setting.
_Avoid_: filter strength, hide level

### What the tables hold

**Catalogue**:
One kind of thing the game's tables describe, read once and kept: the uniques,
the set items, the runewords, the cube recipes, the base items, the prefixes and
the suffixes.
_Avoid_: database, registry, store, cache

**Source**:
One thing in a catalogue: a unique, a piece of a set, a set's own bonus, a
runeword, a cube recipe, an affix. What they have in common is granting stats,
which is what makes them worth holding together.
_Avoid_: record, entry, result

**Variant**:
The same source made on a different kind of base, and the whole of what it
grants there. A runeword rolls differently in a weapon, in a helm or body
armour and in a shield, because each of its runes gives one set of bonuses per
kind. A criterion is answered by whichever variant satisfies it.
_Avoid_: version, flavour, form, slot

**Roll**:
One of the amounts a source can grant, out of the range its properties are held
in. The best roll is every property at the top of its range and the worst every
property at the bottom. A criterion is answered on whichever roll suits it and
reports the range either way; a catalogue item has to be said at one, because an
item carries a number where a source carries a range.
_Avoid_: value, amount, instance

**Ingredient**:
One of the things a source is made from, named by its code and kept in the
order they go in: the runes of a runeword in socket order. A source made from
nothing has none.
_Avoid_: component, part, reagent, input

**Base tier**:
Which of the three the game builds a base item in: normal, exceptional or
elite. A base names the other two in its upgrade columns, and the tier it is
itself by pointing one of them at its own code. A source made on no one base is
built in none. Always said with its base, since a rule's tier is a different
idea under the same word.
_Avoid_: quality, grade, level

**Property**:
One entry in the game's tables that grants something: what it grants, the
parameter saying which of it, the range it rolls in, and the count of set pieces
where that applies. A source's stat lines and its totals are both worked out
from its properties, by the one path every catalogue reads them through.
_Avoid_: mod, modifier, bonus, affix

**Stat index**:
Every source of every catalogue, keyed by what it can grant, so that what grants
a stat can be asked without knowing which catalogue the answer is in.
_Avoid_: search index, lookup, table

**Criterion**:
One question the stat index is asked of a source: a stat with a comparator and a
value, or a piece of text. A stat criterion is answered on the best roll, so a
source that can reach the value answers it. Not a condition, which is what a rule
asks of an item.
_Avoid_: condition, filter, term, query

**Query**:
The whole of what the stat index is asked: the criteria, all of which a source
must satisfy, and the one kind of source to answer from where the asker wants
only one. Structured rather than written as text, so that a condition builder
can offer rows without a syntax having to be parsed.
_Avoid_: search, filter, request

**Kind**:
What a catalogue calls itself where a kind of source is asked for: "unique",
"set item", "set bonus", "runeword", "recipe", "base", "prefix" and "suffix". A
catalogue owns more than one where the things it reads are not one another's
variation, as a set's pieces and its own bonuses are not, and as a prefix and a
suffix are not. The catalogue owns the word; the stat index only ever compares
it, which is what keeps the index ignorant of every kind there is.
_Avoid_: type, category, class

**Heading**:
What a catalogue gathers its sources under where it has something to gather
them by: the recipes are gathered under what they make and, where that says
less, under what they do. A heading is the catalogue's, not the panel's, so a
panel that shows one and a search that reads one agree without arranging
anything for themselves. The list a panel draws it into calls the row a group,
which is that class's own word for a row that folds and not a second word for
this.
_Avoid_: group, category, section

**Note**:
Something a source does that no stat line words, because the game shows it in
the shape of the item instead: the sockets a recipe adds, the levels it costs,
the conditions it is only allowed under. Read into words by the catalogue, and
drawn after the stat lines.
_Avoid_: remark, detail, extra

**Entry**:
One source as the stat index holds it: the kind that registered it, the search
key a text criterion is matched against, what it can grant as stat totals, one
set per kind of base it can be made on, and a handle back to the source itself. An entry is how the index holds a source,
never another word for one.
_Avoid_: record, row, item

**Result**:
One source that answered a query, and the range it rolls for each stat criterion
that was asked. The range is part of the answer rather than a detail of it: an
amount a source can roll is not one it will.
_Avoid_: match, hit, answer
