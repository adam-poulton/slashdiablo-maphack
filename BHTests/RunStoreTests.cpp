#include "doctest.h"
#include <string>
#include <vector>
#include "Modules/Session/RunStore.h"

/*
 * The best a route has been run, and a run set beside it.
 *
 * Times are in seconds here and turned to milliseconds where a run is made, so
 * that a test reads as the splits of a run do.
 */

namespace {

typedef SessionModel::Run Run;
typedef SessionModel::RunSplit RunSplit;

const long long kSecond = 1000;
const char* const kKey = "bosses-normal/1/sc";

// A run whose splits were reached at these times, on both clocks alike. A time
// of -1 is a split still open, and -2 one passed over.
Run RunOf(const std::vector<long long>& seconds, bool finished) {
	Run run = {};
	run.route = "bosses-normal";
	run.name = "Act bosses (Normal)";
	long long last = 0;
	for (size_t i = 0; i < seconds.size(); i++) {
		RunSplit split = { "Split " + std::to_string(i + 1), RunSplit::Open, false, 0, 0, 0 };
		if (seconds[i] >= 0) {
			split.state = RunSplit::Reached;
			split.realMs = split.gameMs = (unsigned long long)(seconds[i] * kSecond);
			last = seconds[i];
		} else if (seconds[i] == -2) {
			split.state = RunSplit::Skipped;
		}
		run.splits.push_back(split);
	}
	run.ended = true;
	run.finished = finished;
	run.realMs = run.gameMs = (unsigned long long)(last * kSecond);
	return run;
}

std::vector<long long> Seconds(const std::vector<long long>& ms) {
	std::vector<long long> seconds;
	for (long long each : ms)
		seconds.push_back(each < 0 ? each : each / kSecond);
	return seconds;
}

}

TEST_CASE("bests are kept by route, class and whether the character is hardcore") {
	CHECK(RunStore::KeyFor("bosses-normal", 1, false) == "bosses-normal/1/sc");
	CHECK(RunStore::KeyFor("bosses-normal", 1, true) == "bosses-normal/1/hc");
	CHECK(RunStore::KeyFor("act1", 6, false) == "act1/6/sc");
}

TEST_CASE("nothing is known of a route never run") {
	RunStore store;
	CHECK(store.Find(kKey) == NULL);
}

TEST_CASE("the first finished run is the best, and every stretch of it the best of its kind") {
	RunStore store;
	store.Record(kKey, RunOf({ 100, 250, 400 }, true));

	const RunStore::Best* best = store.Find(kKey);
	REQUIRE(best != NULL);
	CHECK(best->attempts == 1);
	CHECK(best->finished == 1);
	const RunStore::Times& times = best->by[RunStore::RealTime];
	CHECK(times.total == 400 * kSecond);
	CHECK(Seconds(times.splits) == std::vector<long long>{ 100, 250, 400 });
	CHECK(Seconds(times.segments) == std::vector<long long>{ 100, 150, 150 });
}

TEST_CASE("a slower run leaves the best run alone and can still set a best stretch") {
	RunStore store;
	store.Record(kKey, RunOf({ 100, 250, 400 }, true));
	store.Record(kKey, RunOf({ 120, 240, 450 }, true));

	const RunStore::Best* best = store.Find(kKey);
	CHECK(best->attempts == 2);
	CHECK(best->finished == 2);
	const RunStore::Times& times = best->by[RunStore::RealTime];
	CHECK(times.total == 400 * kSecond);
	CHECK(Seconds(times.splits) == std::vector<long long>{ 100, 250, 400 });
	// The middle stretch was done in 120 where the best run took 150.
	CHECK(Seconds(times.segments) == std::vector<long long>{ 100, 120, 150 });
}

TEST_CASE("a faster run becomes the best") {
	RunStore store;
	store.Record(kKey, RunOf({ 100, 250, 400 }, true));
	store.Record(kKey, RunOf({ 90, 200, 380 }, true));

	const RunStore::Times& times = store.Find(kKey)->by[RunStore::RealTime];
	CHECK(times.total == 380 * kSecond);
	CHECK(Seconds(times.splits) == std::vector<long long>{ 90, 200, 380 });
	CHECK(Seconds(times.segments) == std::vector<long long>{ 90, 110, 150 });
}

TEST_CASE("a run given up is an attempt, and sets best stretches but no best run") {
	RunStore store;
	store.Record(kKey, RunOf({ 80, 300, -1 }, false));

	const RunStore::Best* best = store.Find(kKey);
	CHECK(best->attempts == 1);
	CHECK(best->finished == 0);
	const RunStore::Times& times = best->by[RunStore::RealTime];
	CHECK(times.total == -1);
	CHECK(Seconds(times.splits) == std::vector<long long>{ -1, -1, -1 });
	CHECK(Seconds(times.segments) == std::vector<long long>{ 80, 220, -1 });
}

