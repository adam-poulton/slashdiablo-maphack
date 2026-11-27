#include "doctest.h"
#include <vector>
#include "Modules/Session/RunEvaluator.h"

/*
 * Which splits of a route the game reaches as a run goes.
 */

using namespace SessionEvents;

namespace {

typedef std::vector<unsigned int> Reached;

const unsigned int kAndariel = 156;
const unsigned int kDuriel = 211;
const unsigned int kMephisto = 242;

Splits::Route Preset(const char* id) {
	const std::vector<Splits::Route> presets = Splits::Presets();
	const Splits::Route* found = Splits::Find(presets, id);
	REQUIRE(found != NULL);
	return *found;
}

RunEvaluator Running(const char* route, unsigned int difficulty = 0, unsigned int level = 1) {
	RunEvaluator evaluator;
	evaluator.Start(Preset(route), difficulty, level);
	return evaluator;
}

Notable Boss(unsigned int classId) {
	return Notable{ 37, classId, "A boss", "A boss", TierBoss, -1 };
}

}

TEST_CASE("nothing is reached while no run is going") {
	RunEvaluator evaluator;
	CHECK_FALSE(evaluator.Running());
	CHECK(evaluator.Feed(Boss(kAndariel)).empty());
}

TEST_CASE("an act boss dying in view reaches its split") {
	RunEvaluator evaluator = Running("bosses-normal");
	CHECK(evaluator.Feed(Boss(kAndariel)) == Reached{ 0 });
	CHECK(evaluator.IsClosed(0));
	CHECK(evaluator.FirstOpen() == 1);
}

TEST_CASE("the quest for a boss reaches its split where the death went unseen") {
	RunEvaluator evaluator = Running("bosses-normal");
	CHECK(evaluator.Feed(Quest{ 6, 0x0004 }).empty());
	CHECK(evaluator.Feed(Quest{ 6, 0x201e }) == Reached{ 0 });
}

TEST_CASE("a split is reached once, by whichever of its marks comes first") {
	RunEvaluator evaluator = Running("bosses-normal");
	CHECK(evaluator.Feed(Boss(kAndariel)) == Reached{ 0 });
	CHECK(evaluator.Feed(Quest{ 6, 0x201e }).empty());
	CHECK(evaluator.Feed(Boss(kAndariel)).empty());
}

TEST_CASE("Duriel's split is reached by the bit his death sets") {
	RunEvaluator evaluator = Running("bosses-normal");
	// Under way, and then "talk to Jerhyn".
	CHECK(evaluator.Feed(Quest{ 14, 0x000c }).empty());
	CHECK(evaluator.Feed(Quest{ 14, 0x002c }) == Reached{ 1 });
}

TEST_CASE("a boss that died among ordinary monsters reaches its split all the same") {
	RunEvaluator evaluator = Running("bosses-normal");
	CHECK(evaluator.Feed(Killed{ 37, kAndariel, "Andariel", TierNormal, 1 }) == Reached{ 0 });
}

TEST_CASE("splits are reached in whatever order the game reaches them") {
	RunEvaluator evaluator = Running("bosses-normal");
	CHECK(evaluator.Feed(Boss(kMephisto)) == Reached{ 2 });
	CHECK(evaluator.FirstOpen() == 0);
	CHECK(evaluator.Feed(Boss(kAndariel)) == Reached{ 0 });
	CHECK(evaluator.FirstOpen() == 1);
}

TEST_CASE("a split is reached in its own difficulty and no other") {
	RunEvaluator evaluator = Running("bosses-hell");
	CHECK(evaluator.Feed(Boss(kAndariel)) == Reached{ 0 });

	CHECK(evaluator.Feed(Joined{ "a", 1, 40, 0 }).empty());
	CHECK(evaluator.Feed(Boss(kAndariel)) == Reached{ 5 });
	CHECK(evaluator.Feed(Boss(kDuriel)) == Reached{ 6 });

	CHECK(evaluator.Feed(Joined{ "a", 2, 70, 0 }).empty());
	CHECK(evaluator.Feed(Quest{ 6, 0x0002 }) == Reached{ 10 });
}

TEST_CASE("a Normal split is not reached by what happens in Nightmare") {
	RunEvaluator evaluator = Running("bosses-normal", 1);
	CHECK(evaluator.Feed(Boss(kAndariel)).empty());
	CHECK(evaluator.Feed(Quest{ 6, 0x0003 }).empty());
}

TEST_CASE("an item coming into the character's keeping reaches its split") {
	RunEvaluator evaluator = Running("milestones-normal");
	CHECK(evaluator.Feed(ItemHeld{ "rin" }).empty());
	CHECK(evaluator.Feed(ItemHeld{ "box" }) == Reached{ 2 });
}

