#include "doctest.h"
#include <string>
#include <vector>
#include "Modules/Session/HudLayout.h"

/*
 * The session HUD without the game: what each view says, how it is set out, and
 * where it sits on the canvas.
 *
 * Text is measured by a stand-in that makes every character six pixels wide,
 * so a position here is a sum that can be read off the test.
 */

using namespace HudLayout;
using namespace SessionEvents;

namespace {

const unsigned long long kSecond = 1000;
const Size kCanvas = { 1344, 700 };

Metrics SixWide() {
	Metrics metrics;
	metrics.width = [](const std::string& text) { return (int)text.size() * 6; };
	metrics.textHeight = 8;
	metrics.rowGap = 3;
	metrics.labelGap = 5;
	metrics.groupGap = 12;
	metrics.sectionGap = 2;
	metrics.padding = 4;
	return metrics;
}

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
	CHECK(ViewNamed("splits") == ViewSplits);
	CHECK(ViewNamed("Full") == ViewFull);
	CHECK(ViewNamed("everything") == ViewCount);
	CHECK(ViewNamed("") == ViewCount);
}

TEST_CASE("stepping through the views goes round, either way") {
	CHECK(Next(ViewLine, 1) == ViewCompact);
	CHECK(Next(ViewCompact, 1) == ViewSplits);
	CHECK(Next(ViewFull, 1) == ViewLine);
	CHECK(Next(ViewLine, -1) == ViewFull);
	CHECK(Next(ViewCompact, -5) == ViewLine);
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

TEST_CASE("one row is set out left to right, a figure close to its name") {
	const Block block = Arrange({ { { "ab", ToneValue }, { "Game", ToneLabel },
		{ "3:21", ToneValue } } }, SixWide());

	REQUIRE(block.cells.size() == 3);
	// Padding, then 12 wide, the gap after a figure, 24 wide, the gap after a name.
	CHECK(block.cells[0].at.x == 4);
	CHECK(block.cells[1].at.x == 4 + 12 + 12);
	CHECK(block.cells[2].at.x == 4 + 12 + 12 + 24 + 5);
	CHECK(block.cells[0].at.y == 4);
	CHECK(block.size.width == 4 + 12 + 12 + 24 + 5 + 24 + 4);
	CHECK(block.size.height == 4 + 8 + 4);
}

TEST_CASE("rows stand in columns as wide as their widest cell") {
	const Block block = Arrange({
		{ { "Session", ToneLabel }, { "1:12:34", ToneValue } },
		{ { "Game 7", ToneLabel }, { "3:21", ToneValue } },
	}, SixWide());

	REQUIRE(block.cells.size() == 4);
	// Names share their left edge.
	CHECK(block.cells[0].at.x == 4);
	CHECK(block.cells[2].at.x == 4);
	// Figures share their right edge: the column is 42 wide and starts at 51.
	CHECK(block.cells[1].at.x == 4 + 42 + 5);
	CHECK(block.cells[3].at.x == 4 + 42 + 5 + (42 - 24));
	CHECK(block.cells[2].at.y == 4 + 8 + 3);
	CHECK(block.size.width == 4 + 42 + 5 + 42 + 4);
	CHECK(block.size.height == 4 + 8 + 3 + 8 + 4);
}

TEST_CASE("nothing to show is a block of nothing but its padding") {
	const Block block = Arrange({}, SixWide());
	CHECK(block.cells.empty());
	CHECK(block.size.width == 8);
	CHECK(block.size.height == 8);
}

TEST_CASE("a block pinned to a corner is that far in from it") {
	const Size block = { 200, 40 };
	CHECK(Resolve({ SideNear, SideNear, 10, 6 }, kCanvas, block).x == 10);
	CHECK(Resolve({ SideNear, SideNear, 10, 6 }, kCanvas, block).y == 6);
	CHECK(Resolve({ SideFar, SideFar, 10, 6 }, kCanvas, block).x == 1344 - 200 - 10);
	CHECK(Resolve({ SideFar, SideFar, 10, 6 }, kCanvas, block).y == 700 - 40 - 6);
}

TEST_CASE("a block pinned to the middle is centred, and offset from there") {
	const Size block = { 200, 40 };
	CHECK(Resolve({ SideMiddle, SideNear, 0, 4 }, kCanvas, block).x == (1344 - 200) / 2);
	CHECK(Resolve({ SideMiddle, SideNear, -30, 4 }, kCanvas, block).x == (1344 - 200) / 2 - 30);
	CHECK(Resolve({ SideNear, SideMiddle, 0, 25 }, kCanvas, block).y == (700 - 40) / 2 + 25);
}

TEST_CASE("a block is kept wholly on the canvas") {
	const Size block = { 200, 40 };
	CHECK(Resolve({ SideNear, SideNear, -50, -50 }, kCanvas, block).x == 0);
	CHECK(Resolve({ SideNear, SideNear, -50, -50 }, kCanvas, block).y == 0);
	CHECK(Resolve({ SideNear, SideNear, 5000, 5000 }, kCanvas, block).x == 1344 - 200);
	CHECK(Resolve({ SideNear, SideNear, 5000, 5000 }, kCanvas, block).y == 700 - 40);
	CHECK(Resolve({ SideFar, SideFar, 5000, 5000 }, kCanvas, block).x == 0);
	// Wider than the canvas, it starts at the canvas's edge.
	CHECK(Resolve({ SideMiddle, SideNear, 0, 0 }, { 100, 100 }, block).x == 0);
}

TEST_CASE("a block is pinned to the third of the canvas its middle is in") {
	const Size block = { 200, 40 };
	Place place = PlaceOf({ 20, 10 }, block, kCanvas);
	CHECK(place.across == SideNear);
	CHECK(place.down == SideNear);
	CHECK(place.x == 20);
	CHECK(place.y == 10);

	place = PlaceOf({ 1100, 640 }, block, kCanvas);
	CHECK(place.across == SideFar);
	CHECK(place.down == SideFar);
	CHECK(place.x == 1344 - 200 - 1100);
	CHECK(place.y == 700 - 40 - 640);

	place = PlaceOf({ 600, 320 }, block, kCanvas);
	CHECK(place.across == SideMiddle);
	CHECK(place.down == SideMiddle);
	CHECK(place.x == 600 - (1344 - 200) / 2);
	CHECK(place.y == 320 - (700 - 40) / 2);
}

TEST_CASE("the place of a block puts it back where it was") {
	const Size block = { 173, 37 };
	for (int x = 0; x <= 1344 - 173; x += 97) {
		for (int y = 0; y <= 700 - 37; y += 61) {
			const Point at = Resolve(PlaceOf({ x, y }, block, kCanvas), kCanvas, block);
			CAPTURE(x);
			CAPTURE(y);
			CHECK(at.x == x);
			CHECK(at.y == y);
		}
	}
}

TEST_CASE("a block pinned to the right keeps its right edge when it grows") {
	const Place place = PlaceOf({ 1344 - 200 - 12, 8 }, { 200, 12 }, kCanvas);

	const Point small = Resolve(place, kCanvas, { 200, 12 });
	const Point large = Resolve(place, kCanvas, { 320, 60 });
	CHECK(small.x + 200 == large.x + 320);
	CHECK(small.y == large.y);
}

TEST_CASE("a block pinned to the bottom keeps its bottom edge when it grows") {
	const Place place = PlaceOf({ 10, 700 - 12 - 90 }, { 200, 12 }, kCanvas);

	const Point small = Resolve(place, kCanvas, { 200, 12 });
	const Point large = Resolve(place, kCanvas, { 320, 60 });
	CHECK(small.y + 12 == large.y + 60);
	CHECK(small.x == large.x);
}

TEST_CASE("a block pinned to a corner stays on it when the canvas changes size") {
	const Size block = { 200, 12 };
	const Place place = PlaceOf({ 1344 - 200 - 12, 700 - 12 - 90 }, block, kCanvas);

	const Point at = Resolve(place, { 800, 600 }, block);
	CHECK(at.x == 800 - 200 - 12);
	CHECK(at.y == 600 - 12 - 90);
}

TEST_CASE("a block is under a side panel only where that panel is open, on its half") {
	const Size block = { 200, 12 };
	const Point onLeft = { 100, 100 };
	const Point onRight = { 900, 100 };

	CHECK(UnderPanel(onLeft, block, kCanvas, true, false));
	CHECK_FALSE(UnderPanel(onLeft, block, kCanvas, false, true));
	CHECK(UnderPanel(onRight, block, kCanvas, false, true));
	CHECK_FALSE(UnderPanel(onRight, block, kCanvas, true, false));
	CHECK_FALSE(UnderPanel(onLeft, block, kCanvas, false, false));
	CHECK(UnderPanel(onLeft, block, kCanvas, true, true));
}

TEST_CASE("a block across the middle is under either panel") {
	const Point across = { 600, 4 };
	CHECK(UnderPanel(across, { 200, 12 }, kCanvas, true, false));
	CHECK(UnderPanel(across, { 200, 12 }, kCanvas, false, true));
}

TEST_CASE("a block that stops at the middle is not under the panel beyond it") {
	CHECK_FALSE(UnderPanel({ 472, 4 }, { 200, 12 }, kCanvas, false, true));
	CHECK_FALSE(UnderPanel({ 672, 4 }, { 200, 12 }, kCanvas, true, false));
}

namespace {

// A run of three splits, the first reached ten seconds ahead of the best run
// and the second twenty behind, with the third still to come.
Figures RunFigures() {
	SessionModel::Run best = {};
	SessionModel::Run run = {};
	const char* const names[] = { "Andariel", "Duriel", "Mephisto" };
	const unsigned long long bestAt[] = { 100, 250, 400 };
	const unsigned long long runAt[] = { 90, 270 };
	for (int i = 0; i < 3; i++) {
		best.splits.push_back({ names[i], SessionModel::RunSplit::Reached, false,
			bestAt[i] * kSecond, bestAt[i] * kSecond, 10 });
		run.splits.push_back({ names[i], SessionModel::RunSplit::Open, false, 0, 0, 0 });
		if (i < 2)
			run.splits[i] = { names[i], SessionModel::RunSplit::Reached, false,
				runAt[i] * kSecond, runAt[i] * kSecond, 12 };
	}
	best.ended = best.finished = true;
	best.realMs = best.gameMs = 400 * kSecond;

	RunStore store;
	store.Record("key", best);

	Figures figures = SomeFigures();
	figures.kills = 1713;
	figures.deaths = 1;
	figures.lastDrop = "Ist Rune";
	figures.inRun = true;
	figures.runEnded = false;
	figures.run = RunCompare::Compare(run, store.Find("key"), RunStore::RealTime, 300 * kSecond);
	return figures;
}

std::vector<std::string> Texts(const Section& section) {
	std::vector<std::string> texts;
	for (const Row& row : section)
		texts.push_back(Text(row));
	return texts;
}

}

TEST_CASE("the line view in a run is the split it is on, the run's clock and how it stands") {
	const std::vector<Section> sections = Sections(ViewLine, RunFigures());
	REQUIRE(sections.size() == 1);
	CHECK(Text(sections[0][0]) == "Mephisto 5:00 +20.0 XP +980.4K/s");
	CHECK(sections[0][0][2].tone == ToneBehind);
}

TEST_CASE("the compact view in a run gains a row for it") {
	const std::vector<Section> sections = Sections(ViewCompact, RunFigures());
	REQUIRE(sections.size() == 1);
	REQUIRE(sections[0].size() == 4);
	CHECK(Text(sections[0][3]) == "Mephisto 5:00 Last +20.0");
}

TEST_CASE("the splits view lists the route, with the time to beat for what is still to come") {
	const std::vector<Section> sections = Sections(ViewSplits, RunFigures());
	REQUIRE(sections.size() == 1);
	// The run's clock and the best run's stand under the times, in their column.
	CHECK(Texts(sections[0]) == std::vector<std::string>{
		"Andariel 1:30 -10.0", "Duriel 4:30 +20.0", "Mephisto 6:40 ", "Time 5:00 ", "Best 6:40 " });
	CHECK(sections[0][3][1].tone == ToneValue);
	CHECK(sections[0][4][1].tone == ToneDim);
}

TEST_CASE("a run with no best to beat has a clock and nothing under it") {
	Figures figures = RunFigures();
	figures.run.bestTotal = -1;
	CHECK(Texts(Sections(ViewSplits, figures)[0]).back() == "Time 5:00 ");
}

TEST_CASE("a split's standing is ahead, behind, or the best that stretch has been done") {
	Figures figures = RunFigures();
	Section splits = Sections(ViewSplits, figures)[0];
	// The first stretch took 90 where the best was 100.
	CHECK(splits[0][2].tone == ToneBest);
	CHECK(splits[1][2].tone == ToneBehind);

	figures.run.splits[0].gold = false;
	splits = Sections(ViewSplits, figures)[0];
	CHECK(splits[0][2].tone == ToneAhead);
}

TEST_CASE("the split a run is on is written as a figure, where it stood as a name") {
	const Section splits = Sections(ViewSplits, RunFigures())[0];
	CHECK(splits[0][0].tone == ToneLabel);
	CHECK(splits[2][0].tone == ToneValue);
	CHECK(splits[2][0].left);
	// A time reached is the run's own, and the time to beat is not.
	CHECK(splits[1][1].tone == ToneValue);
	CHECK(splits[2][1].tone == ToneDim);
}

TEST_CASE("a long route shows the splits around the one the run is on") {
	Figures figures = RunFigures();
	figures.run.splits.clear();
	for (int i = 0; i < 15; i++) {
		RunCompare::Line line = {};
		line.name = "Split " + std::to_string(i + 1);
		line.state = i < 9 ? SessionModel::RunSplit::Reached : SessionModel::RunSplit::Open;
		line.time = i < 9 ? (i + 1) * 60 * (long long)kSecond : -1;
		line.best = -1;
		figures.run.splits.push_back(line);
	}
	figures.run.current = 9;

	// Eight splits, and the run's clock and the best run's under them.
	const Section splits = Sections(ViewSplits, figures)[0];
	REQUIRE(splits.size() == 10);
	CHECK(splits.front()[0].text == "Split 7");
	CHECK(splits[3][0].text == "Split 10");
	CHECK(splits[7][0].text == "Split 14");

	// Near the end it is the last eight, so the block does not shrink.
	figures.run.current = 14;
	CHECK(Sections(ViewSplits, figures)[0].front()[0].text == "Split 8");
	CHECK(Sections(ViewSplits, figures)[0][7][0].text == "Split 15");
}

TEST_CASE("the full view is the splits, the session, and what has died and dropped") {
	const std::vector<Section> sections = Sections(ViewFull, RunFigures());
	REQUIRE(sections.size() == 4);
	CHECK(sections[0].size() == 5);
	CHECK(sections[1].size() == 3);
	CHECK(Text(sections[1][0]) == "Session 1:12:34 Played 58:10");
	CHECK(Texts(sections[2]) == std::vector<std::string>{ "Kills 1,713 Deaths 1" });
	CHECK(Texts(sections[3]) == std::vector<std::string>{ "Drop Ist Rune" });
}

TEST_CASE("a view of a run, with no run to show, is the nearest that needs none") {
	Figures figures = SomeFigures();
	figures.kills = 12;

	const std::vector<Section> splits = Sections(ViewSplits, figures);
	REQUIRE(splits.size() == 1);
	CHECK(Text(splits[0][0]) == "Session 1:12:34 Played 58:10");

	const std::vector<Section> full = Sections(ViewFull, figures);
	REQUIRE(full.size() == 2);
	CHECK(Text(full[1][0]) == "Kills 12 Deaths 0");
}

TEST_CASE("a run that has finished says so where it said which split it was on") {
	Figures figures = RunFigures();
	figures.runEnded = true;
	figures.run.current = -1;
	CHECK(Text(Sections(ViewLine, figures)[0][0]).substr(0, 8) == "Finished");
}

TEST_CASE("a run in hand is read off the session, on the clock asked for") {
	SessionModel model;
	model.Apply({ 0, Joined{ "a", 0, 1, 0 } });
	model.Apply({ 10 * kSecond, RunStarted{ "bosses-normal", "Act bosses (Normal)", { "Andariel", "Duriel" } } });
	model.Apply({ 60 * kSecond, Left() });
	model.Apply({ 90 * kSecond, Joined{ "b", 0, 5, 0 } });
	model.Apply({ 100 * kSecond, SplitReached{ 0, false } });

	const Figures real = FiguresOf(model, 130 * kSecond, NULL, RunStore::RealTime);
	CHECK(real.inRun);
	CHECK_FALSE(real.runEnded);
	CHECK(real.run.clock == 120 * (long long)kSecond);
	CHECK(real.run.splits[0].time == 90 * (long long)kSecond);
	CHECK(real.run.current == 1);

	// Thirty seconds of it were spent between two games.
	const Figures game = FiguresOf(model, 130 * kSecond, NULL, RunStore::GameTime);
	CHECK(game.run.clock == 90 * (long long)kSecond);
	CHECK(game.run.splits[0].time == 60 * (long long)kSecond);
}

TEST_CASE("a finished run stays to be read for a minute, and one given up does not") {
	SessionModel model;
	model.Apply({ 0, Joined{ "a", 0, 1, 0 } });
	model.Apply({ 0, RunStarted{ "act1", "Act 1 (Andariel)", { "Andariel" } } });
	model.Apply({ 100 * kSecond, SplitReached{ 0, false } });
	model.Apply({ 100 * kSecond, RunEnded{ true } });

	const Figures soon = FiguresOf(model, 130 * kSecond);
	CHECK(soon.inRun);
	CHECK(soon.runEnded);
	CHECK(soon.run.clock == 100 * (long long)kSecond);
	CHECK_FALSE(FiguresOf(model, 161 * kSecond).inRun);

	model.Apply({ 200 * kSecond, RunStarted{ "act1", "Act 1 (Andariel)", { "Andariel" } } });
	model.Apply({ 210 * kSecond, RunEnded{ false } });
	CHECK_FALSE(FiguresOf(model, 211 * kSecond).inRun);
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
	CHECK_FALSE(figures.inRun);
}

TEST_CASE("sections stand one under the other, each in columns of its own") {
	const Block block = ArrangeSections({
		{ { { "Andariel", ToneLabel }, { "1:30", ToneValue } } },
		{ { { "Run", ToneLabel }, { "12:34:56", ToneValue } } },
	}, SixWide());

	REQUIRE(block.cells.size() == 4);
	// The second section's figure is not pushed out by the first's long name.
	CHECK(block.cells[1].at.x == 4 + 48 + 5);
	CHECK(block.cells[3].at.x == 4 + 18 + 5);
	// A row gap and the gap between sections.
	CHECK(block.cells[2].at.y == 4 + 8 + 3 + 2);
	CHECK(block.size.width == 4 + 48 + 5 + 24 + 4);
	CHECK(block.size.height == 4 + 8 + 3 + 2 + 8 + 4);
}

TEST_CASE("a cell that stands left takes a name's gap after it, whatever its tone") {
	const Block block = Arrange({
		{ { "Duriel", ToneLabel }, { "4:30", ToneValue } },
		{ { "Mephisto", ToneValue, true }, { "6:40", ToneDim } },
	}, SixWide());

	// Both names against the left, both times against the right of one column.
	CHECK(block.cells[0].at.x == 4);
	CHECK(block.cells[2].at.x == 4);
	CHECK(block.cells[1].at.x == 4 + 48 + 5);
	CHECK(block.cells[3].at.x == 4 + 48 + 5);
}