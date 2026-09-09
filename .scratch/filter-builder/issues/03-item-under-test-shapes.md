# Unidentified and rolled: what shapes an item under test can take

Type: grilling
Status: open

## Question

ADR 0004 established the catalogue item: a source said as item facts at one of
the rolls it can take. Test and simulation both need more than that.

Simulation shows drops **unidentified, as they would drop**, which is not the
same thing as a source at a roll: an unidentified unique shows its base name,
and the conditions that read its stats must answer the way they answer for a
real unidentified drop rather than reading through to the roll underneath.

Decide whether unidentified is a state a catalogue item carries or a fourth
shape at the `ItemFacts` seam beside live, packet and catalogue. Decide what
each condition answers for an item whose stats are not readable yet, and how
that squares with ADR 0002 (an unknown fact stops the whole rule). Decide what
a rolled-but-unidentified item is even for, given the roll is exactly what the
player cannot see.

This is the sharpest technical unknown on the map and several tickets wait on it.
