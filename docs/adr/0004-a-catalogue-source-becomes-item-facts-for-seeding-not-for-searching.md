# A catalogue source becomes item facts for seeding, not for searching

A source can be said as item facts at one of the rolls it can take, which makes it a catalogue item: a third kind beside a live item and a packet item. `ItemFacts` already has a home for everything such an item knows, down to its base code, its quality, its unique or set or runeword identity, its prefixes and suffixes and its properties, and `StatSource` is an interface a source can answer over its own property entries. So a rule can be walked against an item nobody has dropped, which is what lets a filter builder be seeded from an item the player picked out of the Info window, and what lets it preview the label and the automap mark by walking the real rules rather than by a second rendering path written for previews.

The stat index deliberately does not go through this. A criterion is answered on the best roll, and the answer carries the range the source rolls, because the question a player is asking is what could grant them a stat. Saying a source as item facts picks one roll and throws the rest away, which is precisely the information a criterion is answered from. Reading the totals off an index entry is also a comparison against a stored range, where a walk is a rule evaluation against a synthesised item, and the search has to stay instant while criteria are being added a row at a time.

## Considered options

Letting the filter's own conditions do the searching, by saying every source as item facts at its best roll and walking rules over the lot, would give one language for both and would let a search become a filter line directly. It was rejected because it flattens the range, because it needs a filter context supplied for the questions that are not about the item at all, such as which class is playing and what filter level is set, and because it puts a rule walk over every source behind every keystroke. Nothing about it is impossible; it answers a different question worse.

Not building the bridge at all, and giving the filter builder a seeding path of its own that reads catalogue records directly, was rejected because it is how a preview comes to disagree with what a game shows. A preview that is a walk cannot drift from a walk.

## Consequences

The item facts seam gains a third implementer, which is the first evidence that the shape ADR 0001 records is a real seam rather than two adapters that happen to share a header. Where the live and packet adapters disagree, a catalogue item is a third answer to compare against, and unlike the other two it can be built in a test without a game.

Sources and stat totals are kept on the index whether or not anything asks for the item facts of a source, so the two representations of the same source exist side by side. They are not free to drift: the item facts are rendered from the same property entries the totals were collected from, and neither is derived from the other.

A criterion cannot reach what the totals mechanism leaves out, which is amounts granted per character level and the properties the game hardcodes and answers from a stand-in stat. A rule walked against a catalogue item does not have that limit, so if a search ever genuinely needs one of those, walking a narrowed set of sources through the bridge is the way in rather than widening the index.
