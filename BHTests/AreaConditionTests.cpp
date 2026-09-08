#include "doctest.h"
#include <vector>
#include "FilterContext.h"
#include "ItemFacts.h"
#include "ItemFilter.h"
#include "RuleFixture.h"

/*
 * AREAID and AREALVL read the area the item is lying in.
 *
 * They are the only conditions about the world that an item answers rather than
 * the character, and it matters at exactly one moment: the drop packets of an
 * area arrive before the client has moved the character into it, so a rule
 * asked about the character's area would judge those first items as though they
 * were still in the area behind them. Working out the item's own area is the
 * game's part and is not reachable from here; that the conditions read it is.
 */

namespace {

// An item lying in an area, with nothing else about it for a rule to read.
ItemFacts AnItemIn(unsigned int areaId, unsigned int areaLevel) {
	ItemFacts facts = {};
	facts.ground = true;
	facts.areaId = areaId;
	facts.areaLevel = areaLevel;
	return facts;
}

bool Matches(const std::string& condition, const ItemFacts& facts) {
	RuleList list;
	list.Add(condition, "%NAME%");

	FilterContext context = {};
	return !MatchingActions(list.Rules(), facts, context, PING_LEVEL_ALL).empty();
}

}  // namespace

TEST_CASE("the area conditions read the item's area, not the character's") {
	// Harrogath at the moment a character standing in it is sent to the Halls
	// of Anguish: the items arriving belong to the area they were sent to.
	ItemFacts inTown = AnItemIn(103, 0);
	ItemFacts inTheHalls = AnItemIn(107, 85);

	CHECK(Matches("AREAID=103", inTown));
	CHECK(Matches("AREALVL=0", inTown));
	CHECK_FALSE(Matches("AREAID=103", inTheHalls));
	CHECK_FALSE(Matches("AREALVL=0", inTheHalls));

	CHECK(Matches("AREAID=107", inTheHalls));
	CHECK(Matches("AREALVL>0", inTheHalls));
	CHECK_FALSE(Matches("AREAID=107", inTown));
	CHECK_FALSE(Matches("AREALVL>0", inTown));
}

TEST_CASE("an item that is not on the ground has no area") {
	ItemFacts held = AnItemIn(107, 85);
	held.ground = false;

	CHECK_FALSE(Matches("AREAID=107", held));
	CHECK_FALSE(Matches("AREALVL>0", held));
}
