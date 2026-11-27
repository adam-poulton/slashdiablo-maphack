#include "doctest.h"
#include <string>
#include <vector>
#include "Modules/Session/SessionRows.h"

/*
 * What the session window's tabs list, as text.
 */

using namespace SessionEvents;
using namespace SessionRows;

namespace {

const unsigned long long kSecond = 1000;
const unsigned long long kMinute = 60 * kSecond;

SessionModel After(const std::vector<Event>& events) {
	SessionModel model;
	for (const Event& event : events)
		model.Apply(event);
	return model;
}

Started Someone() {
	Started fact = {};
	fact.character = "Sorc_One";
	fact.classId = 1;
	return fact;
}

// The figure beside a name, or "<none>" where no row carries the name. Looked
// for under a heading where one is given, since the session and the game both
// have a "Deaths".
std::string Figure(const std::vector<Row>& rows, const std::string& name,
		const std::string& under = "") {
	bool inside = under.empty();
	for (const Row& row : rows) {
		if (row.heading) {
			if (!under.empty())
				inside = row.cells[0] == under;
			continue;
		}
		if (inside && row.cells[0] == name)
			return row.cells[1];
	}
	return "<none>";
}

std::vector<std::string> Headings(const std::vector<Row>& rows) {
	std::vector<std::string> headings;
	for (const Row& row : rows) {
		if (row.heading)
			headings.push_back(row.cells[0]);
	}
	return headings;
}

const Level kNoLevel = { 0, 0 };

}

TEST_CASE("a run is named for its game without the number it ends in") {
	CHECK(RunNameOf("baal-12") == "baal");
	CHECK(RunNameOf("baal12") == "baal");
	CHECK(RunNameOf("baal run #3") == "baal run");
	CHECK(RunNameOf("chaos") == "chaos");
	CHECK(RunNameOf("2fast4u") == "2fast4u");
	CHECK(RunNameOf("") == "");
}

TEST_CASE("a game named with only a number is a run of its own") {
	CHECK(RunNameOf("123") == "123");
}

TEST_CASE("a session nothing has started has no overview") {
	CHECK(Overview(SessionModel(), 0, kNoLevel, "").empty());
}

TEST_CASE("the overview names the character, its level and its class") {
	SessionModel model = After({
		{ 0, Someone() },
		{ 0, Joined{ "baal-1", 2, 87, 1000 } },
	});
	CHECK(Figure(Overview(model, kMinute, kNoLevel, ""), "Character") ==
		"Sorc_One, level 87 Sorceress");
}

TEST_CASE("a hardcore character is said to be one") {
	Started fact = Someone();
	fact.hardcore = true;
	SessionModel model = After({ { 0, fact }, { 0, Joined{ "a", 0, 5, 0 } } });
	CHECK(Figure(Overview(model, kMinute, kNoLevel, ""), "Character") ==
		"Sorc_One, level 5 Sorceress (Hardcore)");
}

TEST_CASE("the overview gives the session's clocks, games and deaths") {
	SessionModel model = After({
		{ 0, Someone() },
		{ 0, Joined{ "baal-1", 2, 87, 1000 } },
		{ 30 * kSecond, Died() },
		{ 60 * kSecond, Left() },
		{ 100 * kSecond, Joined{ "baal-2", 2, 87, 1000 } },
	});
	const std::vector<Row> rows = Overview(model, 130 * kSecond, kNoLevel, "22:40");

	CHECK(Figure(rows, "Started") == "22:40");
	CHECK(Figure(rows, "Session time") == "2:10");
	CHECK(Figure(rows, "Time played") == "1:30");
	CHECK(Figure(rows, "Games") == "2");
	CHECK(Figure(rows, "Deaths", "Session") == "1");
}

TEST_CASE("when the session started is left out where it is not known") {
	SessionModel model = After({ { 0, Someone() } });
	CHECK(Figure(Overview(model, 0, kNoLevel, ""), "Started") == "<none>");
}

