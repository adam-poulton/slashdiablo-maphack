# Reusable named pieces: does the layer have styles, item lists and condition groups

Type: grilling
Status: open
Blocked by: 02, 04

## Question

Surfaced by [ticket 01](01-prior-art-loot-filter-builders.md). Every filter that
grew up under real use grew named, reusable pieces rather than more rules: Path
of Diablo and Project Diablo 2 added named styles, item lists, text macros and
aliases; FilterBlade has Modules. BH already has one of its own in
`ConditionGroup`, read out of the config into `condition_group` and expanded
while a rule is parsed, and the builder currently plans to ignore it.

Decide whether the layer has reusable pieces of its own, and which kinds: a named
run of conditions, a named action or style shared by many rules, a named list of
items a condition matches against. Decide what editing one does to every rule
using it, and how a rule shows that part of it is not its own.

The pull the other way is real: a reusable piece is an indirection, and a player
who cannot see what a rule actually says has lost the thing rows were meant to
give them. Deciding *not* to have them is a legitimate answer, but the evidence
is that filters acquire them anyway, and that deciding late is worse.

Decide separately what the layer does with the existing `ConditionGroup`
mechanism, which the base filter uses and the layer is walked alongside.
