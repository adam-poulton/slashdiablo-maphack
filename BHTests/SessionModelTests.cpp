#include "doctest.h"
#include <vector>
#include "Modules/Session/SessionModel.h"

/*
 * What a session comes to, worked out from its events.
 *
 * Times are the events' own milliseconds. The model is asked about a moment and
 * measures whatever is still running to it, so nothing here waits on a clock.
 */

using namespace SessionEvents;

namespace {

const unsigned long long kSecond = 1000;
const unsigned long long kMinute = 60 * kSecond;

SessionModel After(const std::vector<Event>& events) {
	SessionModel model;
	for (const Event& event : events)
		model.Apply(event);
	return model;
}

Joined Join(unsigned long long experience, unsigned int level = 80) {
	return Joined{ "baal-1", 2, level, experience };
}

}

TEST_CASE("a session nothing has happened in has nothing to show") {
	SessionModel model;
	CHECK_FALSE(model.HasStarted());
	CHECK_FALSE(model.InGame());
	CHECK(model.Games().empty());
	CHECK(model.PlayedMs(10 * kMinute) == 0);
	CHECK(model.GameMs(10 * kMinute) == 0);
	CHECK(model.PlayedRate(10 * kMinute) == 0.0);
	CHECK(model.SessionRate(0) == 0.0);
}

TEST_CASE("the first line of a session says whose it is, and only the first") {
	Started first = {};
	first.character = "Sorc_One";
	Started second = {};
	second.character = "Someone_Else";

	SessionModel model = After({ { 0, first }, { 10, second } });
	CHECK(model.HasStarted());
	CHECK(model.Header().character == "Sorc_One");
}

TEST_CASE("a game runs from joining it to leaving it") {
	SessionModel model = After({
		{ 2 * kSecond, Join(1000) },
		{ 92 * kSecond, Left() },
	});

	REQUIRE(model.Games().size() == 1);
	const SessionModel::Game& game = model.Games()[0];
	CHECK(game.name == "baal-1");
	CHECK(game.difficulty == 2);
	CHECK_FALSE(game.open);
	CHECK(game.joinedAt == 2 * kSecond);
	CHECK(game.leftAt == 92 * kSecond);
	CHECK(model.PlayedMs(500 * kSecond) == 90 * kSecond);
	CHECK(model.GameMs(500 * kSecond) == 0);
}

TEST_CASE("a game still being played is measured to the moment asked about") {
	SessionModel model = After({ { 2 * kSecond, Join(1000) } });

	CHECK(model.InGame());
	CHECK(model.GameMs(32 * kSecond) == 30 * kSecond);
	CHECK(model.PlayedMs(32 * kSecond) == 30 * kSecond);
	CHECK(model.GameMs(1 * kSecond) == 0);
}

TEST_CASE("time between games is the session's but is not time played") {
	SessionModel model = After({
		{ 0, Join(1000) },
		{ 60 * kSecond, Left() },
		{ 100 * kSecond, Join(1000) },
	});

	CHECK(model.Games().size() == 2);
	CHECK(model.PlayedMs(130 * kSecond) == 90 * kSecond);
	CHECK(model.GameMs(130 * kSecond) == 30 * kSecond);
}

TEST_CASE("experience gained is what the total rose by in a game") {
	SessionModel model = After({
		{ 0, Join(1000) },
		{ 10 * kSecond, Experience{ 1400 } },
		{ 20 * kSecond, Experience{ 2000 } },
	});

	CHECK(model.Gained() == 1000);
	CHECK(model.Lost() == 0);
	CHECK(model.Games()[0].gained == 1000);
	CHECK(model.Games()[0].startExperience == 1000);
	CHECK(model.Games()[0].experience == 2000);
}

TEST_CASE("what a character gained outside the session's games is no part of it") {
	SessionModel model = After({
		{ 0, Join(1000) },
		{ 10 * kSecond, Experience{ 1500 } },
		{ 20 * kSecond, Left() },
		{ 30 * kSecond, Join(9000) },
		{ 40 * kSecond, Experience{ 9100 } },
	});

	CHECK(model.Gained() == 600);
	CHECK(model.Games()[1].gained == 100);
}