TEST_CASE("the overview gives experience gained and how fast it is coming") {
	SessionModel model = After({
		{ 0, Someone() },
		{ 0, Joined{ "baal-1", 2, 87, 0 } },
		{ 50 * kSecond, Experience{ 1234567 } },
	});
	const std::vector<Row> rows = Overview(model, 100 * kSecond, kNoLevel, "");

	CHECK(Figure(rows, "Gained") == "1,234,567");
	CHECK(Figure(rows, "Per second played") == "+12.3K/s");
	CHECK(Figure(rows, "Last minute") == "+20.6K/s");
	CHECK(Figure(rows, "Levels gained") == "0");
	// Nothing lost, so nothing said of losing it.
	CHECK(Figure(rows, "Lost to deaths") == "<none>");
	CHECK(Figure(rows, "Net") == "<none>");
}

TEST_CASE("experience lost to deaths is shown beside what is left of the gain") {
	SessionModel model = After({
		{ 0, Someone() },
		{ 0, Joined{ "baal-1", 2, 87, 100000 } },
		{ 10 * kSecond, Experience{ 150000 } },
		{ 20 * kSecond, Experience{ 120000 } },
	});
	const std::vector<Row> rows = Overview(model, 100 * kSecond, kNoLevel, "");

	CHECK(Figure(rows, "Gained") == "50,000");
	CHECK(Figure(rows, "Lost to deaths") == "30,000");
	CHECK(Figure(rows, "Net") == "+20,000");
}

TEST_CASE("the next level is how far through this one, and how long the rest takes") {
	SessionModel model = After({
		{ 0, Someone() },
		{ 0, Joined{ "baal-1", 2, 87, 1000 } },
		{ 100 * kSecond, Experience{ 1500 } },
	});
	// Level runs 1000 to 3000: a quarter done, 1500 to go at 5 a second.
	CHECK(Figure(Overview(model, 100 * kSecond, { 1000, 3000 }, ""), "Next level") ==
		"25.0%, 5:00 to go");
}

TEST_CASE("the next level has no time to go while nothing is being gained") {
	SessionModel model = After({
		{ 0, Someone() },
		{ 0, Joined{ "baal-1", 2, 87, 1500 } },
	});
	CHECK(Figure(Overview(model, 100 * kSecond, { 1000, 3000 }, ""), "Next level") == "25.0%");
}

TEST_CASE("the next level is left out where there is none to reach") {
	SessionModel model = After({
		{ 0, Someone() },
		{ 0, Joined{ "baal-1", 2, 99, 5000 } },
	});
	CHECK(Figure(Overview(model, kMinute, kNoLevel, ""), "Next level") == "<none>");
	// Or where the level given is not the one the experience is in.
	CHECK(Figure(Overview(model, kMinute, { 6000, 9000 }, ""), "Next level") == "<none>");
}

TEST_CASE("the game being played has a heading of its own, and only while it is") {
	SessionModel model = After({
		{ 0, Someone() },
		{ 0, Joined{ "baal-1", 2, 87, 0 } },
		{ 0, Players{ 8 } },
		{ 20 * kSecond, Experience{ 5000 } },
	});

	std::vector<Row> rows = Overview(model, 40 * kSecond, kNoLevel, "");
	CHECK(Headings(rows) == std::vector<std::string>{ "Session", "Experience", "This game" });
	CHECK(Figure(rows, "Game") == "baal-1 (Hell)");
	CHECK(Figure(rows, "Time") == "0:40");
	CHECK(Figure(rows, "Experience", "This game") == "+5,000");
	CHECK(Figure(rows, "Per second", "This game") == "+125/s");
	CHECK(Figure(rows, "Players") == "8");
	CHECK(Figure(rows, "Deaths", "This game") == "<none>");

	model.Apply({ 50 * kSecond, Left() });
	rows = Overview(model, 60 * kSecond, kNoLevel, "");
	CHECK(Headings(rows) == std::vector<std::string>{ "Session", "Experience" });
}

TEST_CASE("a single player game is called that, having no name") {
	SessionModel model = After({ { 0, Someone() }, { 0, Joined{ "", 0, 1, 0 } } });
	CHECK(Figure(Overview(model, kMinute, kNoLevel, ""), "Game") == "Single player (Normal)");
}

