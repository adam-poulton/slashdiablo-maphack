# slashdiablo-maphack

A Diablo II mod that judges every item the game shows or is about to show, and
decides what the player sees of it. The language below is the item filter's,
which is where most of the project's own vocabulary lives.

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