TEST_CASE("experience a death takes is counted apart from what was gained") {
	SessionModel model = After({
		{ 0, Join(10000) },
		{ 10 * kSecond, Experience{ 13000 } },
		{ 20 * kSecond, Died() },
		{ 20 * kSecond, Experience{ 12000 } },
	});

	CHECK(model.Gained() == 3000);
	CHECK(model.Lost() == 1000);
	CHECK(model.Deaths() == 1);
	CHECK(model.Games()[0].deaths == 1);
	CHECK(model.Games()[0].lost == 1000);
}

TEST_CASE("an experience total that has not moved is not a change") {
	SessionModel model = After({
		{ 0, Join(1000) },
		{ 10 * kSecond, Experience{ 1000 } },
	});

	CHECK(model.Gained() == 0);
	CHECK(model.Lost() == 0);
}

TEST_CASE("experience past what 32 bits hold is counted whole") {
	SessionModel model = After({
		{ 0, Join(3520485254ull, 99) },
		{ 10 * kSecond, Experience{ 3520485254ull + 5000000000ull } },
	});

	CHECK(model.Gained() == 5000000000ull);
}

TEST_CASE("the rate of a game is what it has netted over how long it has run") {
	SessionModel model = After({
		{ 0, Join(0) },
		{ 5 * kSecond, Experience{ 3000 } },
		{ 8 * kSecond, Experience{ 2000 } },
	});

	CHECK(model.GameRate(10 * kSecond) == doctest::Approx(200.0));
}

TEST_CASE("a rate over no time at all is nothing, not a division by it") {
	SessionModel model = After({
		{ 5 * kSecond, Join(0) },
		{ 5 * kSecond, Experience{ 3000 } },
	});

	CHECK(model.GameRate(5 * kSecond) == 0.0);
	CHECK(model.PlayedRate(5 * kSecond) == 0.0);
}

TEST_CASE("the session's rates are over time played, and over all of it") {
	SessionModel model = After({
		{ 0, Join(0) },
		{ 50 * kSecond, Experience{ 6000 } },
		{ 60 * kSecond, Left() },
		{ 100 * kSecond, Join(6000) },
		{ 110 * kSecond, Experience{ 9000 } },
	});

	// 9000 over 60 s + 40 s played, and over the 200 s since the session began.
	CHECK(model.PlayedRate(140 * kSecond) == doctest::Approx(90.0));
	CHECK(model.SessionRate(200 * kSecond) == doctest::Approx(45.0));
	CHECK(model.GameRate(140 * kSecond) == doctest::Approx(75.0));
}

TEST_CASE("the recent rate counts only what changed inside its window") {
	SessionModel model = After({
		{ 0, Join(0) },
		{ 10 * kSecond, Experience{ 100000 } },
		{ 150 * kSecond, Experience{ 106000 } },
	});

	// The last minute of a game three minutes old holds the second gain alone.
	CHECK(model.RecentRate(180 * kSecond, kMinute) == doctest::Approx(100.0));
}

TEST_CASE("the recent rate is over the part of its window spent in a game") {
	SessionModel model = After({
		{ 0, Join(0) },
		{ 20 * kSecond, Left() },
		{ 170 * kSecond, Join(0) },
		{ 175 * kSecond, Experience{ 3000 } },
	});

	// Ten seconds of the last minute were played.
	CHECK(model.RecentRate(180 * kSecond, kMinute) == doctest::Approx(300.0));
}

TEST_CASE("the recent rate of a window with no play in it is nothing") {
	SessionModel model = After({
		{ 0, Join(0) },
		{ 10 * kSecond, Experience{ 5000 } },
		{ 20 * kSecond, Left() },
	});

	CHECK(model.RecentRate(10 * kMinute, kMinute) == 0.0);
}

TEST_CASE("the recent rate looks back no further than it keeps") {
	SessionModel model = After({
		{ 0, Join(0) },
		{ 1 * kMinute, Experience{ 600000 } },
		{ 9 * kMinute, Experience{ 630000 } },
	});

	// Asked for an hour, it answers for five minutes: 30000 over 300 s.
	CHECK(model.RecentRate(10 * kMinute, 60 * kMinute) == doctest::Approx(100.0));
}

