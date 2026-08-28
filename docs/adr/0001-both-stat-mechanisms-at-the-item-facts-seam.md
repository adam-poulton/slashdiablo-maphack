# Expose both stat mechanisms at the ItemFacts seam

`ItemFacts` reaches item stats through two members rather than one: a `Stat(id, sub)` point query and a `Stats()` accessor for the raw entry list. This looks redundant, and it is deliberate: the live path genuinely uses two different mechanisms, and they do not necessarily agree. `ItemStatCondition` calls `D2COMMON_GetUnitStat`, which aggregates; `ED`, `Charged`, `Durability` and `Fools` copy the stat list and scan it with their own predicates, unpacking sub-index bits that a point query cannot express. Keeping both means the live adapter forwards each condition to the same D2 call it used before, so collapsing 76 evaluate methods into 38 changes no live behaviour.

## Considered options

Expressing every stat read as a scan over `Stats()` would be the smaller interface, and it would make live and packet results structurally identical rather than merely tested to agree. It was rejected because it changes live behaviour wherever `GetUnitStat` aggregates differently from summing raw entries, and at the time of the decision the project had no tests with which to find out where that is.

## Consequences

The point query is the one place where the two adapters still implement the same question independently: the live adapter forwards to `GetUnitStat`, the packet adapter sums `properties`. That is one pair to keep honest instead of the 38 that existed before, and it is the natural thing to cover with fixtures.

Collapsing to `Stats()` alone stays open as a follow-up once the golden-master fixtures can demonstrate that the two agree across real items.
