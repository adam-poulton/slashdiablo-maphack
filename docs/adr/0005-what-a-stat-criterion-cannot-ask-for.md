# What a stat criterion cannot ask for

A stat criterion is answered from the stat totals a source's property entries add up to, which is the mechanism that answers what a property grants rather than the one that answers how it reads. Two kinds of thing a source really grants are outside those totals, so no criterion can find them. Both are deliberate, and this is where they are written down rather than left to be discovered by a player whose search came back empty.

**Amounts granted per character level.** The tables hold these in eighths in the property's parameter rather than in its range, so there is no amount to compare a value against. Harlequin Crest grants one and a half life a character level and no flat life at all, and is found by no criterion on life, nor by one on the per-level stat it is granted under. Excluding them is what stops a caller reading the eighths as a flat amount, which would answer worse than answering nothing.

**Properties the tables give no stat to.** A handful of properties are built into the game instead: `dmg%`, `dmg-min`, `dmg-max` and `indestruct` are spelled out by name in `StatDescriptions` and do reach the index, under the stats they really write rather than the stand-in stat that describes them. Anything else the tables name no stat for grants nothing to add up. Adding one is a row in that table, not a change to the index.

This refines the last paragraph of ADR 0004, which reads as though every hardcoded property is unreachable. The four that are spelled out are reachable; the exception is the ones that are not.

## Considered options

Widening the totals to carry per-level amounts, tagged as per-level so a caller could tell them apart, was rejected for now. A criterion asking for thirty life would then have to say which character level it is asking at, and the value a player types is not a level-dependent one. It is not ruled out: a criterion carrying a level is a coherent thing to add later, and the totals are where it would go.

Answering a criterion by walking a rule against the source said as item facts, which is what a catalogue item is, would reach both. ADR 0004 records why the search does not go that way, and the way in for a search that genuinely needs it is to walk a narrowed set of sources through the bridge rather than to widen the index.

Leaving the gap undocumented was rejected because the failure it produces is silence. A search for life that does not find Harlequin Crest looks like a bug in the index, and the reason it is not one is not visible from any answer the index gives.

## Consequences

A criterion names a stat as `ItemStatCost.txt` names it, `fireresist` rather than Fire Resist, and asks about one stat at a time. What the game folds into a single line, the four resistances and the four attributes, is four criteria and four ranges. Offering a player the names they read, and one row that expands into four criteria, is the condition builder's work and not the index's.

A source that grants nothing a criterion can reach is still in the index, answers a text criterion, and carries its worded lines. Nothing disappears; only the stat criteria cannot see the two kinds of grant above.
