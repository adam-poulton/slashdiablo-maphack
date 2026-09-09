# Filter builder and preview

Label: wayfinder:map

## Destination

A written spec at `.scratch/filter-builder/spec.md`, detailed enough that
implementation issues can be cut from it and built by separate sessions. It
covers four things as one design: the Info window's navigation redesign, an
Affixes view, the filter builder with its preview / test / simulation surfaces,
and the safety check. This effort plans; it writes no production code.

## Notes

Domain: `CONTEXT.md` holds the ubiquitous language and every session must speak
it. `docs/adr/` holds the decisions this feature builds on, in particular
ADR 0001 (the ItemFacts seam), ADR 0004 (a catalogue source becomes item facts
for seeding, not for searching) and ADR 0008 (the window framework runs out of
game). New terms this feature needs go into `CONTEXT.md`; decisions that close
off an option go into a new ADR.

Skills every session should consult: `/grilling` and `/domain-modeling` by
default, `/prototype` for the tickets typed as such, `/research` for those.

### Settled while charting

These came from the charting session, not from a ticket. They are the frame
every ticket is answered inside; a ticket that wants to overturn one has to say
so out loud.

- **The layer.** The released `BH.cfg` filter stays read-only and untoggleable.
  The builder owns a separate layer of structured rules, walked **first**, so
  its verdicts win. Migration is the player deleting conflicting lines from
  `BH.cfg` at their own pace.
- **Rules are structure, not text.** Condition rows and action fields, rendered
  from a single condition registry covering the whole vocabulary. No raw rule
  text and no escape hatch.
- **Hiding is a first-class action**, alongside name, description, tier and map
  mark.
- **Rules live in groups.** Individual rules and whole groups toggle.
- **Storage is BH-owned and structured**, never hand-edited. Profiles ("Reset",
  "End game") are separate files switched wholesale, orthogonal to filter level
  and ping tier.
- **Draft until Apply.** Edits do not reach the running game until applied.
- **Three surfaces, deliberately distinct.** *Preview* is the action template's
  own appearance with no item involved. *Test* walks chosen items against a
  chosen rule or the whole layer. *Simulation* generates a random assortment of
  drops from toggleable pools, unidentified as they would drop, with a rarity
  slider biasing the sample toward the rare end.
- **A player-set filter context** backs all three: filter level, class, CLVL,
  difficulty, area, seeded from the live game where there is one and defaulted
  where there is not.
- **The safety check warns, it never refuses.** A curated, player-extendable
  list of items that must never be silently lost, shown as a standing panel of
  verdicts, plus a warning when an Apply newly breaks an entry.
- **The builder lives inside the Info window**, which is why the existing five
  tabs condense to side navigation. Seeding a rule from a source gives an
  identity condition plus that source's own stats offered as one-click rows.
- **Affixes get a front end.** `AffixCatalogue` exists and registers
  `PrefixKind` / `SuffixKind` with the stat index; nothing draws it yet.
- **In scope beyond the core:** export and import of a rule group, and importing
  existing `ItemDisplay` lines out of the player's `BH.cfg`.

## Decisions so far

<!-- one line per closed ticket: gist, then the link to the ticket holding it -->

- [Prior art: how existing loot-filter builders present rule building](issues/01-prior-art-loot-filter-builders.md) -
  no mod in the D2 lineage ever shipped a rule editor; the two that exist
  elsewhere either shrink the vocabulary until rules cannot be expressed or
  never let the player author a condition at all. A structured editor cannot
  attach to a hand-edited file of lines, which backs the separate layer. Nobody
  anywhere ships a safety check. Findings in
  [research/01](research/01-prior-art-loot-filter-builders.md).

## Not yet specified

- **The builder's own layout.** How the rule list, the row editor, the action
  fields, the preview and the context controls share one panel. Cannot be drawn
  until the navigation redesign says what width there is, the registry says what
  a row looks like, and the action vocabulary says what fields there are.
  Ticket 01 adds two constraints to it: order has to stay visible in a UI
  organised by category, because that is where every category-organised filter
  editor loses it; and tier must read as structurally distinct from filter
  level, because confusing the two is the single most repeated misunderstanding
  in BH's own user guide.
- **Choosing a test subject.** How a player finds and pins an item to test
  against when they did not arrive by seeding from Info: a search, a pick from a
  catalogue view, an item off the ground in the live game.
- **Undo, and what Apply actually writes.** Whether the draft is a diff or a
  whole replacement, what happens to a draft the player abandons, and whether
  undo survives closing the window.
- **New domain terms for `CONTEXT.md`.** Layer, profile, group, draft, subject,
  simulation, safety list and whatever else the tickets coin. Gathered once the
  vocabulary has stopped moving.
- **Which decisions earn an ADR**, and their text.
- **Where the spec is cut into implementation issues**, and in what order.

## Out of scope

- Toggling or editing the released `BH.cfg` filter from the UI. The released
  filter is what players trust and are used to; the layer is how they move off
  it, not a way to damage it.
- A browsable view of the base filter's rules. Its rules surface only through
  preview attribution, which is what the migration story needs and no more.
- Automatic per-character or per-account layer switching. Profiles are switched
  explicitly by the player; conditions already read the class.
- Applying edits live to the running game. Ruled out in favour of draft-until-
  Apply; the feedback loop is preview, test and simulation instead.