TEST_CASE("a session with no games lists none") {
	CHECK(Games(After({ { 0, Someone() } }), kMinute).empty());
}

TEST_CASE("games are listed under their run, latest first") {
	SessionModel model = After({
		{ 0, Joined{ "baal-1", 2, 87, 0 } },
		{ 30 * kSecond, Experience{ 6000 } },
		{ 60 * kSecond, Left() },
		{ 70 * kSecond, Joined{ "baal-2", 2, 87, 6000 } },
	});
	const std::vector<Row> rows = Games(model, 100 * kSecond);

	REQUIRE(rows.size() == 3);
	CHECK(rows[0].heading);
	CHECK(rows[0].cells[0] == "baal (Hell)");

	CHECK(rows[1].cells[GameNumber] == "2");
	CHECK(rows[1].cells[GameName] == "baal-2");
	CHECK(rows[1].cells[GameLength] == "0:30");

	CHECK(rows[2].cells[GameNumber] == "1");
	CHECK(rows[2].cells[GameName] == "baal-1");
	CHECK(rows[2].cells[GameLength] == "1:00");
	CHECK(rows[2].cells[GameExperience] == "+6,000");
	CHECK(rows[2].cells[GameRate] == "+100/s");
	CHECK(rows[2].cells[GameLevel] == "87");
}

TEST_CASE("a game that lost more than it gained says so") {
	SessionModel model = After({
		{ 0, Joined{ "baal-1", 2, 87, 9000 } },
		{ 10 * kSecond, Experience{ 6000 } },
		{ 30 * kSecond, Left() },
	});
	const std::vector<Row> rows = Games(model, kMinute);
	CHECK(rows[1].cells[GameExperience] == "-3,000");
	CHECK(rows[1].cells[GameRate] == "-100/s");
}

TEST_CASE("a game a level was gained in shows the levels it ran between") {
	SessionModel model = After({
		{ 0, Joined{ "baal-1", 2, 87, 0 } },
		{ 10 * kSecond, LevelReached{ 89 } },
	});
	CHECK(Games(model, kMinute)[1].cells[GameLevel] == "87 - 89");
}

TEST_CASE("a run played in two difficulties is two runs") {
	SessionModel model = After({
		{ 0, Joined{ "baal-1", 1, 87, 0 } },
		{ 10 * kSecond, Left() },
		{ 20 * kSecond, Joined{ "baal-2", 2, 87, 0 } },
		{ 30 * kSecond, Left() },
	});
	CHECK(Headings(Games(model, kMinute)) ==
		std::vector<std::string>{ "baal (Hell)", "baal (Nightmare)" });
}

TEST_CASE("runs are in the order they were last played") {
	SessionModel model = After({
		{ 0, Joined{ "chaos-1", 2, 87, 0 } },
		{ 10 * kSecond, Left() },
		{ 20 * kSecond, Joined{ "baal-1", 2, 87, 0 } },
		{ 30 * kSecond, Left() },
		{ 40 * kSecond, Joined{ "chaos-2", 2, 87, 0 } },
		{ 50 * kSecond, Left() },
	});
	CHECK(Headings(Games(model, kMinute)) ==
		std::vector<std::string>{ "chaos (Hell)", "baal (Hell)" });
}

TEST_CASE("a run with two finished games opens with what they average") {
	SessionModel model = After({
		{ 0, Joined{ "baal-1", 2, 87, 0 } },
		{ 10 * kSecond, Experience{ 4000 } },
		{ 40 * kSecond, Left() },
		{ 50 * kSecond, Joined{ "baal-2", 2, 87, 4000 } },
		{ 60 * kSecond, Experience{ 6000 } },
		{ 70 * kSecond, Left() },
	});
	const std::vector<Row> rows = Games(model, 2 * kMinute);

	REQUIRE(rows.size() == 4);
	CHECK(rows[1].summary);
	CHECK(rows[1].cells[GameNumber] == "");
	CHECK(rows[1].cells[GameName] == "Average of 2");
	// 40 s and 20 s, 4000 and 2000: 30 s and 3000 a game, 6000 over a minute.
	CHECK(rows[1].cells[GameLength] == "0:30");
	CHECK(rows[1].cells[GameExperience] == "+3,000");
	CHECK(rows[1].cells[GameRate] == "+100/s");
	CHECK_FALSE(rows[2].summary);
}

