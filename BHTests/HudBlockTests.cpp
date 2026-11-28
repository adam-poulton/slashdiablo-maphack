#include "doctest.h"
#include <string>
#include <vector>
#include "Modules/Tracking/HudBlock.h"

/*
 * A HUD's block without the game: how rows are set out, and where the block
 * sits on the canvas.
 *
 * Text is measured by a stand-in that makes every character six pixels wide,
 * so a position here is a sum that can be read off the test.
 */

using namespace HudBlock;

namespace {

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