TEST_CASE("a super unique dying in view reaches its split") {
	RunEvaluator evaluator = Running("milestones-normal");
	CHECK(evaluator.Feed(Notable{ 110, 479, "Shenk the Overseer", "Overseer", TierSuperUnique, 42 }) ==
		Reached{ 13 });
}

TEST_CASE("an area entered reaches a split marked by it") {
	Splits::Route route = { "mine", "Mine", { { "Throne", -1, false,
		{ { Splits::Trigger::Area, 131, 0, "" } } } } };
	RunEvaluator evaluator;
	evaluator.Start(route, 2, 80);

	CHECK(evaluator.Feed(AreaEntered{ 130, false, "" }).empty());
	CHECK(evaluator.Feed(AreaEntered{ 131, false, "" }) == Reached{ 0 });
}

TEST_CASE("a level reached reaches its split, and every level past it does not again") {
	RunEvaluator evaluator = Running("completionist-normal");
	CHECK(evaluator.Feed(LevelReached{ 39 }).empty());
	CHECK(evaluator.Feed(LevelReached{ 40 }) == Reached{ 2 });
	CHECK(evaluator.Feed(LevelReached{ 41 }).empty());
}

TEST_CASE("what was already so when the run began reaches nothing") {
	RunEvaluator evaluator = Running("completionist-normal", 0, 45);
	CHECK(evaluator.IsClosed(2));
	CHECK_FALSE(evaluator.IsClosed(0));
	CHECK(evaluator.Feed(LevelReached{ 46 }).empty());
}

TEST_CASE("a split that wants every goal is reached by the last of them") {
	RunEvaluator evaluator = Running("completionist-normal");

	Reached reached;
	for (unsigned int quest : Splits::QuestIds()) {
		reached = evaluator.Feed(Quest{ quest, 0x0001 });
		if (quest != Splits::QuestIds().back())
			CHECK(reached.empty());
	}
	CHECK(reached == Reached{ 0 });

	// The Cow King's bit is one of The Search for Cain's.
	CHECK(evaluator.Feed(Quest{ 4, 0x0401 }) == Reached{ 1 });
	// The level is the last goal, so reaching it reaches the end as well.
	CHECK(evaluator.Feed(LevelReached{ 40 }) == Reached{ 2, 3 });
}

TEST_CASE("a quest keeps the bits it has had set, though the game clears them after") {
	RunEvaluator evaluator = Running("completionist-normal");
	evaluator.Feed(Quest{ 4, 0x0400 });
	evaluator.Feed(Quest{ 4, 0x0001 });
	CHECK(evaluator.IsClosed(1));
}

TEST_CASE("a split the player dealt with is not then reached by the game") {
	RunEvaluator evaluator = Running("bosses-normal");
	evaluator.Close(0);
	CHECK(evaluator.Feed(Boss(kAndariel)).empty());
	CHECK(evaluator.FirstOpen() == 1);
}

TEST_CASE("a split taken back is open, and is not reached again by what is still so") {
	RunEvaluator evaluator = Running("bosses-normal");
	evaluator.Feed(Boss(kAndariel));
	evaluator.Reopen(0);

	CHECK_FALSE(evaluator.IsClosed(0));
	CHECK(evaluator.FirstOpen() == 0);
	CHECK(evaluator.Feed(Experience{ 500 }).empty());
	CHECK(evaluator.Feed(Boss(kAndariel)).empty());
}

TEST_CASE("a split nothing marks is never reached by the game") {
	Splits::Route route = { "mine", "Mine", { { "By hand", -1, false, {} },
		{ "By hand, all", -1, true, {} } } };
	RunEvaluator evaluator;
	evaluator.Start(route, 0, 1);

	CHECK_FALSE(evaluator.IsClosed(0));
	CHECK_FALSE(evaluator.IsClosed(1));
	CHECK(evaluator.Feed(Boss(kAndariel)).empty());
}

TEST_CASE("a run stopped reaches nothing more") {
	RunEvaluator evaluator = Running("bosses-normal");
	evaluator.Stop();
	CHECK(evaluator.Feed(Boss(kAndariel)).empty());
}

TEST_CASE("starting again forgets what the run before it had done") {
	RunEvaluator evaluator = Running("bosses-normal");
	evaluator.Feed(Boss(kAndariel));
	evaluator.Start(Preset("bosses-normal"), 0, 1);

	CHECK(evaluator.FirstOpen() == 0);
	CHECK(evaluator.Feed(Boss(kAndariel)) == Reached{ 0 });
}
