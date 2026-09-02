# A set bonus is a source of its own

A set grants in two places. A piece grants what it is worth on its own and what
it adds once enough of its set is worn; the set grants what wearing several of
its pieces is worth, which no piece carries. So the set items catalogue reads
two kinds of source, `set item` and `set bonus`, and registers both into the
stat index. A search for a stat that arrives only with four pieces worn answers
with the set that grants it, named as a set rather than as whichever piece
happened to be listed beside it.

Two kinds rather than one because a kind is what a panel scopes to. The sets
panel lists pieces under set headings, and a query scoped to one kind that
answered with bonuses as well would put a set into the list as though it were an
item to find. Keeping them apart costs the panel one lookup, which is the
catalogue answering what a piece's set is worth, and leaves a global search free
to show both.

## What a criterion sees

A source's stat totals are collected from its always-on properties and from the
ones a count of pieces unlocks, together. A stat a piece grants only at three
pieces worn is a stat that piece grants, and a search that could not find it
would be answering the player's question wrongly in the same way ADR 0005
describes.

The count is not part of the totals, so a criterion cannot ask for a stat *at* a
count, and a range that spans an always-on amount and a counted one reads as a
single range. Neither is worth widening the totals for while a criterion carries
no count of its own: the source's lines say which bonus needs how many pieces,
and the panel and the summary both read them.

What the numbers a piece's base carries are worth is the exception. Only the
always-on properties move them, because the game only moves them for a piece
worn on its own, so a source carries its modifiers already worked out rather
than leaving a reader to derive them from the index's totals.

## Considered options

Holding a set's bonuses on each of its pieces, as the panel did before there was
a catalogue, was rejected because it is the shape that makes user story 9
impossible: a stat no piece grants has nowhere to live, and a search can only
answer with pieces that do not themselves grant what was asked for.

One kind for both, with a flag on the source saying which it is, was rejected
because every consumer would then have to know the flag. The index compares
kinds and nothing else, which is what keeps it ignorant of every catalogue there
is, and a flag it does not read would push that knowledge into each caller.

## Consequences

A set's bonuses are named by the set's own code, so a piece reaches them through
one lookup on the catalogue rather than by holding an index into a list. A realm
that has edited its sets table changes what the lookup answers and nothing else.

Sets are two of the index's kinds where the other catalogues are one each, so
anything offering the player a kind to search shows a word for each. That is a
truthful list rather than a leak: a set bonus really is a different kind of thing
from a piece of a set.
