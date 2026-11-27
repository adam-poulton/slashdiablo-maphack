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
	CHECK(ViewNamed("splits") == ViewCount);
	CHECK(ViewNamed("") == ViewCount);
}

TEST_CASE("stepping through the views goes round, either way") {
	CHECK(Next(ViewLine, 1) == ViewCompact);
	CHECK(Next(ViewCompact, 1) == ViewLine);
	CHECK(Next(ViewLine, -1) == ViewCompact);
	CHECK(Next(ViewCompact, -3) == ViewLine);
}

TEST_CASE("the line view is the session clock, the game and its clock, and the rate") {
	const std::vector<Row> rows = Rows(ViewLine, SomeFigures());
	REQUIRE(rows.size() == 1);
	CHECK(Text(rows[0]) == "1:12:34 Game 7 3:21 XP +980.4K/s");
}

TEST_CASE("the compact view is three rows of names and figures") {
	const std::vector<Row> rows = Rows(ViewCompact, SomeFigures());
	REQUIRE(rows.size() == 3);
	CHECK(Text(rows[0]) == "Session 1:12:34 Played 58:10");
	CHECK(Text(rows[1]) == "Game 7 3:21 XP +1.25M/s");
	CHECK(Text(rows[2]) == "Session XP +980.4K/s Recent +1.40M/s");
}

TEST_CASE("a name is told from a figure") {
	const std::vector<Row> rows = Rows(ViewCompact, SomeFigures());
	for (const Row& row : rows) {
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
