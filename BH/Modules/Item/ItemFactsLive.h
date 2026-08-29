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
