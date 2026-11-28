#include "doctest.h"
#include <fstream>
#include <string>
#include <vector>
#include "Modules/Race/RunModel.h"
#include "Modules/Race/RunStore.h"
#include "Modules/Session/SessionFile.h"
#include "Modules/Session/SessionModel.h"

using namespace PlayEvents;

/*
 * fixtures/session-race.jsonl is a new character raced through the five act
 * bosses of Normal in an hour and a quarter, a game to an act, and then a
 * second run on the same route: Andariel half a minute sooner, Duriel half a
 * minute later, and Mephisto still to come where the log stops.
 */

namespace {

const char* kRace = "BHTests/fixtures/session-race.jsonl";
const unsigned long long kRaceEnd = 6230000;
const unsigned long long kSecond = 1000;

RunModel Raced() {
	std::vector<Event> events;
	REQUIRE(SessionFile::Read(kRace, events));
	RunModel model;
	for (const Event& event : events)
		model.Apply(event);
	return model;
}

}

TEST_CASE("every line of a recorded race is an event, and is written back as it was") {
	std::ifstream file(kRace, std::ios::binary);
	REQUIRE(file.is_open());

	unsigned int lines = 0;
	std::string line;
	while (std::getline(file, line)) {
		if (!line.empty() && line.back() == '\r')
			line.pop_back();
		lines++;
		CAPTURE(lines);
		Event event;
		REQUIRE(FromLine(line, event));
		REQUIRE(ToLine(event) == line);
	}
	CHECK(lines == 204);
}

TEST_CASE("a recorded race holds the run that was finished and the one in hand") {
	const RunModel model = Raced();
	REQUIRE(model.Runs().size() == 2);

	const RunModel::Run& first = model.Runs()[0];
	CHECK(first.finished);
	CHECK(first.realMs == 4500 * kSecond);
	// Ten seconds between each of its five games.
	CHECK(first.gameMs == 4460 * kSecond);
	CHECK(first.splits[0].realMs == 600 * kSecond);
	CHECK(first.splits[0].level == 13);
	CHECK(first.splits[4].realMs == 4500 * kSecond);
	CHECK(first.splits[4].gameMs == 4460 * kSecond);

	const RunModel::Run* second = model.ActiveRun();
	REQUIRE(second != NULL);
	CHECK(second->splits[0].realMs == 570 * kSecond);
	CHECK(second->splits[1].realMs == 1530 * kSecond);
	CHECK(second->splits[1].gameMs == 1520 * kSecond);
	CHECK(second->splits[2].state == RunModel::Split::Open);
	CHECK(model.RunRealMs(kRaceEnd) == 1680 * kSecond);
	CHECK(model.RunGameMs(kRaceEnd) == 1670 * kSecond);
}

TEST_CASE("a recorded race's second run is set beside its first") {
	const RunModel model = Raced();
	RunStore store;
	store.Record("key", model.Runs()[0]);

	const RunCompare::Lines lines = RunCompare::Compare(*model.ActiveRun(), store.Find("key"),
		RunStore::RealTime, model.RunRealMs(kRaceEnd));
	CHECK(lines.splits[0].delta == -30 * (long long)kSecond);
	CHECK(lines.splits[0].gold);
	CHECK(lines.splits[1].delta == 30 * (long long)kSecond);
	CHECK_FALSE(lines.splits[1].gold);
	CHECK(lines.current == 2);
	CHECK(lines.bestTotal == 4500 * (long long)kSecond);
	CHECK(lines.sumOfBest == 4500 * (long long)kSecond);
	CHECK(lines.bestPossible == 4530 * (long long)kSecond);
}

TEST_CASE("a recorded race is five games to the session tracker, and no runs") {
	std::vector<Event> events;
	REQUIRE(SessionFile::Read(kRace, events));
	SessionModel model;
	for (const Event& event : events)
		model.Apply(event);
	CHECK(model.Games().size() >= 5);
	CHECK(model.PlayedMs(kRaceEnd) > 0);
}
