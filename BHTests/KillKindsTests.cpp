#include "doctest.h"
#include "Modules/Session/KillKinds.h"

/*
 * Which deaths are kills, and what kind of monster each was.
 *
 * The kinds here are rows of the game's MonStats.txt, with the flags that table
 * gives them.
 */

using namespace KillKinds;
using namespace SessionEvents;

namespace {

Kind Monster(unsigned int classId) {
	return { classId, true, false, false, 0 };
}

}

TEST_CASE("a monster that can be killed and fights the player is a kill") {
	CHECK(Counts(Monster(571)));	// a Minion of Destruction
	CHECK(Counts(Monster(544)));	// Baal
}

TEST_CASE("what cannot be killed is not a kill") {
	Kind meteor = Monster(153);
	meteor.killable = false;
	CHECK_FALSE(Counts(meteor));
}

TEST_CASE("a townsperson is not a kill") {
	Kind guard = Monster(203);
	guard.npc = true;
	guard.align = 1;
	CHECK_FALSE(Counts(guard));
}

TEST_CASE("what fights for the player is not a kill") {
	Kind mercenary = Monster(338);
	mercenary.align = 1;
	CHECK_FALSE(Counts(mercenary));

	Kind golem = Monster(289);
	golem.align = 1;
	golem.neverCount = true;
	CHECK_FALSE(Counts(golem));
}

TEST_CASE("a wall, a door or a bone prison is not a kill, though it can be broken") {
	Kind bonePrison = Monster(340);
	bonePrison.neverCount = true;
	CHECK_FALSE(Counts(bonePrison));

	Kind barricade = Monster(524);
	barricade.neverCount = true;
	CHECK_FALSE(Counts(barricade));
}

TEST_CASE("Baal's tentacles and his clone are part of his fight, not kills") {
	for (unsigned int tentacle = 562; tentacle <= 566; tentacle++)
		CHECK_FALSE(Counts(Monster(tentacle)));
	CHECK_FALSE(Counts(Monster(570)));

	// The monsters on either side of them in the table are kills as usual.
	CHECK(Counts(Monster(561)));
	CHECK(Counts(Monster(567)));
	CHECK(Counts(Monster(571)));
}

TEST_CASE("a monster is the most that can be said of it") {
	CHECK(TierOf({ false, false, false, false, false }) == TierNormal);
	CHECK(TierOf({ false, false, false, false, true }) == TierMinion);
	CHECK(TierOf({ false, false, false, true, false }) == TierChampion);
	CHECK(TierOf({ false, false, true, false, false }) == TierUnique);
}

TEST_CASE("a super unique is marked unique as well, and is a super unique") {
	CHECK(TierOf({ false, true, true, false, false }) == TierSuperUnique);
}

TEST_CASE("an act boss is a boss whatever else it is marked") {
	CHECK(TierOf({ true, false, false, false, false }) == TierBoss);
	CHECK(TierOf({ true, true, true, false, false }) == TierBoss);
}

TEST_CASE("the tiers run from the least to the most") {
	CHECK(TierNormal < TierMinion);
	CHECK(TierMinion < TierChampion);
	CHECK(TierChampion < TierUnique);
	CHECK(TierUnique < TierSuperUnique);
	CHECK(TierSuperUnique < TierBoss);
}
