# What a stat criterion cannot ask for

A stat criterion is answered from the stat totals a source's property entries add up to, which is the mechanism that answers what a property grants rather than the one that answers how it reads. Some of what a source really grants is outside those totals, so no criterion can find it. All of it is deliberate, and this is where it is written down rather than left to be discovered by a player whose search came back empty.

**Amounts granted per character level.** The tables hold these in eighths in the property's parameter rather than in its range, so there is no amount to compare a value against. Harlequin Crest grants one and a half life a character level and no flat life at all, and is found by no criterion on life, nor by one on the per-level stat it is granted under.

**Poison damage.** Held per frame in 256ths, with the duration in frames in the parameter, so the number on the row is not the number the game shows and neither is a number of points a player would type.

Both are left out rather than converted because a caller comparing a value against a stat's range cannot tell converted units from points, and answering in the wrong units answers worse than answering nothing.

**Properties the tables give no stat to.** A handful of properties are built into the game instead, and only the ones spelled out by name in `StatDescriptions` grant anything to add up: `dmg-min` and `dmg-max` under `mindamage` and `maxdamage`, `indestruct` under `item_indesctructible`, and `dmg%` under both `item_mindamage_percent` and `item_maxdamage_percent`, which is the one of the four whose totals name different stats from the stand-in stat that describes it. Anything else the tables name no stat for is unreachable, and adding one is a row in that table rather than a change to the index.

This refines the last paragraph of ADR 0004, which reads as though every hardcoded property is unreachable. The four that are spelled out are reachable; the exception is the ones that are not.

## What is reachable but is not a roll

The two halves of a damage line are each the whole of what they grant. "Adds 10-40 Fire Damage" is one row carrying ten and forty, and it writes exactly ten of `firemindam` and exactly forty of `firemaxdam` rather than either half rolling ten to forty. The totals say so, so a criterion asking for more than thirty of a minimum does not find it and a result reports each half as the fixed amount it is. Reading the row's two ends as a range on both stats is the mistake this is written down to prevent.

## Which roll answers a criterion

A criterion is answered on the roll most favourable to it. "More than" is answered on the best roll, which is the case the spec calls out: what a player is asking is what could grant them a stat, and hiding a source whose worst roll falls short is the answer being wrong. The same reading settles the other two comparators, which the spec does not call out: "less than" is answered on the worst roll and "equal to" by the value falling anywhere in the range, so a criterion is satisfied whenever some roll of the source would satisfy it. A result carries the range either way, so what the player sees is what the source can do rather than the one roll that answered.

A source that does not write the stat at all answers no criterion on it. Its range would otherwise read as zero to zero, which "less than five" would be satisfied by for every source in the index.

## Considered options

Widening the totals to carry per-level amounts, tagged as per-level so a caller could tell them apart, was rejected for now. A criterion asking for thirty life would then have to say which character level it is asking at, and the value a player types is not a level-dependent one. It is not ruled out: a criterion carrying a level is a coherent thing to add later, and the totals are where it would go.

Answering a criterion by walking a rule against the source said as item facts, which is what a catalogue item is, would reach both. ADR 0004 records why the search does not go that way, and the way in for a search that genuinely needs it is to walk a narrowed set of sources through the bridge rather than to widen the index.

Leaving the gaps undocumented was rejected because the failure they produce is silence. A search for life that does not find Harlequin Crest looks like a bug in the index, and the reason it is not one is not visible from anything the index says.

## Consequences

A criterion names a stat as `ItemStatCost.txt` names it, `fireresist` rather than Fire Resist, and asks about one stat at a time. What the game folds into a single line, the four resistances and the four attributes, is four criteria and four ranges. Offering a player the names they read, and one row that expands into four criteria, is the condition builder's work and not the index's.

A source that grants nothing a criterion can reach is still in the index, answers a text criterion, and carries its worded lines. Nothing disappears; only the stat criteria cannot see what is listed above.
