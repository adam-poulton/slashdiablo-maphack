# Rule and group order within a layer, and what toggling a group means

Type: grilling
Status: open
Blocked by: 08

## Question

The layer is walked before the base filter, so within the layer order decides
which of two of the player's own rules wins.

Decide whether the player controls that order and how. Decide what a group is
with respect to order: a contiguous run of rules that moves as one, or a label
that says nothing about position. Decide what disabling a group does to the
rules inside it - whether a rule remembers its own enabled state underneath a
disabled group, and what the UI shows for a rule that is on inside a group that
is off.

Decide how `%CONTINUE%` reads here, given that the layer handing over to the base
filter is itself a kind of continue.
