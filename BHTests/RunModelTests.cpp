#include "doctest.h"
#include <vector>
#include "Modules/Race/RunModel.h"

/*
 * The runs a character has been timed on, worked out from events.
 *
 * Times are the events' own milliseconds. The model is asked about a moment and
 * measures whatever is still running to it, so nothing here waits on a clock.
 */

using namespace PlayEvents;

namespace {

const unsigned long long kSecond = 1000;

RunModel After(const std::vector<Event>& events) {
	RunModel model;
	for (const Event& event : events)
		model.Apply(event);
	return model;
}

Joined Join(unsigned long long experience, unsigned int level = 80) {
	return Joined{ "baal-1", 2, level, experience };
}

}

namespace {

RunStarted BossRun() {
	return RunStarted{ "bosses-normal", "Act bosses (Normal)", { "Andariel", "Duriel", "Mephisto" } };
}

}

TEST_CASE("a character with no run has none in hand") {
	RunModel model = After({ { 0, Join(0) } });
	CHECK(model.Runs().empty());
	CHECK(model.ActiveRun() == NULL);
	CHECK(model.RunRealMs(10 * kSecond) == 0);
	CHECK(model.RunGameMs(10 * kSecond) == 0);
}

TEST_CASE("a run begins with its route's splits open and its clock at nothing") {
	RunModel model = After({ { 0, Join(0) }, { 10 * kSecond, BossRun() } });

	const RunModel::Run* run = model.ActiveRun();
	REQUIRE(run != NULL);
	CHECK(run->route == "bosses-normal");
	CHECK(run->name == "Act bosses (Normal)");
	REQUIRE(run->splits.size() == 3);
	CHECK(run->splits[0].name == "Andariel");
	CHECK(run->splits[0].state == RunModel::Split::Open);
	CHECK_FALSE(run->ended);
	CHECK(model.RunRealMs(10 * kSecond) == 0);
	CHECK(model.RunRealMs(25 * kSecond) == 15 * kSecond);
}

TEST_CASE("a split is reached at the run's time, with the level the character was") {
	RunModel model = After({
		{ 0, Join(0, 1) },
		{ 10 * kSecond, BossRun() },
		{ 50 * kSecond, LevelReached{ 12 } },
		{ 70 * kSecond, SplitReached{ 0, false } },
	});

	const RunModel::Split& split = model.ActiveRun()->splits[0];
	CHECK(split.state == RunModel::Split::Reached);
	CHECK_FALSE(split.manual);
	CHECK(split.realMs == 60 * kSecond);
	CHECK(split.gameMs == 60 * kSecond);
	CHECK(split.level == 12);
}

TEST_CASE("a run's time in game leaves out what was spent between games") {
	RunModel model = After({
		{ 0, Join(0) },
		{ 10 * kSecond, BossRun() },
		{ 40 * kSecond, Left() },
		{ 100 * kSecond, Join(0) },
		{ 130 * kSecond, SplitReached{ 0, false } },
	});

	CHECK(model.ActiveRun()->splits[0].realMs == 120 * kSecond);
	CHECK(model.ActiveRun()->splits[0].gameMs == 60 * kSecond);
	CHECK(model.RunRealMs(140 * kSecond) == 130 * kSecond);
	CHECK(model.RunGameMs(140 * kSecond) == 70 * kSecond);
}

TEST_CASE("splits are reached in whatever order they come") {
	RunModel model = After({
		{ 0, Join(0) },
		{ 0, BossRun() },
		{ 30 * kSecond, SplitReached{ 2, false } },
		{ 50 * kSecond, SplitReached{ 0, true } },
	});

	const RunModel::Run* run = model.ActiveRun();
	CHECK(run->splits[0].state == RunModel::Split::Reached);
	CHECK(run->splits[0].manual);
	CHECK(run->splits[1].state == RunModel::Split::Open);
	CHECK(run->splits[2].realMs == 30 * kSecond);
}

TEST_CASE("a split taken back is open again, and one passed over is neither") {
	RunModel model = After({
		{ 0, Join(0) },
		{ 0, BossRun() },
		{ 30 * kSecond, SplitReached{ 0, false } },
		{ 40 * kSecond, SplitUndone{ 0 } },
		{ 50 * kSecond, SplitSkipped{ 1 } },
	});

	const RunModel::Run* run = model.ActiveRun();
	CHECK(run->splits[0].state == RunModel::Split::Open);
	CHECK(run->splits[0].name == "Andariel");
	CHECK(run->splits[0].realMs == 0);
	CHECK(run->splits[1].state == RunModel::Split::Skipped);
	CHECK(run->splits[1].name == "Duriel");
}