TEST_CASE("levels gained are counted across games, from the level each was joined at") {
	SessionModel model = After({
		{ 0, Join(0, 10) },
		{ 10 * kSecond, LevelReached{ 11 } },
		{ 20 * kSecond, LevelReached{ 13 } },
		{ 30 * kSecond, Left() },
		{ 40 * kSecond, Join(0, 20) },
		{ 50 * kSecond, LevelReached{ 21 } },
	});

	CHECK(model.LevelsGained() == 4);
	CHECK(model.Games()[0].startLevel == 10);
	CHECK(model.Games()[0].level == 13);
	CHECK(model.Games()[1].level == 21);
}

TEST_CASE("a level that is not higher is not a level gained") {
	SessionModel model = After({
		{ 0, Join(0, 10) },
		{ 10 * kSecond, LevelReached{ 10 } },
		{ 20 * kSecond, LevelReached{ 9 } },
	});

	CHECK(model.LevelsGained() == 0);
	CHECK(model.Games()[0].level == 10);
}

TEST_CASE("time is kept for each area, and for a town as a town") {
	SessionModel model = After({
		{ 0, Join(0) },
		{ 0, AreaEntered{ 109, true } },
		{ 15 * kSecond, AreaEntered{ 131, false } },
		{ 75 * kSecond, AreaEntered{ 109, true } },
		{ 80 * kSecond, Left() },
	});

	REQUIRE(model.Areas().size() == 2);
	CHECK(model.Areas().at(109).town);
	CHECK_FALSE(model.Areas().at(131).town);
	CHECK(model.Areas().at(109).entries == 2);
	CHECK(model.AreaMs(109, 500 * kSecond) == 20 * kSecond);
	CHECK(model.AreaMs(131, 500 * kSecond) == 60 * kSecond);
}

TEST_CASE("the area the character is standing in is measured to the moment asked about") {
	SessionModel model = After({
		{ 0, Join(0) },
		{ 0, AreaEntered{ 109, true } },
		{ 10 * kSecond, AreaEntered{ 131, false } },
	});

	CHECK(model.AreaMs(131, 40 * kSecond) == 30 * kSecond);
	CHECK(model.AreaMs(109, 40 * kSecond) == 10 * kSecond);
	CHECK(model.AreaMs(999, 40 * kSecond) == 0);
}

TEST_CASE("experience is credited to the area it was earned in") {
	SessionModel model = After({
		{ 0, Join(0) },
		{ 0, AreaEntered{ 129, false } },
		{ 10 * kSecond, Experience{ 400 } },
		{ 20 * kSecond, AreaEntered{ 131, false } },
		{ 30 * kSecond, Experience{ 1000 } },
		{ 40 * kSecond, Experience{ 900 } },
	});

	CHECK(model.Areas().at(129).gained == 400);
	CHECK(model.Areas().at(131).gained == 600);
}

TEST_CASE("an area's time carries over from one game to the next") {
	SessionModel model = After({
		{ 0, Join(0) },
		{ 0, AreaEntered{ 131, false } },
		{ 30 * kSecond, Left() },
		{ 50 * kSecond, Join(0) },
		{ 50 * kSecond, AreaEntered{ 131, false } },
	});

	CHECK(model.AreaMs(131, 60 * kSecond) == 40 * kSecond);
}

TEST_CASE("a game the log never closed ends at the last thing the log said") {
	SessionModel model = After({
		{ 0, Join(0) },
		{ 0, AreaEntered{ 131, false } },
		{ 40 * kSecond, Experience{ 500 } },
		{ 300 * kSecond, Join(500) },
	});

	REQUIRE(model.Games().size() == 2);
	CHECK_FALSE(model.Games()[0].open);
	CHECK(model.Games()[0].leftAt == 40 * kSecond);
	CHECK(model.PlayedMs(310 * kSecond) == 50 * kSecond);
	CHECK(model.AreaMs(131, 310 * kSecond) == 40 * kSecond);
}

