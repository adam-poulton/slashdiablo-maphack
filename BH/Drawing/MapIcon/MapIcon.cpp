#include "MapIcon.h"
#include <algorithm>
#include <cctype>

namespace MapIcon {
	namespace {
		struct Entry {
			const char* name;
			const char* label;
		};

		const Entry Entries[IconCount] = {
			{ "none", "None" },
			{ "cross", "Cross" },
			{ "square", "Square" },
			{ "box", "Box" },
			{ "diamond", "Diamond" },
			{ "triangle", "Triangle" },
			{ "plus", "Plus" },
			{ "game", "Game icon" },
		};

		// The monster X as it has always been drawn: an outline, not two strokes, at
		// the size it is drawn at as size 4.
		const int CrossOutline[][2] = {
			{ 0, -2 }, { 4, -4 }, { 8, -2 }, { 4, 0 }, { 8, 2 }, { 4, 4 }, { 0, 2 },
			{ -4, 4 }, { -8, 2 }, { -4, 0 }, { -8, -2 }, { -4, -4 }, { 0, -2 },
		};
		const unsigned int CrossNativeSize = 4;

		void Polyline(Shape& shape, const int points[][2], unsigned int count) {
			for (unsigned int i = 0; i + 1 < count; i++)
				shape.lines.push_back(Line{ points[i][0], points[i][1],
					points[i + 1][0], points[i + 1][1] });
		}

		bool Valid(Icon icon) {
			return icon >= None && icon < IconCount;
		}
	}

	Shape Geometry(Icon icon, unsigned int size) {
		Shape shape;
		const int s = (int)std::min(std::max(size, MinSize), MaxSize);
		switch (icon) {
			case Cross: {
				const unsigned int count = sizeof(CrossOutline) / sizeof(CrossOutline[0]);
				int points[count][2];
				for (unsigned int i = 0; i < count; i++) {
					points[i][0] = CrossOutline[i][0] * s / (int)CrossNativeSize;
					points[i][1] = CrossOutline[i][1] * s / (int)CrossNativeSize;
				}
				Polyline(shape, points, count);
				break;
			}
			case Square:
				shape.fills.push_back(Fill{ -s, -s, 2 * s, 2 * s });
				break;
			case Box: {
				// The same footprint as the square of that size, so the two can be
				// swapped without the marker moving.
				const int points[][2] = {
					{ -s, -s }, { s - 1, -s }, { s - 1, s - 1 }, { -s, s - 1 }, { -s, -s },
				};
				Polyline(shape, points, 5);
				break;
			}
			case Diamond: {
				// Twice as wide as it is tall, as a square on the ground is on the
				// automap.
				const int points[][2] = {
					{ 0, -s }, { 2 * s, 0 }, { 0, s }, { -2 * s, 0 }, { 0, -s },
				};
				Polyline(shape, points, 5);
				break;
			}
			case Triangle: {
				const int points[][2] = {
					{ 0, -s }, { s, s }, { -s, s }, { 0, -s },
				};
				Polyline(shape, points, 4);
				break;
			}
			case Plus:
				shape.lines.push_back(Line{ -s, 0, s, 0 });
				shape.lines.push_back(Line{ 0, -s, 0, s });
				break;
			default:
				break;
		}
		return shape;
	}

	bool HasSize(Icon icon) {
		return Valid(icon) && icon != None && icon != Game;
	}

	bool HasColor(Icon icon) {
		return HasSize(icon);
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
