#include "MapIcon.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>

namespace MapIcon {
	namespace {
		struct Entry {
			const char* name;
			const char* label;
		};

		const Entry Entries[IconCount] = {
			{ "none", "None" },
			{ "square", "Square" },
			{ "circle", "Circle" },
			{ "diamond", "Diamond" },
			{ "triangle", "Triangle" },
			{ "star", "Star" },
			{ "cross", "Cross" },
			{ "plus", "Plus" },
			{ "chest", "Chest" },
		};

		struct Point {
			int x, y;
		};

		// The monster X as it has always been drawn, at the size it is drawn at as
		// size 4.
		const Point CrossOutline[] = {
			{ 0, -2 }, { 4, -4 }, { 8, -2 }, { 4, 0 }, { 8, 2 }, { 4, 4 }, { 0, 2 },
			{ -4, 4 }, { -8, 2 }, { -4, 0 }, { -8, -2 }, { -4, -4 },
		};
		const int CrossNativeSize = 4;

		// A regular five pointed star's inner points, as a share of its outer.
		const double StarInnerRatio = 0.381966;

		bool Valid(Icon icon) {
			return icon >= None && icon < IconCount;
		}

		int Clamp(unsigned int size) {
			return (int)(std::min)((std::max)(size, MinSize), MaxSize);
		}

		// num / den to the nearest whole number, halves away from zero, so a shape
		// and its mirror image round to mirror images. den is positive.
		int RoundDiv(int num, int den) {
			return num >= 0 ? (2 * num + den) / (2 * den) : -((-2 * num + den) / (2 * den));
		}

		// The corners of a shape with straight sides, in order around it; empty for
		// one without.
		std::vector<Point> Corners(Icon icon, int s) {
			std::vector<Point> points;
			switch (icon) {
				case Square:
					// Even, so it leans a pixel to the top left, as the 2x2 chest box
					// always has.
					points = { { -s, -s }, { s - 1, -s }, { s - 1, s - 1 }, { -s, s - 1 } };
					break;
				case Diamond:
					// Twice as wide as it is tall, as a square on the ground is on the
					// automap.
					points = { { 0, -s }, { 2 * s, 0 }, { 0, s }, { -2 * s, 0 } };
					break;
				case Triangle:
					points = { { 0, -s }, { s, s }, { -s, s } };
					break;
				case Cross:
					for (const Point& point : CrossOutline)
						points.push_back({ point.x * s / CrossNativeSize,
							point.y * s / CrossNativeSize });
					break;
				case Plus: {
					// Arms a pixel wide at the smallest sizes, filling out as it grows.
					int h = s / 3;
					points = {
						{ -h, -s }, { h, -s }, { h, -h }, { s, -h }, { s, h }, { h, h },
						{ h, s }, { -h, s }, { -h, h }, { -s, h }, { -s, -h }, { -h, -h },
					};
					break;
				}
				case Star: {
					// Larger across than the other shapes, since its points leave most
					// of that empty.
					const double pi = 3.14159265358979323846;
					double outer = (double)((3 * s + 1) / 2);
					for (int k = 0; k < 10; k++) {
						double radius = (k % 2 == 0) ? outer : outer * StarInnerRatio;
						double angle = (-90.0 + 36.0 * k) * pi / 180.0;
						points.push_back({ (int)std::lround(radius * std::cos(angle)),
							(int)std::lround(radius * std::sin(angle)) });
					}
					// Its points reach further up than down, so it is moved to sit
					// centred on its height, as every other shape does.
					int top = points[0].y, bottom = points[0].y;
					for (const Point& point : points) {
						top = (std::min)(top, point.y);
						bottom = (std::max)(bottom, point.y);
					}
					int shift = -(top + bottom) / 2;
					for (Point& point : points)
						point.y += shift;
					break;
				}
				default:
					break;
			}
			return points;
		}

		// The pixels a shape covers, over the box that holds it.
		class Mask {
			private:
				int left, top, width, height;
				std::vector<char> cells;
			public:
				Mask(int left, int top, int right, int bottom) : left(left), top(top),
					width(right - left + 1), height(bottom - top + 1),
					cells((size_t)(width * height), 0) {};

				int Left() const { return left; };
				int Top() const { return top; };
				int Right() const { return left + width - 1; };
				int Bottom() const { return top + height - 1; };

				bool At(int x, int y) const {
					if (x < left || y < top || x > Right() || y > Bottom())
						return false;
					return cells[(size_t)((y - top) * width + (x - left))] != 0;
				}
				void Set(int x, int y) {
					if (x < left || y < top || x > Right() || y > Bottom())
						return;
					cells[(size_t)((y - top) * width + (x - left))] = 1;
				}
		};

		// Every pixel the line passes nearest, one per step along its longer axis.
		void DrawLine(Mask& mask, Point a, Point b) {
			int dx = b.x - a.x, dy = b.y - a.y;
			if (std::abs(dx) >= std::abs(dy)) {
				if (dx < 0) { std::swap(a, b); dx = -dx; dy = -dy; }
				if (dx == 0) {
					mask.Set(a.x, a.y);
					return;
				}
				for (int x = a.x; x <= b.x; x++)
					mask.Set(x, RoundDiv(a.y * dx + (x - a.x) * dy, dx));
			} else {
				if (dy < 0) { std::swap(a, b); dx = -dx; dy = -dy; }
				for (int y = a.y; y <= b.y; y++)
					mask.Set(RoundDiv(a.x * dy + (y - a.y) * dx, dy), y);
			}
		}

