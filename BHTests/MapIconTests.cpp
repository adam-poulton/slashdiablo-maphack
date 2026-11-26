#include "doctest.h"
#include "Drawing/MapIcon/MapIcon.h"
#include <climits>
#include <cmath>
#include <cstdlib>
#include <set>
#include <utility>

/*
 * The shapes a map marker is drawn as.
 *
 * What matters is that a shape is where the marker is: centred on it, growing
 * outwards from it with the size, its border as wide as asked and its fill the
 * rest, and drawn as the markers already on the map have always been drawn at the
 * sizes that stand in for them.
 */

using namespace MapIcon;

namespace {
	typedef std::set<std::pair<int, int>> Pixels;

	struct Bounds {
		int left, top, right, bottom;	// inclusive
	};

	// Every pixel the rects cover, failing if any is covered twice.
	void Add(Pixels& pixels, const std::vector<Rect>& rects) {
		for (const Rect& rect : rects)
			for (int y = rect.y; y < rect.y + rect.height; y++)
				for (int x = rect.x; x < rect.x + rect.width; x++)
					CHECK(pixels.insert(std::make_pair(x, y)).second);
	}

	Pixels Covered(const Raster& raster) {
		Pixels pixels;
		Add(pixels, raster.fill);
		Add(pixels, raster.border);
		return pixels;
	}

	Pixels Covered(const std::vector<Rect>& rects) {
		Pixels pixels;
		Add(pixels, rects);
		return pixels;
	}

	Bounds Measure(const Pixels& pixels) {
		Bounds b = { INT_MAX, INT_MAX, INT_MIN, INT_MIN };
		for (const std::pair<int, int>& pixel : pixels) {
			if (pixel.first < b.left) b.left = pixel.first;
			if (pixel.first > b.right) b.right = pixel.first;
			if (pixel.second < b.top) b.top = pixel.second;
			if (pixel.second > b.bottom) b.bottom = pixel.second;
		}
		return b;
	}

	// The pixels a line between pixel centres runs nearest, worked out apart from
	// the code under test.
	void LinePixels(Pixels& pixels, const Line& line) {
		int dx = line.x2 - line.x1, dy = line.y2 - line.y1;
		int steps = std::abs(dx) > std::abs(dy) ? std::abs(dx) : std::abs(dy);
		// Divided last, so a pixel's worth and a half is exactly that, and rounds
		// away from the middle as the shapes do.
		for (int i = 0; i <= steps; i++) {
			double x = steps == 0 ? line.x1 : line.x1 + (double)(i * dx) / steps;
			double y = steps == 0 ? line.y1 : line.y1 + (double)(i * dy) / steps;
			pixels.insert(std::make_pair((int)std::lround(x), (int)std::lround(y)));
		}
	}

	const Icon Shapes[] = { Square, Circle, Diamond, Triangle, Star, Cross, Plus };
	const Icon Outlined[] = { Square, Diamond, Triangle, Star, Cross, Plus };
}

TEST_CASE("the X at size 4 is the monster X") {
	const Line expected[] = {
		{ 0, -2, 4, -4 }, { 4, -4, 8, -2 }, { 8, -2, 4, 0 }, { 4, 0, 8, 2 },
		{ 8, 2, 4, 4 }, { 4, 4, 0, 2 }, { 0, 2, -4, 4 }, { -4, 4, -8, 2 },
		{ -8, 2, -4, 0 }, { -4, 0, -8, -2 }, { -8, -2, -4, -4 }, { -4, -4, 0, -2 },
	};
	std::vector<Line> lines = Outline(Cross, 4);

	REQUIRE(lines.size() == 12);
	for (unsigned int i = 0; i < 12; i++)
		CHECK(lines[i] == expected[i]);
}

TEST_CASE("a square at size 1 is the 2x2 box chests have always been") {
	Raster raster = Rasterize(Square, 1, 0);

	REQUIRE(raster.fill.size() == 1);
	CHECK(raster.border.empty());
	CHECK(raster.fill[0] == Rect{ -1, -1, 2, 2 });
}

TEST_CASE("a bordered square is a ring around its fill") {
	Raster raster = Rasterize(Square, 4, 1);

	CHECK(Covered(raster) == Covered(Rasterize(Square, 4, 0)));
	Bounds fill = Measure(Covered(raster.fill));
	CHECK(fill.left == -3);
	CHECK(fill.top == -3);
	CHECK(fill.right == 2);
	CHECK(fill.bottom == 2);
	CHECK(Covered(raster.border).size() == 8 * 8 - 6 * 6);
}

