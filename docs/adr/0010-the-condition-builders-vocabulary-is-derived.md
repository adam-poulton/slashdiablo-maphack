# The condition builder's vocabulary is derived

The stats the codex's condition builder offers are every stat the sources in
front of the player actually write, read out of the stat index, and each is
labelled with the game's own description of it with the value's placeholder
taken out. There is no list of stats written anywhere. The only names written
down are for the nine the tables describe with nothing at all, which is not a
vocabulary so much as an admission that for those there is nothing to derive
from.

This supersedes the expectation in the Consequences of ADR 0005, which reads as
though the builder would hand-write the names a player reads and fold the four
resistances into one row. It does neither.

## Why

A criterion names a stat as `ItemStatCost.txt` names it, and the tables' names
are not the words a player would type. The obvious answer is a curated list of
friendly names, and against the real 1.11 tables that list would need well over
a hundred entries: the property codes the uniques, the set items and the
runewords use write 148 distinct stats between them, 141 of them described. A
list that long is a list that goes stale, and it goes stale silently - a realm
that ships a unique granting something the list does not carry makes that
something unsearchable, and nothing says so.

Asking the index instead has a property no curated list can have. The set of
stats offered is exactly the set a criterion can answer, because both are read
from the same totals: a stat in the picker is granted by at least one source in
scope, and a stat granted by one is in the picker. Nothing offered can come back
empty for want of anything to grant it, and nothing that could be found is
missing from what may be asked for. The per level and poison amounts ADR 0005
rules out disappear from the picker without being named here, because
`CollectTotals` already leaves them out of the totals this reads.

## What it costs

The game's description strings were written to follow a number rather than to
label a control, so the derivation alone does not produce a list a player can
read. What a player sees must satisfy two things the tables do not give for
free: every entry is words rather than a table's name for something, and no two
entries read alike. Three kinds of wart stand in the way, and each is answered
without giving up the derivation.

**Fragments.** `descstrpos` for `maxhp` is "to Life". The leading preposition is
dropped and the placeholders are substituted with nothing, which turns "+%d to
Life" into "Life" and "+%d magic damage" into "magic damage". Some come out
awkward - `item_charged_skill` reads "(/ Charges)" - but they are still words,
and left alone.

**Pairs the tables word alike.** Four of the five are a stat granting points
beside one granting a share of them: flat and percentage cold, fire and
lightning absorb are each "Cold Absorb", "Fire Absorb", "Lightning Absorb", and
`damageresist` and `normal_damage_reduction` are both "Damage Reduced by".
Which is which *is* in the tables even though the wording is not - `descfunc`
says whether a value renders as a percentage - so a label shared by two stats
gets a "%" on whichever of them reads as one. This is derived, not written down,
so it settles whatever pairs a future table happens to hold.

**Stats the tables word with nothing at all.** Seven have no `descfunc`, and the
two skill stats have one but take their words from a parameter no picker can
supply. There is nothing to derive from, so these carry a written name -
`item_numsockets` is "Sockets", `maxdurability` is "Durability" - as do
`magicmindam` and `magicmaxdam`, which share every column that could tell them
apart. Nine entries, each there because the tables say nothing, and the list
does not grow as the data does.

Against the real 1.11 tables that leaves 122 entries for the uniques, 95 for the
runewords and 80 for the set items, with no raw stat name and no repeated label
among any of them. A last-resort rule appends the stat's name to a label still
shared after all of the above; it fires on none of the current data, and its
firing is the signal that a pair needs a written name rather than that a player
should read a table's.

## Considered options

**A hand-authored list of friendly names**, with one row expanding into four
criteria for the resistances and the attributes, which is what ADR 0005
expected. Rejected for the staleness above. It is also the only option that
could offer "All Resistances" as one row, and that is a real loss: a player
wanting it writes four conditions instead of one. It is not ruled out for ever -
a short table of groupings laid over a derived vocabulary would add it back
without giving up the derivation, and that is the way in if anyone wants it.

**Every `ItemStatCost` stat with a `descfunc`, minus an exclusion list.**
Rejected because the exclusion list is the maintained table again, wearing a
different hat, and because it offers stats nothing in scope grants.

**Two combo boxes, a category and then the stats in it.** Rejected because a
category per stat is a hand-authored table, and because `Combohook` does not
scroll: its open list is N rows hanging off the box, and the window is drawn for
a 600 pixel canvas.

**Dropping the stats the tables word badly** - the valueless flags, the ones
with no description. Rejected because it breaks the property the whole approach
is for. Once something is droppable the picker is no longer the set a criterion
can answer, and a player whose search comes back empty is back to not knowing
whether the thing is unfindable or merely absent.

**Showing the stat's own name beside the label**, in a second column, so that
nothing has to be written down and every pair tells itself apart. This is what
was built first. Rejected because it puts table names in front of a player to
solve a problem five rows have, and because it does not actually finish the job:
a row shows only the label once a stat is chosen, so two conditions reading
"Cold Absorb" still looked alike.

## Consequences

Adding a catalogue, or a realm adding an item, changes what the builder offers
without a line of code changing. A wording fix in the string tables reaches the
picker for the same reason.

The written names are for stats the tables say nothing about, and are not the
place to put a label that merely reads better than the tables' own. Overriding a
stat the tables do describe would make the derived list a fallback rather than
the source, and the next reader would have no way to tell which entries were
still following the data. Anyone wanting nicer wording should reach for the
grouping table described above, which lays something over the derived list
without replacing any of it.