TEST_CASE("a run that ends keeps how long it ran, and is no longer in hand") {
	RunModel model = After({
		{ 0, Join(0) },
		{ 10 * kSecond, BossRun() },
		{ 40 * kSecond, Left() },
		{ 100 * kSecond, Join(0) },
		{ 130 * kSecond, SplitReached{ 2, false } },
		{ 130 * kSecond, RunEnded{ true } },
	});

	CHECK(model.ActiveRun() == NULL);
	REQUIRE(model.Runs().size() == 1);
	const RunModel::Run& run = model.Runs()[0];
	CHECK(run.ended);
	CHECK(run.finished);
	CHECK(run.realMs == 120 * kSecond);
	CHECK(run.gameMs == 60 * kSecond);
	CHECK(model.RunRealMs(500 * kSecond) == 0);
}

TEST_CASE("a run given up is over and not finished") {
	RunModel model = After({
		{ 0, Join(0) },
		{ 0, BossRun() },
		{ 30 * kSecond, RunEnded{ false } },
	});

	REQUIRE(model.Runs().size() == 1);
	CHECK(model.Runs()[0].ended);
	CHECK_FALSE(model.Runs()[0].finished);
	CHECK(model.Runs()[0].realMs == 30 * kSecond);
}

TEST_CASE("a run begun while another is going is the end of that one") {
	RunModel model = After({
		{ 0, Join(0) },
		{ 0, BossRun() },
		{ 20 * kSecond, SplitReached{ 0, false } },
		{ 45 * kSecond, BossRun() },
	});

	REQUIRE(model.Runs().size() == 2);
	CHECK(model.Runs()[0].ended);
	CHECK_FALSE(model.Runs()[0].finished);
	CHECK(model.Runs()[0].realMs == 45 * kSecond);
	CHECK(model.ActiveRun() == &model.Runs()[1]);
	CHECK(model.ActiveRun()->splits[0].state == RunModel::Split::Open);
}

TEST_CASE("a split of no run, or past the end of one, is nothing") {
	RunModel model = After({
		{ 0, Join(0) },
		{ 5 * kSecond, SplitReached{ 0, false } },
		{ 6 * kSecond, SplitSkipped{ 0 } },
		{ 7 * kSecond, RunEnded{ true } },
		{ 10 * kSecond, BossRun() },
		{ 20 * kSecond, SplitReached{ 3, false } },
		{ 21 * kSecond, SplitUndone{ 9 } },
		{ 22 * kSecond, SplitSkipped{ 9 } },
	});

	REQUIRE(model.Runs().size() == 1);
	for (const RunModel::Split& split : model.ActiveRun()->splits)
		CHECK(split.state == RunModel::Split::Open);
}

TEST_CASE("what else happens in a game is nothing to a run's clocks") {
	RunModel model = After({
		{ 0, Join(0) },
		{ 0, BossRun() },
		{ 5 * kSecond, Quest{ 6, 0x201e } },
		{ 6 * kSecond, Experience{ 5000 } },
		{ 7 * kSecond, Killed{ 131, 571, "Minion of Destruction", TierNormal, 7 } },
	});

	REQUIRE(model.Runs().size() == 1);
	CHECK(model.RunRealMs(10 * kSecond) == 10 * kSecond);
	CHECK(model.ActiveRun()->splits[0].state == RunModel::Split::Open);
}

TEST_CASE("the game being played says which difficulty and level a run starts in") {
	RunModel model = After({ { 0, Joined{ "a", 1, 31, 0 } }, { 5 * kSecond, LevelReached{ 32 } } });
	CHECK(model.InGame());
	CHECK(model.Difficulty() == 1);
	CHECK(model.Level() == 32);

	model.Apply({ 9 * kSecond, Left() });
	CHECK_FALSE(model.InGame());
	CHECK(model.Level() == 32);
}

TEST_CASE("a route not yet run is every split open, with no time on either clock") {
	const RunModel::Run run = RunModel::Unstarted("act1", "Act 1", { "Andariel", "Duriel" });
	CHECK(run.route == "act1");
	CHECK(run.name == "Act 1");
	REQUIRE(run.splits.size() == 2);
	CHECK(run.splits[1].name == "Duriel");
	CHECK(run.splits[1].state == RunModel::Split::Open);
	CHECK_FALSE(run.ended);
	CHECK(run.realMs == 0);
}