TEST_CASE("a stretch is not timed across a split that was passed over") {
	RunStore store;
	store.Record(kKey, RunOf({ 100, -2, 400 }, true));

	const RunStore::Times& times = store.Find(kKey)->by[RunStore::RealTime];
	CHECK(Seconds(times.splits) == std::vector<long long>{ 100, -1, 400 });
	CHECK(Seconds(times.segments) == std::vector<long long>{ 100, -1, -1 });
}

TEST_CASE("a stretch is not timed between splits reached out of order") {
	const Run run = RunOf({ 300, 100, 400 }, true);
	CHECK(RunCompare::Segment(run, 0, RunStore::RealTime) == 300 * kSecond);
	CHECK(RunCompare::Segment(run, 1, RunStore::RealTime) == -1);
	CHECK(RunCompare::Segment(run, 2, RunStore::RealTime) == 300 * kSecond);
	CHECK(RunCompare::Segment(run, 3, RunStore::RealTime) == -1);
}

TEST_CASE("the two clocks keep bests of their own") {
	Run run = RunOf({ 100, 250, 400 }, true);
	for (RunSplit& split : run.splits)
		split.gameMs = split.realMs / 2;
	run.gameMs = run.realMs / 2;

	RunStore store;
	store.Record(kKey, run);
	CHECK(store.Find(kKey)->by[RunStore::RealTime].total == 400 * kSecond);
	CHECK(store.Find(kKey)->by[RunStore::GameTime].total == 200 * kSecond);
	CHECK(Seconds(store.Find(kKey)->by[RunStore::GameTime].segments) ==
		std::vector<long long>{ 50, 75, 75 });
}

TEST_CASE("the best by one clock need not be the best by the other") {
	Run quickInGame = RunOf({ 100, 250, 500 }, true);
	quickInGame.gameMs = 300 * kSecond;
	Run quickOverall = RunOf({ 100, 250, 400 }, true);
	quickOverall.gameMs = 390 * kSecond;

	RunStore store;
	store.Record(kKey, quickInGame);
	store.Record(kKey, quickOverall);
	CHECK(store.Find(kKey)->by[RunStore::RealTime].total == 400 * kSecond);
	CHECK(store.Find(kKey)->by[RunStore::GameTime].total == 300 * kSecond);
}

TEST_CASE("times kept for a route that has since changed length are not times to beat") {
	RunStore store;
	store.Record(kKey, RunOf({ 100, 250, 400 }, true));
	store.Record(kKey, RunOf({ 500, 900 }, true));

	const RunStore::Best* best = store.Find(kKey);
	CHECK(best->attempts == 2);
	CHECK(best->by[RunStore::RealTime].total == 900 * kSecond);
	CHECK(Seconds(best->by[RunStore::RealTime].splits) == std::vector<long long>{ 500, 900 });
}

TEST_CASE("bests are written to a file and read back as they were") {
	RunStore store;
	store.Record(kKey, RunOf({ 100, 250, 400 }, true));
	store.Record(kKey, RunOf({ 80, -1, -1 }, false));
	store.Record("act1/6/hc", RunOf({ 70 }, true));
	const std::string written = store.Write();

	RunStore read;
	REQUIRE(read.Read(written));
	CHECK(read.Write() == written);
	CHECK(read.Find(kKey)->attempts == 2);
	CHECK(read.Find(kKey)->by[RunStore::RealTime].total == 400 * kSecond);
	CHECK(Seconds(read.Find(kKey)->by[RunStore::RealTime].segments) ==
		std::vector<long long>{ 80, 150, 150 });
	CHECK(read.Find("act1/6/hc")->finished == 1);
}

TEST_CASE("a file that is not bests is refused and leaves nothing behind") {
	RunStore store;
	store.Record(kKey, RunOf({ 100 }, true));
	CHECK_FALSE(store.Read("not json"));
	CHECK(store.Find(kKey) == NULL);
	CHECK_FALSE(store.Read("{\"bests\": 4}"));
	CHECK_FALSE(store.Read("[]"));
}

TEST_CASE("a run with nothing to compare it with is only its own times") {
	Run run = RunOf({ 100, -1, -1 }, false);
	run.ended = false;
	const RunCompare::Lines lines = RunCompare::Compare(run, NULL, RunStore::RealTime, 130 * kSecond);

	REQUIRE(lines.splits.size() == 3);
	CHECK(lines.clock == 130 * kSecond);
	CHECK(lines.current == 1);
	CHECK(lines.bestTotal == -1);
	CHECK(lines.sumOfBest == -1);
	CHECK(lines.bestPossible == -1);
	CHECK(lines.splits[0].time == 100 * kSecond);
	CHECK(lines.splits[0].best == -1);
	CHECK_FALSE(lines.splits[0].compared);
	CHECK(lines.splits[1].time == -1);
}

TEST_CASE("a run is ahead or behind the best run split by split") {
	RunStore store;
	store.Record(kKey, RunOf({ 100, 250, 400 }, true));

	Run run = RunOf({ 90, 270, -1 }, false);
	run.ended = false;
	const RunCompare::Lines lines = RunCompare::Compare(run, store.Find(kKey), RunStore::RealTime,
		300 * kSecond);

	CHECK(lines.bestTotal == 400 * kSecond);
	CHECK(lines.current == 2);
	CHECK(lines.splits[0].compared);
	CHECK(lines.splits[0].delta == -10 * kSecond);
	CHECK(lines.splits[1].delta == 20 * kSecond);
	CHECK(lines.splits[1].best == 250 * kSecond);
	CHECK_FALSE(lines.splits[2].compared);
	CHECK(lines.splits[2].best == 400 * kSecond);
}