TEST_CASE("the game being played is left out of its run's average") {
	SessionModel model = After({
		{ 0, Joined{ "baal-1", 2, 87, 0 } },
		{ 40 * kSecond, Left() },
		{ 50 * kSecond, Joined{ "baal-2", 2, 87, 0 } },
	});
	const std::vector<Row> rows = Games(model, kMinute);

	// One finished game is its own average, so there is no row for it.
	REQUIRE(rows.size() == 3);
	CHECK_FALSE(rows[1].summary);
	CHECK(rows[1].cells[GameName] == "baal-2");
}

TEST_CASE("the overview counts kills, and what of them were more than ordinary") {
	SessionModel model = After({
		{ 0, Someone() },
		{ 0, Joined{ "baal-1", 2, 87, 0 } },
		{ 1 * kSecond, Killed{ 131, 571, "Minion of Destruction", TierNormal, 1230 } },
	});
	CHECK(Figure(Overview(model, kMinute, kNoLevel, ""), "Kills") == "1,230");

	model.Apply({ 2 * kSecond, Killed{ 131, 571, "Minion of Destruction", TierChampion, 3 } });
	CHECK(Figure(Overview(model, kMinute, kNoLevel, ""), "Kills") == "1,233 (3 champion)");

	model.Apply({ 3 * kSecond, Notable{ 132, 544, "Baal", "Baal", TierBoss, -1 } });
	CHECK(Figure(Overview(model, kMinute, kNoLevel, ""), "Kills") == "1,234 (3 champion, 1 unique)");
}

TEST_CASE("the overview counts drops") {
	SessionModel model = After({
		{ 0, Someone() },
		{ 0, Joined{ "baal-1", 2, 87, 0 } },
	});
	CHECK(Figure(Overview(model, kMinute, kNoLevel, ""), "Drops") == "0");

	model.Apply({ 5 * kSecond, Dropped{ 1001, "Ist Rune", "r24", 2, 8, 1, 131, "" } });
	CHECK(Figure(Overview(model, kMinute, kNoLevel, ""), "Drops") == "1");
}

namespace {

SessionModel Fought() {
	return After({
		{ 0, Someone() },
		{ 0, Joined{ "baal-1", 2, 87, 0 } },
		{ 0, AreaEntered{ 109, true, "Harrogath" } },
		{ 10 * kSecond, AreaEntered{ 130, false, "The Worldstone Keep Level 3" } },
		{ 20 * kSecond, Killed{ 130, 456, "Death Lord", TierNormal, 5 } },
		{ 30 * kSecond, AreaEntered{ 131, false, "Throne of Destruction" } },
		{ 40 * kSecond, Killed{ 131, 571, "Minion of Destruction", TierNormal, 40 } },
		{ 41 * kSecond, Killed{ 131, 571, "Minion of Destruction", TierChampion, 2 } },
		{ 45 * kSecond, Experience{ 6000 } },
		{ 50 * kSecond, Notable{ 131, 571, "Lister the Tormentor", "Minion of Destruction", TierSuperUnique, 65 } },
		{ 60 * kSecond, AreaEntered{ 132, false, "The Worldstone Chamber" } },
		{ 95 * kSecond, Notable{ 132, 544, "Baal", "Baal", TierBoss, -1 } },
	});
}

// The rows under a heading, up to the next one.
std::vector<Row> Under(const std::vector<Row>& rows, const std::string& heading) {
	std::vector<Row> under;
	bool inside = false;
	for (const Row& row : rows) {
		if (row.heading)
			inside = row.cells[0] == heading;
		else if (inside)
			under.push_back(row);
	}
	return under;
}

}

