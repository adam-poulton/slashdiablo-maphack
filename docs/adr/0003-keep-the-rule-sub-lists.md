# Keep the rule sub-lists rather than walking every rule once

Rules are filed into five lists as they are read, by what their action does, and the lists overlap: a rule that both names an item and marks the automap is in two of them and is evaluated twice for the same item. In the shipped config that is roughly 1,600 of about 3,300 rules, so walking the one master list instead, with a note per list of whether it has stopped, would evaluate every rule exactly once and is the obvious tidy-up. It is rejected because the sub-lists are not a way of organising the rules, they are an early exit, and the tidy-up costs far more than it saves.

A walk ends at the first matching rule that asks to stop, and asking to stop is the default. So a name walk of some 3,270 rules usually ends a few hundred in, and a map walk of some 1,640 ends sooner still. A single walk cannot end until every list it is serving has ended, and the ignore list's ten-odd rules are scattered through to the end of the file, so it would read nearly all 3,300 rules for every item. Against the shipped config that is several times the work, on the path whose own comments record it dropping frames. It is also worse per caller: the automap asks about every item in every room and wants only the map list, and a single walk would make it pay for the four lists it never reads.

## Considered options

Walking only the union of the lists a particular caller needs splits the difference. It was rejected for the same reason in smaller measure, and for adding a second idea of what a walk is over.

The numbers above are an approximation of how the rules divide, taken by matching the shipped `Packaging/BH.cfg` against what decides which list a rule goes in. Anyone revisiting this should take them again rather than trust them, and should take them against a config in use: a filter written to name almost nothing would divide quite differently.

## Consequences

The duplicate evaluation stays, and is real. Nothing has measured it as a cost, which is the honest state of it: it is a count of redundant work, not an observed slowness.

If a profile ever shows it mattering, the single walk needs only the placement of each rule kept on the rule itself, which is worked out when the rules are read and currently thrown away. `MatchingActions` does not stand in the way of it: it walks whatever list it is handed, and a master list is a list.
