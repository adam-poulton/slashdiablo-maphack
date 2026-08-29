#pragma once
#include <vector>
#include "ItemFacts.h"

struct UnitAny;

/*
 * An item that already exists in the world, answering as an item from a packet
 * does.
 *
 * The game will answer two different questions about an item's stats and does
 * not promise the same number to both. Asked for a stat it adds it up itself,
 * applying whatever it applies; asked for the list it hands over the entries as
 * stored. Conditions here use both, so both are offered, and each is forwarded
 * to the call the conditions were already making rather than worked out again
 * from the other. ADR 0001 is why.
 */
class LiveStats : public StatSource {
public:
	explicit LiveStats(UnitAny* item) : item(item), built(false) {}

	int Stat(unsigned int stat, unsigned int sub) const override;

	// Copied out on the first ask and kept, since a condition reading the list
	// usually walks all of it and several conditions may ask about one item.
	const std::vector<StatEntry>& Stats() const override;

private:
	UnitAny* item;
	mutable std::vector<StatEntry> entries;
	mutable bool built;
};

/*
 * An item in the world, offered both ways at once.
 *
 * A condition that has been collapsed onto one body reads the facts; one that
 * has not yet reads the unit. Holding both here is what lets those be converted
 * a few at a time rather than all at once, and when the last one is done the
 * unit goes and this is simply how an item in the world becomes facts.
 *
 * Only what the collapsed conditions ask for is filled in. The rest of an item
 * in the world is still read from the game as it is asked for, which is what
 * the stats are for.
 */
// How many of an item's sockets are filled.
unsigned int GetUsedSockets(UnitAny *item);

class LiveItem {
public:
	explicit LiveItem(UnitAny* item);

	// False when the item's code is not one the data tables describe. Nothing
	// is filled in that case, which is the answer this replaced also gave.
	bool Known() const { return known; }

	UnitItemInfo& Unit() { return unit; }
	const ItemFacts& Facts() const { return facts; }

private:
	LiveStats stats;
	UnitItemInfo unit;
	ItemFacts facts;
	bool known;
};