TEST_CASE("a session nothing has died in lists no kills") {
	SessionModel model = After({ { 0, Someone() }, { 0, Joined{ "a", 0, 1, 0 } },
		{ 0, AreaEntered{ 1, true, "Rogue Encampment" } } });
	CHECK(Kills(model, kMinute).empty());
}

TEST_CASE("the kills are listed by area, by kind of monster, and one by one for the unique") {
	CHECK(Headings(Kills(Fought(), 100 * kSecond)) ==
		std::vector<std::string>{ "Areas", "Monsters", "Unique monsters" });
}

TEST_CASE("an area gives what died there, how long was spent and what was earned") {
	const std::vector<Row> areas = Under(Kills(Fought(), 100 * kSecond), "Areas");

	// Most kills first, and no town among them.
	REQUIRE(areas.size() == 3);
	CHECK(areas[0].cells[KillName] == "Throne of Destruction");
	CHECK(areas[0].cells[KillTotal] == "43");
	CHECK(areas[0].cells[KillChampions] == "2");
	CHECK(areas[0].cells[KillUniques] == "1");
	CHECK(areas[0].cells[KillTime] == "0:30");
	CHECK(areas[0].cells[KillDetail] == "+6.0K xp");

	CHECK(areas[1].cells[KillName] == "The Worldstone Keep Level 3");
	CHECK(areas[1].cells[KillTotal] == "5");
	// Nothing where there is nothing to count.
	CHECK(areas[1].cells[KillChampions] == "");
	CHECK(areas[1].cells[KillUniques] == "");
	CHECK(areas[1].cells[KillDetail] == "");

	// The area being stood in, measured to now.
	CHECK(areas[2].cells[KillName] == "The Worldstone Chamber");
	CHECK(areas[2].cells[KillTotal] == "1");
	CHECK(areas[2].cells[KillTime] == "0:40");
}

TEST_CASE("an area the log gave no name is called by its number") {
	SessionModel model = After({
		{ 0, Joined{ "a", 0, 1, 0 } },
		{ 0, AreaEntered{ 131, false } },
		{ 5 * kSecond, Killed{ 131, 571, "Minion of Destruction", TierNormal, 1 } },
	});
	CHECK(Under(Kills(model, kMinute), "Areas")[0].cells[KillName] == "Area 131");
}

TEST_CASE("the kinds of monster are listed most killed first") {
	const std::vector<Row> kinds = Under(Kills(Fought(), 100 * kSecond), "Monsters");

	REQUIRE(kinds.size() == 3);
	CHECK(kinds[0].cells[KillName] == "Minion of Destruction");
	CHECK(kinds[0].cells[KillTotal] == "43");
	CHECK(kinds[0].cells[KillChampions] == "2");
	CHECK(kinds[0].cells[KillUniques] == "1");
	CHECK(kinds[0].cells[KillTime] == "");
	CHECK(kinds[1].cells[KillName] == "Death Lord");
	CHECK(kinds[2].cells[KillName] == "Baal");
	CHECK(kinds[2].cells[KillUniques] == "1");
}

TEST_CASE("each unique monster is listed with when it died and where, latest first") {
	const std::vector<Row> unique = Under(Kills(Fought(), 100 * kSecond), "Unique monsters");

	REQUIRE(unique.size() == 2);
	CHECK(unique[0].cells[KillName] == "Baal");
	CHECK(unique[0].cells[KillTime] == "1:35");
	CHECK(unique[0].cells[KillDetail] == "The Worldstone Chamber");
	CHECK(unique[1].cells[KillName] == "Lister the Tormentor");
	CHECK(unique[1].cells[KillTime] == "0:50");
	CHECK(unique[1].cells[KillDetail] == "Throne of Destruction");
}