TEST_CASE("a one pixel border is the shape's outline") {
	for (Icon icon : Outlined) {
		for (unsigned int size = MinSize; size <= MaxSize; size++) {
			INFO("icon " << std::string(Name(icon)) << " size " << size);
			Pixels outline;
			for (const Line& line : Outline(icon, size))
				LinePixels(outline, line);
			CHECK(Covered(Rasterize(icon, size, 1).border) == outline);

		}
	}
}

TEST_CASE("a border takes the shape's own footprint, whatever its width") {
	for (Icon icon : Shapes) {
		for (unsigned int size = MinSize; size <= MaxSize; size++) {
			Pixels whole = Covered(Rasterize(icon, size, 0));
			for (unsigned int width = MinBorder; width <= MaxBorder; width++) {
				INFO("icon " << std::string(Name(icon)) << " size " << size << " width " << width);
				CHECK(Covered(Rasterize(icon, size, width)) == whole);
			}
		}
	}
}

TEST_CASE("a wider border reaches further in, until it is the whole shape") {
	for (Icon icon : Shapes) {
		size_t whole = Covered(Rasterize(icon, MaxSize, 0)).size();
		Pixels previous = Covered(Rasterize(icon, MaxSize, MinBorder).border);
		for (unsigned int width = MinBorder + 1; width <= MaxBorder; width++) {
			INFO("icon " << std::string(Name(icon)) << " width " << width);
			Pixels border = Covered(Rasterize(icon, MaxSize, width).border);
			CHECK((border.size() > previous.size() || previous.size() == whole));
			for (const std::pair<int, int>& pixel : previous)
				CHECK(border.count(pixel) == 1);
			previous = border;
		}
	}
}

TEST_CASE("every shape is centred on the marker") {
	for (Icon icon : Shapes) {
		for (unsigned int size = MinSize; size <= MaxSize; size++) {
			Bounds b = Measure(Covered(Rasterize(icon, size, 0)));
			INFO("icon " << std::string(Name(icon)) << " size " << size);
			// Even extents cannot be centred on a pixel: they lean one pixel to
			// the top left, as the 2x2 chest box always has.
			CHECK(b.left + b.right >= -1);
			CHECK(b.left + b.right <= 0);
			CHECK(b.top + b.bottom >= -1);
			CHECK(b.top + b.bottom <= 0);
		}
	}
}

TEST_CASE("every shape but the square is its own mirror image") {
	for (Icon icon : Shapes) {
		if (icon == Square)
			continue;
		for (unsigned int size = MinSize; size <= MaxSize; size++) {
			INFO("icon " << std::string(Name(icon)) << " size " << size);
			Pixels pixels = Covered(Rasterize(icon, size, 1));
			for (const std::pair<int, int>& pixel : pixels)
				CHECK(pixels.count(std::make_pair(-pixel.first, pixel.second)) == 1);
		}
	}
}

TEST_CASE("every shape grows with its size") {
	for (Icon icon : Shapes) {
		Bounds previous = Measure(Covered(Rasterize(icon, MinSize, 0)));
		for (unsigned int size = MinSize + 1; size <= MaxSize; size++) {
			Bounds b = Measure(Covered(Rasterize(icon, size, 0)));
			INFO("icon " << std::string(Name(icon)) << " size " << size);
			CHECK(b.right - b.left > previous.right - previous.left);
			previous = b;
		}
	}
}

TEST_CASE("a size or width outside the range is the nearest end of it") {
	CHECK(Covered(Rasterize(Circle, 0, 0)) == Covered(Rasterize(Circle, MinSize, 0)));
	CHECK(Covered(Rasterize(Circle, 100, 0)) == Covered(Rasterize(Circle, MaxSize, 0)));
	CHECK(Covered(Rasterize(Circle, MaxSize, 100).border) ==
		Covered(Rasterize(Circle, MaxSize, MaxBorder).border));
}

TEST_CASE("nothing is drawn for no icon or for the game's sprite") {
	for (Icon icon : { None, Chest }) {
		Raster raster = Rasterize(icon, 4, 1);
		CHECK(raster.fill.empty());
		CHECK(raster.border.empty());
		CHECK(Outline(icon, 4).empty());
		CHECK_FALSE(IsShape(icon));
	}
	for (Icon icon : Shapes)
		CHECK(IsShape(icon));
}

TEST_CASE("no colour at all draws nothing") {
	CHECK_FALSE(Look(NoColor, NoColor, 1).Draws());
	CHECK(Look(0xFF, NoColor, 1).Draws());
	CHECK(Look(NoColor, 0xFF, 1).Draws());
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