TEST_CASE("what belongs inside a game is dropped outside one") {
	SessionModel model = After({
		{ 0, Experience{ 5000 } },
		{ 0, LevelReached{ 50 } },
		{ 0, Died() },
		{ 0, AreaEntered{ 131, false } },
		{ 0, Players{ 8 } },
		{ 10, Left() },
	});

	CHECK(model.Games().empty());
	CHECK(model.Gained() == 0);
	CHECK(model.LevelsGained() == 0);
	CHECK(model.Deaths() == 0);
	CHECK(model.Areas().empty());
}

TEST_CASE("time does not run backwards for an event stamped before the last") {
	SessionModel model = After({
		{ 60 * kSecond, Join(0) },
		{ 30 * kSecond, Left() },
	});

	CHECK(model.Games()[0].leftAt == 60 * kSecond);
	CHECK(model.PlayedMs(100 * kSecond) == 0);
}

TEST_CASE("a game remembers the most players it held at once") {
	SessionModel model = After({
		{ 0, Join(0) },
		{ 0, Players{ 3 } },
		{ 10 * kSecond, Players{ 8 } },
		{ 20 * kSecond, Players{ 5 } },
	});

	CHECK(model.Games()[0].players == 8);
}

TEST_CASE("kills are counted by tier, for the session, the game, the area and the kind") {
	SessionModel model = After({
		{ 0, Join(0) },
		{ 0, AreaEntered{ 131, false, "Throne of Destruction" } },
		{ 1 * kSecond, Killed{ 131, 571, "Minion of Destruction", TierNormal, 4 } },
		{ 2 * kSecond, Killed{ 131, 571, "Minion of Destruction", TierChampion, 2 } },
		{ 3 * kSecond, Killed{ 131, 641, "Hell Witch", TierMinion, 1 } },
	});

	CHECK(model.KillTotals().Total() == 7);
	CHECK(model.KillTotals().byTier[TierNormal] == 4);
	CHECK(model.KillTotals().byTier[TierChampion] == 2);
	CHECK(model.KillTotals().byTier[TierMinion] == 1);
	CHECK(model.KillTotals().Uniques() == 0);
	CHECK(model.Games()[0].kills == 7);
	CHECK(model.Areas().at(131).kills.Total() == 7);
	CHECK(model.Areas().at(131).name == "Throne of Destruction");

	REQUIRE(model.Monsters().size() == 2);
	CHECK(model.Monsters().at(571).name == "Minion of Destruction");
	CHECK(model.Monsters().at(571).kills.Total() == 6);
	CHECK(model.Monsters().at(571).kills.byTier[TierChampion] == 2);
	CHECK(model.Monsters().at(641).kills.Total() == 1);
}

TEST_CASE("a unique monster is a kill, and is remembered by name") {
	SessionModel model = After({
		{ 0, Join(0) },
		{ 0, AreaEntered{ 131, false, "Throne of Destruction" } },
		{ 5 * kSecond, Notable{ 131, 571, "Lister the Tormentor", "Minion of Destruction", TierSuperUnique, 65 } },
		{ 6 * kSecond, AreaEntered{ 132, false, "The Worldstone Chamber" } },
		{ 9 * kSecond, Notable{ 132, 544, "Baal", "Baal", TierBoss, -1 } },
	});

	CHECK(model.KillTotals().Total() == 2);
	CHECK(model.KillTotals().Uniques() == 2);
	CHECK(model.KillTotals().byTier[TierSuperUnique] == 1);
	CHECK(model.KillTotals().byTier[TierBoss] == 1);
	CHECK(model.Areas().at(132).kills.byTier[TierBoss] == 1);

	REQUIRE(model.Notables().size() == 2);
	CHECK(model.Notables()[0].at == 5 * kSecond);
	CHECK(model.Notables()[0].name == "Lister the Tormentor");
	CHECK(model.Notables()[0].area == 131);
	CHECK(model.Notables()[0].game == 0);
	CHECK(model.Notables()[1].tier == TierBoss);

	// Under the kind of monster it is, which is not what it is called.
	CHECK(model.Monsters().at(571).name == "Minion of Destruction");
	CHECK(model.Monsters().at(571).kills.byTier[TierSuperUnique] == 1);
}

