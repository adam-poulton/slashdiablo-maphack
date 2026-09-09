# The condition registry: how a condition declares itself

Type: grilling
Status: open

## Question

The builder renders condition rows from a single declaration rather than from 41
hand-written pickers. What does that declaration hold?

`BH/Modules/Item/ItemFilter.cpp` carries roughly 120 keywords across 41 condition
classes: plain numeric stats (`FRES`, `IAS`), enumerations backed by game tables
(`AREAID`, class names), flags with no value at all (`ETH`, `SUP`), keywords that
take a parameter as well as a comparator (`CHARSTAT`, `SK`, `CLSK`), and at least
one that holds rules of its own.

Decide what a condition has to say about itself for a row to be drawn, and for a
value to be offered and validated, without the builder knowing any condition by
name. Decide where the declaration lives relative to the parser, so that the two
cannot drift, and what happens to a condition that declares nothing.

`Query` in the stat index was already shaped this way and is the nearest prior
art in this codebase; see `CONTEXT.md` under **Query**.
