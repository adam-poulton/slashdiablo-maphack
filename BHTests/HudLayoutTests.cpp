#include "doctest.h"
#include <string>
#include <vector>
#include "Modules/Session/HudLayout.h"

/*
 * The session HUD without the game: what each of its views says.
 */

using namespace HudBlock;
using namespace HudLayout;
using namespace PlayEvents;

namespace {

const unsigned long long kSecond = 1000;

Figures SomeFigures() {
	Figures figures = {};
	figures.sessionMs = (3600 + 12 * 60 + 34) * kSecond;
	figures.playedMs = (58 * 60 + 10) * kSecond;
	figures.gameMs = (3 * 60 + 21) * kSecond;
	figures.games = 7;
	figures.gameRate = 1250000;
	figures.playedRate = 980400;
	figures.recentRate = 1400000;
	return figures;
}

std::string Text(const Row& row) {
	std::string text;
	for (const Cell& cell : row)
		text += (text.empty() ? "" : " ") + cell.text;
	return text;
}

}

TEST_CASE("the views are named, and a name finds its view whatever its case") {
	CHECK(std::string(NameOf(ViewLine)) == "Line");
	CHECK(std::string(NameOf(ViewCompact)) == "Compact");
	CHECK(ViewNamed("line") == ViewLine);
	CHECK(ViewNamed("COMPACT") == ViewCompact);
	CHECK(ViewNamed("Full") == ViewFull);
	CHECK(ViewNamed("everything") == ViewCount);
	CHECK(ViewNamed("") == ViewCount);
}

TEST_CASE("the session HUD has no view of a run, which is the race HUD's to show") {
	CHECK(ViewNamed("splits") == ViewCount);
}

TEST_CASE("stepping through the views goes round, either way") {
	CHECK(Next(ViewLine, 1) == ViewCompact);
	CHECK(Next(ViewCompact, 1) == ViewFull);
	CHECK(Next(ViewFull, 1) == ViewLine);
	CHECK(Next(ViewLine, -1) == ViewFull);
	CHECK(Next(ViewCompact, -4) == ViewLine);
}

TEST_CASE("the line view is the session clock, the game and its clock, and the rate") {
	const std::vector<Section> sections = Sections(ViewLine, SomeFigures());
	REQUIRE(sections.size() == 1);
	const Section& rows = sections[0];
	REQUIRE(rows.size() == 1);
	CHECK(Text(rows[0]) == "1:12:34 Game 7 3:21 XP +980.4K/s");
}

TEST_CASE("the compact view is three rows of names and figures") {
	const std::vector<Section> sections = Sections(ViewCompact, SomeFigures());
	REQUIRE(sections.size() == 1);
	const Section& rows = sections[0];
	REQUIRE(rows.size() == 3);
	CHECK(Text(rows[0]) == "Session 1:12:34 Played 58:10");
	CHECK(Text(rows[1]) == "Game 7 3:21 XP +1.25M/s");
	CHECK(Text(rows[2]) == "Session XP +980.4K/s Recent +1.40M/s");
}

TEST_CASE("a name is told from a figure") {
	for (const Row& row : Sections(ViewCompact, SomeFigures())[0]) {
		REQUIRE(row.size() == 4);
		CHECK(row[0].tone == ToneLabel);
		CHECK(row[1].tone == ToneValue);
		CHECK(row[2].tone == ToneLabel);
		CHECK(row[3].tone == ToneValue);
	}
}

TEST_CASE("the full view is the session, and what has died and dropped") {
	Figures figures = SomeFigures();
	figures.kills = 1713;
	figures.deaths = 1;
	figures.lastDrop = "Ist Rune";

	const std::vector<Section> sections = Sections(ViewFull, figures);
	REQUIRE(sections.size() == 3);
	CHECK(sections[0].size() == 3);
	CHECK(Text(sections[0][0]) == "Session 1:12:34 Played 58:10");
	REQUIRE(sections[1].size() == 1);
	CHECK(Text(sections[1][0]) == "Kills 1,713 Deaths 1");
	REQUIRE(sections[2].size() == 1);
	CHECK(Text(sections[2][0]) == "Drop Ist Rune");
	// A drop's name stands against the left, as a name does.
	CHECK(sections[2][0][1].left);
}

TEST_CASE("the full view has no line for a drop until there has been one") {
	CHECK(Sections(ViewFull, SomeFigures()).size() == 2);
}

TEST_CASE("the figures of a view are read off the session at the moment asked about") {
	SessionModel model;
	model.Apply({ 0, Joined{ "baal-1", 2, 80, 0 } });
	model.Apply({ 20 * kSecond, Left() });
	model.Apply({ 30 * kSecond, Joined{ "baal-2", 2, 80, 0 } });
	model.Apply({ 35 * kSecond, Experience{ 6000 } });

	const Figures figures = FiguresOf(model, 40 * kSecond);
	CHECK(figures.sessionMs == 40 * kSecond);
	CHECK(figures.playedMs == 30 * kSecond);
	CHECK(figures.gameMs == 10 * kSecond);
	CHECK(figures.games == 2);
	CHECK(figures.gameRate == doctest::Approx(600.0));
	CHECK(figures.playedRate == doctest::Approx(200.0));
	CHECK(figures.recentRate == doctest::Approx(200.0));
}

TEST_CASE("the figures say what has died and what last dropped") {
	SessionModel model;
	model.Apply({ 0, Joined{ "a", 2, 80, 0 } });
	model.Apply({ 1 * kSecond, Killed{ 131, 571, "Minion of Destruction", TierNormal, 7 } });
	model.Apply({ 2 * kSecond, Died() });
	model.Apply({ 3 * kSecond, Dropped{ 1, "Ring", "rin", 6, 9, 3, 131, "" } });
	model.Apply({ 4 * kSecond, Dropped{ 2, "Ist Rune", "r24", 2, 8, 1, 131, "" } });

	const Figures figures = FiguresOf(model, 10 * kSecond);
	CHECK(figures.kills == 7);
	CHECK(figures.deaths == 1);
	CHECK(figures.lastDrop == "Ist Rune");
}