TEST_CASE("kills in a later game are that game's, and the session's as well") {
	SessionModel model = After({
		{ 0, Join(0) },
		{ 1 * kSecond, Killed{ 131, 571, "Minion of Destruction", TierNormal, 4 } },
		{ 10 * kSecond, Left() },
		{ 20 * kSecond, Join(0) },
		{ 21 * kSecond, Killed{ 131, 571, "Minion of Destruction", TierNormal, 3 } },
	});

	CHECK(model.Games()[0].kills == 4);
	CHECK(model.Games()[1].kills == 3);
	CHECK(model.KillTotals().Total() == 7);
}

TEST_CASE("a kill of a tier there is none of is no kill") {
	SessionModel model = After({
		{ 0, Join(0) },
		{ 1 * kSecond, Killed{ 131, 571, "x", TierCount, 4 } },
		{ 2 * kSecond, Notable{ 131, 571, "x", "x", TierCount, -1 } },
	});

	CHECK(model.KillTotals().Total() == 0);
	CHECK(model.Notables().empty());
}

TEST_CASE("drops are kept in order, each with the game it fell in") {
	SessionModel model = After({
		{ 0, Join(0) },
		{ 5 * kSecond, Dropped{ 1001, "Ist Rune", "r24", 2, 8, 1, 131, "Throne of Destruction" } },
		{ 10 * kSecond, Left() },
		{ 20 * kSecond, Join(0) },
		{ 25 * kSecond, Dropped{ 1001, "Ring", "rin", 6, 9, 3, 108, "Chaos Sanctuary" } },
	});

	REQUIRE(model.Drops().size() == 2);
	CHECK(model.Drops()[0].at == 5 * kSecond);
	CHECK(model.Drops()[0].game == 0);
	CHECK(model.Drops()[0].item.name == "Ist Rune");
	CHECK(model.Drops()[1].game == 1);
	CHECK(model.Drops()[1].item.areaName == "Chaos Sanctuary");
	CHECK(model.Games()[0].drops == 1);
	CHECK(model.Games()[1].drops == 1);
}

TEST_CASE("a drop names an area the log had entered without a name") {
	SessionModel model = After({
		{ 0, Join(0) },
		{ 0, AreaEntered{ 131, false } },
		{ 5 * kSecond, Dropped{ 1001, "Ist Rune", "r24", 2, 8, 1, 131, "Throne of Destruction" } },
	});

	CHECK(model.Areas().at(131).name == "Throne of Destruction");
}

TEST_CASE("kills and drops outside a game are dropped") {
	SessionModel model = After({
		{ 0, Killed{ 131, 571, "Minion of Destruction", TierNormal, 4 } },
		{ 0, Notable{ 132, 544, "Baal", "Baal", TierBoss, -1 } },
		{ 0, Dropped{ 1001, "Ist Rune", "r24", 2, 8, 1, 131, "" } },
	});

	CHECK(model.KillTotals().Total() == 0);
	CHECK(model.Monsters().empty());
	CHECK(model.Notables().empty());
	CHECK(model.Drops().empty());
	CHECK(model.Areas().empty());
}
namespace {

RunStarted BossRun() {
	return RunStarted{ "bosses-normal", "Act bosses (Normal)", { "Andariel", "Duriel", "Mephisto" } };
}

}

TEST_CASE("a session with no run has none in hand") {
	SessionModel model = After({ { 0, Join(0) } });
	CHECK(model.Runs().empty());
	CHECK(model.ActiveRun() == NULL);
	CHECK(model.RunRealMs(10 * kSecond) == 0);
	CHECK(model.RunGameMs(10 * kSecond) == 0);
}

TEST_CASE("a run begins with its route's splits open and its clock at nothing") {
	SessionModel model = After({ { 0, Join(0) }, { 10 * kSecond, BossRun() } });

	const SessionModel::Run* run = model.ActiveRun();
	REQUIRE(run != NULL);
	CHECK(run->route == "bosses-normal");
	CHECK(run->name == "Act bosses (Normal)");
	REQUIRE(run->splits.size() == 3);
	CHECK(run->splits[0].name == "Andariel");
	CHECK(run->splits[0].state == SessionModel::RunSplit::Open);
	CHECK_FALSE(run->ended);
	CHECK(model.RunRealMs(10 * kSecond) == 0);
	CHECK(model.RunRealMs(25 * kSecond) == 15 * kSecond);
}