TEST_CASE("a stretch done faster than it ever was is marked, though the run is behind") {
	RunStore store;
	store.Record(kKey, RunOf({ 100, 250, 400 }, true));

	Run run = RunOf({ 130, 260, -1 }, false);
	run.ended = false;
	const RunCompare::Lines lines = RunCompare::Compare(run, store.Find(kKey), RunStore::RealTime,
		270 * kSecond);

	CHECK_FALSE(lines.splits[0].gold);
	// 130 where the best was 150, and still ten seconds behind the best run.
	CHECK(lines.splits[1].gold);
	CHECK(lines.splits[1].delta == 10 * kSecond);
	CHECK_FALSE(lines.splits[2].gold);
}

TEST_CASE("the sum of best is every stretch at its fastest") {
	RunStore store;
	store.Record(kKey, RunOf({ 100, 250, 400 }, true));
	store.Record(kKey, RunOf({ 120, 240, 450 }, true));

	Run run = RunOf({ -1, -1, -1 }, false);
	run.ended = false;
	const RunCompare::Lines lines = RunCompare::Compare(run, store.Find(kKey), RunStore::RealTime, 0);
	CHECK(lines.sumOfBest == (100 + 120 + 150) * kSecond);
	CHECK(lines.bestPossible == (100 + 120 + 150) * kSecond);
}

TEST_CASE("the sum of best is not known while a stretch has never been timed") {
	RunStore store;
	store.Record(kKey, RunOf({ 100, 250, -1 }, false));

	Run run = RunOf({ -1, -1, -1 }, false);
	run.ended = false;
	const RunCompare::Lines lines = RunCompare::Compare(run, store.Find(kKey), RunStore::RealTime, 0);
	CHECK(lines.sumOfBest == -1);
	CHECK(lines.bestPossible == -1);
}

TEST_CASE("the best possible time is what is done and the rest at its fastest") {
	RunStore store;
	store.Record(kKey, RunOf({ 100, 250, 400 }, true));

	Run run = RunOf({ 130, -1, -1 }, false);
	run.ended = false;
	const RunCompare::Lines lines = RunCompare::Compare(run, store.Find(kKey), RunStore::RealTime,
		140 * kSecond);
	CHECK(lines.bestPossible == (130 + 150 + 150) * kSecond);
}

TEST_CASE("there is no best possible time for a run that is over") {
	RunStore store;
	store.Record(kKey, RunOf({ 100, 250, 400 }, true));

	const Run run = RunOf({ 130, 300, 420 }, true);
	const RunCompare::Lines lines = RunCompare::Compare(run, store.Find(kKey), RunStore::RealTime,
		420 * kSecond);
	CHECK(lines.bestPossible == -1);
	CHECK(lines.current == -1);
	CHECK(lines.splits[2].delta == 20 * kSecond);
}

TEST_CASE("there is no best possible time once a split is reached ahead of an open one") {
	RunStore store;
	store.Record(kKey, RunOf({ 100, 250, 400 }, true));

	Run run = RunOf({ -1, 200, -1 }, false);
	run.ended = false;
	const RunCompare::Lines lines = RunCompare::Compare(run, store.Find(kKey), RunStore::RealTime,
		210 * kSecond);
	CHECK(lines.current == 0);
	CHECK(lines.bestPossible == -1);
}

TEST_CASE("a split passed over has no time and is not compared") {
	RunStore store;
	store.Record(kKey, RunOf({ 100, 250, 400 }, true));

	Run run = RunOf({ -2, 240, -1 }, false);
	run.ended = false;
	const RunCompare::Lines lines = RunCompare::Compare(run, store.Find(kKey), RunStore::RealTime,
		250 * kSecond);
	CHECK(lines.splits[0].state == RunSplit::Skipped);
	CHECK(lines.splits[0].time == -1);
	CHECK_FALSE(lines.splits[0].compared);
	CHECK(lines.splits[1].delta == -10 * kSecond);
	// The stretch up to it began at a split that was never reached.
	CHECK_FALSE(lines.splits[1].gold);
	CHECK(lines.current == 2);
	CHECK(lines.bestPossible == (240 + 150) * kSecond);
}

TEST_CASE("bests kept for a route of another length are not compared with") {
	RunStore store;
	store.Record(kKey, RunOf({ 100, 250 }, true));

	Run run = RunOf({ 90, -1, -1 }, false);
	run.ended = false;
	const RunCompare::Lines lines = RunCompare::Compare(run, store.Find(kKey), RunStore::RealTime,
		95 * kSecond);
	CHECK(lines.bestTotal == -1);
	CHECK_FALSE(lines.splits[0].compared);
}
