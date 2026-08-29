# Item captures

The item filter's behaviour is recorded by playing rather than described by
hand: what the filter decided about real items, kept so it can be replayed and
compared against later.

## Taking a capture

Set `Capture Item Drops` to `1` in `BH_settings.cfg` and play. Every item that
lands on the ground is appended to `item-captures.txt` beside the config, along
with the decision the filter reached for it. The setting has no entry in the
settings window: it exists to be turned on when a filter is reported as
behaving unexpectedly, and to produce fixtures.

Only items appearing on the ground are recorded, because those are the only
ones the filter sees at this point. An item dropped from your own inventory
arrives as a different kind of message and is not filtered, so it does not
appear. To capture something you already own, drop it, walk far enough away
that it unloads, and walk back: it will arrive as an item coming into view.

What a capture covers is what the session covered. Worth varying:

- **Filter level and ping level.** Roughly half of the shipped filter's rules
  are conditional on the filter level, and the ping level decides whether a
  rule's map and notification actions apply at all. Changing either while
  playing is noticed, and the rules and settings are written out again, so one
  session can cover several combinations.
- **Areas.** Two conditions read the area an item is lying in.
- **Item kinds.** Runes, gems, jewels and items carrying affixes exercise far
  more of the packet reader than potions and gold do.

A capture cannot pin everything. `CHARSTAT` reads character stats as they stood
and `PRICE` asks a question only answerable of an item that already exists as a
game unit; a rule set using either is reported by `contextSensitive` in the
header. A capture also records the character's class and level but not which
character it was, so drops from two characters in one session are only
separable if those differ.

## Turning one into fixtures

    python tools/captures/curate.py <capture.txt> [more captures...]

Writes `BHTests/fixtures/filter-cases.txt`, `parse-cases.txt` and
`tables.txt`. All three are read by the tests through `CaptureFormat`, the same
code the game writes them with.

`tables.txt` holds the rows of the game's own tables that reading a packet
depends on: how wide each stat is written, and the attributes of the item codes
the cases mention. A packet says as little as it can, and without these there is
no way to know where one field ends and the next begins, so a capture carries
them and the fixtures keep the ones something refers to.

A capture is mostly repetition, and curation keeps what carries information: the
first of every packet, item, outcome and area, everything long enough to hold a
stat list, and a sample of the rest. Rules are kept whole, because leaving one
out would renumber the others and change every decision that names one.

Re-running replaces all three files, so folding in a later capture means
passing every capture at once.