TEST_CASE("a split is reached at the run's time, with the level the character was") {
	SessionModel model = After({
		{ 0, Join(0, 1) },
		{ 10 * kSecond, BossRun() },
		{ 50 * kSecond, LevelReached{ 12 } },
		{ 70 * kSecond, SplitReached{ 0, false } },
	});

	const SessionModel::RunSplit& split = model.ActiveRun()->splits[0];
	CHECK(split.state == SessionModel::RunSplit::Reached);
	CHECK_FALSE(split.manual);
	CHECK(split.realMs == 60 * kSecond);
	CHECK(split.gameMs == 60 * kSecond);
	CHECK(split.level == 12);
}

TEST_CASE("a run's time in game leaves out what was spent between games") {
	SessionModel model = After({
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
	SessionModel model = After({
		{ 0, Join(0) },
		{ 0, BossRun() },
		{ 30 * kSecond, SplitReached{ 2, false } },
		{ 50 * kSecond, SplitReached{ 0, true } },
	});

	const SessionModel::Run* run = model.ActiveRun();
	CHECK(run->splits[0].state == SessionModel::RunSplit::Reached);
	CHECK(run->splits[0].manual);
	CHECK(run->splits[1].state == SessionModel::RunSplit::Open);
	CHECK(run->splits[2].realMs == 30 * kSecond);
}

TEST_CASE("a split taken back is open again, and one passed over is neither") {
	SessionModel model = After({
		{ 0, Join(0) },
		{ 0, BossRun() },
		{ 30 * kSecond, SplitReached{ 0, false } },
		{ 40 * kSecond, SplitUndone{ 0 } },
		{ 50 * kSecond, SplitSkipped{ 1 } },
	});

	const SessionModel::Run* run = model.ActiveRun();
	CHECK(run->splits[0].state == SessionModel::RunSplit::Open);
	CHECK(run->splits[0].name == "Andariel");
	CHECK(run->splits[0].realMs == 0);
	CHECK(run->splits[1].state == SessionModel::RunSplit::Skipped);
	CHECK(run->splits[1].name == "Duriel");
}

TEST_CASE("a run that ends keeps how long it ran, and is no longer in hand") {
	SessionModel model = After({
		{ 0, Join(0) },
		{ 10 * kSecond, BossRun() },
		{ 40 * kSecond, Left() },
		{ 100 * kSecond, Join(0) },
		{ 130 * kSecond, SplitReached{ 2, false } },
		{ 130 * kSecond, RunEnded{ true } },
	});

	CHECK(model.ActiveRun() == NULL);
	REQUIRE(model.Runs().size() == 1);
	const SessionModel::Run& run = model.Runs()[0];
	CHECK(run.ended);
	CHECK(run.finished);
	CHECK(run.realMs == 120 * kSecond);
	CHECK(run.gameMs == 60 * kSecond);
	CHECK(model.RunRealMs(500 * kSecond) == 0);
}

TEST_CASE("a run given up is over and not finished") {
	SessionModel model = After({
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
	SessionModel model = After({
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
	CHECK(model.ActiveRun()->splits[0].state == SessionModel::RunSplit::Open);
}

TEST_CASE("a split of no run, or past the end of one, is nothing") {
	SessionModel model = After({
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
	for (const SessionModel::RunSplit& split : model.ActiveRun()->splits)
		CHECK(split.state == SessionModel::RunSplit::Open);
}

TEST_CASE("quest state and items held change nothing the session counts") {
	SessionModel model = After({
		{ 0, Join(0) },
		{ 5 * kSecond, Quest{ 6, 0x201e } },
		{ 6 * kSecond, ItemHeld{ "box" } },
	});

	CHECK(model.Games().size() == 1);
	CHECK(model.Runs().empty());
	CHECK(model.Gained() == 0);
}