		// Whether a pixel's centre is inside the corners, by how many sides a ray
		// from it to the right crosses. Exact, as the corners are whole pixels.
		bool Inside(const std::vector<Point>& points, int px, int py) {
			bool inside = false;
			for (size_t i = 0, j = points.size() - 1; i < points.size(); j = i++) {
				const Point& a = points[i];
				const Point& b = points[j];
				if ((a.y > py) == (b.y > py))
					continue;
				int dy = b.y - a.y;
				long long lhs = (long long)(px - a.x) * dy;
				long long rhs = (long long)(py - a.y) * (b.x - a.x);
				if (dy > 0 ? lhs < rhs : lhs > rhs)
					inside = !inside;
			}
			return inside;
		}

		// The pixels a shape covers, and of those the ones on its edge: its sides,
		// and wherever else it meets a pixel outside it.
		struct Coverage {
			Mask inside;
			Mask edge;
		};

		Coverage Cover(Icon icon, int s) {
			std::vector<Point> points;
			int left = -s, top = -s, right = s, bottom = s;
			if (icon != Circle) {
				points = Corners(icon, s);
				if (points.empty())
					return Coverage{ Mask(0, 0, -1, -1), Mask(0, 0, -1, -1) };
				left = right = points[0].x;
				top = bottom = points[0].y;
				for (const Point& point : points) {
					left = (std::min)(left, point.x);
					right = (std::max)(right, point.x);
					top = (std::min)(top, point.y);
					bottom = (std::max)(bottom, point.y);
				}
			}
			Coverage cover = { Mask(left, top, right, bottom), Mask(left, top, right, bottom) };
			Mask& inside = cover.inside;
			if (icon == Circle) {
				// A pixel's width past the true radius, so the circle has no lone
				// pixel standing out at the top, bottom and sides.
				for (int y = -s; y <= s; y++)
					for (int x = -s; x <= s; x++)
						if (x * x + y * y <= s * s + s)
							inside.Set(x, y);
			} else {
				for (int y = top; y <= bottom; y++)
					for (int x = left; x <= right; x++)
						if (Inside(points, x, y))
							inside.Set(x, y);
				// The sides themselves, so the edge is as crisp as the outline is.
				// They are the edge even at a corner that turns inward, where a
				// side's pixel has nothing outside it.
				for (size_t i = 0; i < points.size(); i++) {
					DrawLine(inside, points[i], points[(i + 1) % points.size()]);
					DrawLine(cover.edge, points[i], points[(i + 1) % points.size()]);
				}
			}
			for (int y = top; y <= bottom; y++)
				for (int x = left; x <= right; x++)
					if (inside.At(x, y) && (!inside.At(x - 1, y) || !inside.At(x + 1, y) ||
							!inside.At(x, y - 1) || !inside.At(x, y + 1)))
						cover.edge.Set(x, y);
			return cover;
		}

		// A row's pixels of one kind are added as runs, and a run under one of the
		// same span on the row above extends it rather than starting another.
		void AddRun(std::vector<Rect>& rects, int x, int y, int width) {
			for (Rect& rect : rects) {
				if (rect.x == x && rect.width == width && rect.y + rect.height == y) {
					rect.height++;
					return;
				}
			}
			rects.push_back(Rect{ x, y, width, 1 });
		}
	}

	Raster Rasterize(Icon icon, unsigned int size, unsigned int borderWidth) {
		Raster raster;
		if (!IsShape(icon))
			return raster;
		const int s = Clamp(size);
		const int w = (int)(std::min)(borderWidth, MaxBorder);
		Coverage cover = Cover(icon, s);
		const Mask& mask = cover.inside;

		// A pixel is border if the edge is less than the width away from it, so a
		// border a pixel wide is the edge itself.
		const int reach = w - 1;
		auto border = [&cover, reach](int x, int y) {
			for (int dy = -reach; dy <= reach; dy++)
				for (int dx = -reach; dx <= reach; dx++)
					if (dx * dx + dy * dy <= reach * reach && cover.edge.At(x + dx, y + dy))
						return true;
			return false;
		};

		for (int y = mask.Top(); y <= mask.Bottom(); y++) {
			int runStart = 0, runKind = 0;		// 0 nothing, 1 fill, 2 border
			for (int x = mask.Left(); x <= mask.Right() + 1; x++) {
				int kind = 0;
				if (x <= mask.Right() && mask.At(x, y))
					kind = (w > 0 && border(x, y)) ? 2 : 1;
				if (kind == runKind)
					continue;
				if (runKind != 0)
					AddRun(runKind == 2 ? raster.border : raster.fill, runStart, y,
						x - runStart);
				runStart = x;
				runKind = kind;
			}
		}
		return raster;
	}

	std::vector<Line> Outline(Icon icon, unsigned int size) {
		std::vector<Line> lines;
		if (!IsShape(icon))
			return lines;
		std::vector<Point> points = Corners(icon, Clamp(size));
		for (size_t i = 0; i < points.size(); i++) {
			const Point& a = points[i];
			const Point& b = points[(i + 1) % points.size()];
			lines.push_back(Line{ a.x, a.y, b.x, b.y });
		}
		return lines;
	}

	bool IsShape(Icon icon) {
		return Valid(icon) && icon != None && icon != Chest;
	}

	const char* Name(Icon icon) {
		return Valid(icon) ? Entries[icon].name : Entries[None].name;
	}

	const char* Label(Icon icon) {
		return Valid(icon) ? Entries[icon].label : Entries[None].label;
	}

	bool Parse(const std::string& name, Icon* icon) {
		std::string lower = name;
		std::transform(lower.begin(), lower.end(), lower.begin(),
			[](unsigned char c) { return (char)std::tolower(c); });
		for (int i = 0; i < IconCount; i++) {
			if (lower.compare(Entries[i].name) == 0) {
				*icon = (Icon)i;
				return true;
			}
		}
		return false;
	}
}
