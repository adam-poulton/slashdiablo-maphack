#include "doctest.h"
#include <fstream>
#include <string>
#include <vector>
#include "Modules/Session/SessionFile.h"
#include "Modules/Session/SessionModel.h"
#include "Modules/Session/SessionRows.h"

/*
 * A whole session's log, read back.
 *
 * fixtures/session-baal-runs.jsonl is a sitting of eight games on a level 80
 * character: five Baal runs and two Chaos runs finished, a death in the first
 * Chaos run, level 81 reached on the way, and a sixth Baal run still being
 * played where the log stops. The harness puts the same log on screen.
 */

using namespace SessionEvents;

namespace {

const char* kLog = "BHTests/fixtures/session-baal-runs.jsonl";

const unsigned long long kLogEnd = 1551000;
const unsigned long long kStartedWith = 690427665ull;
const unsigned long long kEndedWith = 766977105ull;
const unsigned long long kLostToTheDeath = 2650000ull;

SessionModel Replayed() {
	std::vector<Event> events;
	REQUIRE(SessionFile::Read(kLog, events));
	SessionModel model;
	for (const Event& event : events)
		model.Apply(event);
	return model;
}

}

TEST_CASE("every line of a recorded session is an event, and is written back as it was") {
	std::ifstream file(kLog, std::ios::binary);
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
	CHECK(lines == 1387);
}

TEST_CASE("a recorded session comes to what was played") {
	SessionModel model = Replayed();

	CHECK(model.Header().character == "Sorc_One");
	REQUIRE(model.Games().size() == 8);
	CHECK(model.InGame());
	CHECK(model.Games().back().name == "baal-46");
	CHECK(model.Games().front().startExperience == kStartedWith);
	CHECK(model.Games().back().experience == kEndedWith);

	CHECK(model.Deaths() == 1);
	CHECK(model.Games()[3].deaths == 1);
	CHECK(model.Lost() == kLostToTheDeath);
	CHECK(model.Gained() == kEndedWith - kStartedWith + kLostToTheDeath);
	CHECK(model.LevelsGained() == 1);
	CHECK(model.Games().back().level == 81);
}

TEST_CASE("a recorded session's time is the games' and the gaps between them") {
	SessionModel model = Replayed();

	unsigned long long played = 0;
	for (const SessionModel::Game& game : model.Games())
		played += (game.open ? kLogEnd : game.leftAt) - game.joinedAt;
	CHECK(model.PlayedMs(kLogEnd) == played);
	// Seven games were left, with eleven seconds before the next was joined.
	CHECK(kLogEnd - played == 7 * 11000);
}

TEST_CASE("a recorded session's experience is credited where it was earned") {
	SessionModel model = Replayed();

	unsigned long long credited = 0;
	for (const auto& area : model.Areas())
		credited += area.second.gained;
	CHECK(credited == model.Gained());

	// The Throne of Destruction is where a Baal run earns most.
	const unsigned int throne = 131;
	for (const auto& area : model.Areas())
		CHECK(area.second.gained <= model.Areas().at(throne).gained);
	CHECK(model.Areas().at(109).town);
	CHECK(model.Areas().at(109).gained == 0);
}

TEST_CASE("a recorded session's games are listed by run") {
	const std::vector<SessionRows::Row> rows = SessionRows::Games(Replayed(), kLogEnd);

	std::vector<std::string> headings;
	unsigned int games = 0, averages = 0;
	for (const SessionRows::Row& row : rows) {
		if (row.heading)
			headings.push_back(row.cells[0]);
		else if (row.summary)
			averages++;
		else
			games++;
	}
	CHECK(headings == std::vector<std::string>{ "baal (Hell)", "chaos (Hell)" });
	CHECK(games == 8);
	CHECK(averages == 2);
}