TEST_CASE("drops are listed latest first, the item in the colour it was announced in") {
	SessionModel model = After({
		{ 0, Joined{ "baal-1", 2, 87, 0 } },
		{ 65 * kSecond, Dropped{ 1001, "Ist Rune", "r24", 2, 8, 1, 131, "Throne of Destruction" } },
		{ 70 * kSecond, Left() },
		{ 80 * kSecond, Joined{ "baal-2", 2, 87, 0 } },
		{ 90 * kSecond, Dropped{ 1002, "Harlequin Crest Shako", "uap", 7, 4, 2, 132, "The Worldstone Chamber" } },
	});
	const std::vector<Row> rows = Drops(model);

	REQUIRE(rows.size() == 2);
	CHECK(rows[0].cells[DropTime] == "1:30");
	CHECK(rows[0].cells[DropItem] == "Harlequin Crest Shako");
	CHECK(rows[0].cells[DropArea] == "The Worldstone Chamber");
	CHECK(rows[0].cells[DropGame] == "2");
	CHECK(rows[0].cells[DropPing] == "2");
	REQUIRE(rows[0].colors.size() == DropColumns);
	CHECK(rows[0].colors[DropItem] == 4);
	CHECK(rows[0].colors[DropTime] == -1);

	CHECK(rows[1].cells[DropItem] == "Ist Rune");
	CHECK(rows[1].cells[DropGame] == "1");
	CHECK(rows[1].colors[DropItem] == 8);
}

TEST_CASE("a drop that named no area takes the name the area was entered by") {
	SessionModel model = After({
		{ 0, Joined{ "baal-1", 2, 87, 0 } },
		{ 0, AreaEntered{ 131, false, "Throne of Destruction" } },
		{ 5 * kSecond, Dropped{ 1001, "Ist Rune", "r24", 2, 8, 1, 131, "" } },
		{ 6 * kSecond, Dropped{ 1002, "Ring", "rin", 6, 9, 3, 77, "" } },
	});
	const std::vector<Row> rows = Drops(model);
	CHECK(rows[1].cells[DropArea] == "Throne of Destruction");
	CHECK(rows[0].cells[DropArea] == "Area 77");
}

TEST_CASE("no search keeps every row") {
	const std::vector<Row> rows = Kills(Fought(), 100 * kSecond);
	CHECK(Matching(rows, "").size() == rows.size());
}

TEST_CASE("a search keeps the rows that have the text, under their headings") {
	const std::vector<Row> found = Matching(Kills(Fought(), 100 * kSecond), "DEATH lord");

	REQUIRE(found.size() == 2);
	CHECK(found[0].heading);
	CHECK(found[0].cells[0] == "Monsters");
	CHECK(found[1].cells[KillName] == "Death Lord");
}

TEST_CASE("a search can find a row by any of its cells") {
	const std::vector<Row> found = Matching(Kills(Fought(), 100 * kSecond), "worldstone chamber");

	// The area itself, and Baal, who died there.
	CHECK(Headings(found) == std::vector<std::string>{ "Areas", "Unique monsters" });
	CHECK(found.size() == 4);
}

TEST_CASE("a search that finds a heading keeps everything under it") {
	const std::vector<Row> found = Matching(Kills(Fought(), 100 * kSecond), "unique monsters");

	REQUIRE(found.size() == 3);
	CHECK(found[0].cells[0] == "Unique monsters");
	CHECK(found[1].cells[KillName] == "Baal");
	CHECK(found[2].cells[KillName] == "Lister the Tormentor");
}

TEST_CASE("a search that finds nothing keeps nothing, headings included") {
	CHECK(Matching(Kills(Fought(), 100 * kSecond), "pindleskin").empty());
}

TEST_CASE("a search drops a run's average, which is of games no longer all listed") {
	SessionModel model = After({
		{ 0, Joined{ "baal-1", 2, 87, 0 } },
		{ 40 * kSecond, Left() },
		{ 50 * kSecond, Joined{ "baal-2", 2, 87, 0 } },
		{ 70 * kSecond, Left() },
	});
	const std::vector<Row> found = Matching(Games(model, 2 * kMinute), "baal-2");

	REQUIRE(found.size() == 2);
	CHECK(found[0].heading);
	CHECK_FALSE(found[1].summary);
	CHECK(found[1].cells[GameName] == "baal-2");
}