#include "doctest.h"
#include "Drawing/MapIcon/MapIcon.h"
#include <climits>

/*
 * The shapes a map marker is drawn as.
 *
 * What matters is that a shape is where the marker is: centred on it, growing
 * outwards from it with the size, and drawn as the markers already on the map
 * have always been drawn at the sizes that stand in for them.
 */

using namespace MapIcon;

namespace {
	struct Bounds {
		int left, top, right, bottom;	// inclusive
	};

	Bounds Measure(const Shape& shape) {
		Bounds b = { INT_MAX, INT_MAX, INT_MIN, INT_MIN };
		auto take = [&b](int x, int y) {
			if (x < b.left) b.left = x;
			if (x > b.right) b.right = x;
			if (y < b.top) b.top = y;
			if (y > b.bottom) b.bottom = y;
		};
		for (const Line& line : shape.lines) {
			take(line.x1, line.y1);
			take(line.x2, line.y2);
		}
		for (const Fill& fill : shape.fills) {
			take(fill.x, fill.y);
			take(fill.x + fill.width - 1, fill.y + fill.height - 1);
		}
		return b;
	}

	const Icon Drawn[] = { Cross, Square, Box, Diamond, Triangle, Plus };
}

TEST_CASE("the X at size 4 is the monster X") {
	const Line expected[] = {
		{ 0, -2, 4, -4 }, { 4, -4, 8, -2 }, { 8, -2, 4, 0 }, { 4, 0, 8, 2 },
		{ 8, 2, 4, 4 }, { 4, 4, 0, 2 }, { 0, 2, -4, 4 }, { -4, 4, -8, 2 },
		{ -8, 2, -4, 0 }, { -4, 0, -8, -2 }, { -8, -2, -4, -4 }, { -4, -4, 0, -2 },
	};
	Shape shape = Geometry(Cross, 4);

	REQUIRE(shape.lines.size() == 12);
	CHECK(shape.fills.empty());
	for (unsigned int i = 0; i < 12; i++)
		CHECK(shape.lines[i] == expected[i]);
}

TEST_CASE("a square at size 1 is the 2x2 box chests have always been") {
	Shape shape = Geometry(Square, 1);

	REQUIRE(shape.fills.size() == 1);
	CHECK(shape.lines.empty());
	CHECK(shape.fills[0] == Fill{ -1, -1, 2, 2 });
}

TEST_CASE("a box has the footprint of the square of the same size") {
	for (unsigned int size = MinSize; size <= MaxSize; size++) {
		Bounds box = Measure(Geometry(Box, size));
		Bounds square = Measure(Geometry(Square, size));
		CHECK(box.left == square.left);
		CHECK(box.top == square.top);
		CHECK(box.right == square.right);
		CHECK(box.bottom == square.bottom);
	}
}

TEST_CASE("every drawn icon is centred on the marker") {
	for (Icon icon : Drawn) {
		for (unsigned int size = MinSize; size <= MaxSize; size++) {
			Bounds b = Measure(Geometry(icon, size));
			INFO("icon " << Name(icon) << " size " << size);
			// Even extents cannot be centred on a pixel: they lean one pixel to
			// the top left, as the 2x2 chest box always has.
			CHECK(b.left + b.right >= -1);
			CHECK(b.left + b.right <= 0);
			CHECK(b.top + b.bottom >= -1);
			CHECK(b.top + b.bottom <= 0);
		}
	}
}

TEST_CASE("every drawn icon grows with its size") {
	for (Icon icon : Drawn) {
		Bounds previous = Measure(Geometry(icon, MinSize));
		for (unsigned int size = MinSize + 1; size <= MaxSize; size++) {
			Bounds b = Measure(Geometry(icon, size));
			INFO("icon " << Name(icon) << " size " << size);
			CHECK(b.right - b.left > previous.right - previous.left);
			previous = b;
		}
	}
}

TEST_CASE("a size outside the range is the nearest end of it") {
	Bounds small = Measure(Geometry(Square, 0));
	Bounds large = Measure(Geometry(Square, 100));

	CHECK(small.right - small.left == Measure(Geometry(Square, MinSize)).right -
		Measure(Geometry(Square, MinSize)).left);
	CHECK(large.right - large.left == Measure(Geometry(Square, MaxSize)).right -
		Measure(Geometry(Square, MaxSize)).left);
}

TEST_CASE("nothing is drawn for no icon or for the game's sprite") {
	CHECK(Geometry(None, 4).lines.empty());
	CHECK(Geometry(None, 4).fills.empty());
	CHECK(Geometry(Game, 4).lines.empty());
	CHECK(Geometry(Game, 4).fills.empty());
}

TEST_CASE("only shapes have a size and a colour") {
	CHECK_FALSE(HasSize(None));
	CHECK_FALSE(HasColor(None));
	CHECK_FALSE(HasSize(Game));
	CHECK_FALSE(HasColor(Game));
	for (Icon icon : Drawn) {
		CHECK(HasSize(icon));
		CHECK(HasColor(icon));
	}
}

TEST_CASE("every icon's name reads back as that icon") {
	for (int i = 0; i < IconCount; i++) {
		Icon parsed = None;
		REQUIRE(Parse(Name((Icon)i), &parsed));
		CHECK(parsed == (Icon)i);
	}
}

TEST_CASE("names are read whatever their case") {
	Icon parsed = None;
	REQUIRE(Parse("Diamond", &parsed));
	CHECK(parsed == Diamond);
}

TEST_CASE("an unknown name leaves the icon alone") {
	Icon parsed = Plus;
	CHECK_FALSE(Parse("hexagon", &parsed));
	CHECK(parsed == Plus);
}
