#pragma once
#include <string>
#include <vector>

// The shapes a marker on the automap can be drawn as, and the geometry of each.
//
// Geometry is kept apart from drawing so that what a shape is can be tested
// without the game: Draw() is the only part that touches D2GFX, and it lives in
// MapIconDraw.cpp.
namespace MapIcon {
	// The order is the order the icons are offered in. The names are what the
	// config stores, so a name never changes once released; a new icon is added at
	// the end.
	enum Icon {
		None,		// nothing at the marker itself, for a marker that is only its line
		Cross,		// the X monsters have always been drawn with
		Square,		// filled
		Box,		// a square's outline
		Diamond,	// an outline lying along the automap's isometric grid
		Triangle,	// an outline, point up
		Plus,
		Game,		// the game's own automap sprite for a super chest
		IconCount
	};

	// A size is a step, not pixels: each shape says what one step is worth to it.
	// A filled square at size 1 is the 2x2 box chests have always been drawn as,
	// and the X at size 4 is the monster X.
	const unsigned int MinSize = 1;
	const unsigned int MaxSize = 8;

	// Relative to the marker's centre. A line includes both its ends.
	struct Line {
		int x1, y1, x2, y2;
		bool operator==(const Line& other) const {
			return x1 == other.x1 && y1 == other.y1 && x2 == other.x2 && y2 == other.y2;
		}
	};

	// Relative to the marker's centre: left, top, and the extent of the fill, so a
	// fill covers x to x + width - 1.
	struct Fill {
		int x, y, width, height;
		bool operator==(const Fill& other) const {
			return x == other.x && y == other.y && width == other.width &&
				height == other.height;
		}
	};

	struct Shape {
		std::vector<Line> lines;
		std::vector<Fill> fills;
	};

	// What the icon draws at the given size, clamped to the size range. Empty for
	// None, and for Game, which is a sprite with a size of its own.
	Shape Geometry(Icon icon, unsigned int size);

	// Whether size and colour mean anything for the icon. Game is drawn in the
	// sprite's own colours at the sprite's own size; None draws nothing to size.
	bool HasSize(Icon icon);
	bool HasColor(Icon icon);

	// The config name, and back. Parsing ignores case; an unknown name is false and
	// leaves the icon as it was.
	const char* Name(Icon icon);
	bool Parse(const std::string& name, Icon* icon);

	// What the icon is called where it is offered, in sentence case.
	const char* Label(Icon icon);

	// The automap frame of the game's super chest sprite.
	const unsigned int GameCell = 318;

	// Both defined in MapIconDraw.cpp.
	//
	// Draws the icon centred on a point on screen, for showing what an icon looks
	// like away from the automap.
	void Draw(Icon icon, unsigned int size, int x, int y, unsigned int color);

	// Draws the icon at a position in the world on the automap. Apart from Draw()
	// because the game's sprite is placed the way the game places its own cells,
	// which is from the world position and not from a point on screen.
	void DrawAt(Icon icon, unsigned int size, int worldX, int worldY,
		unsigned int color);
}
