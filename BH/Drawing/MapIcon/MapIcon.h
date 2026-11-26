#pragma once
#include <string>
#include <vector>

// The shapes a marker on the automap can be drawn as, and the geometry of each.
//
// Geometry is kept apart from drawing so that what a shape is can be tested
// without the game: the drawing functions are the only part that touches D2GFX,
// and they live in MapIconDraw.cpp.
namespace MapIcon {
	// The order is the order the icons are offered in. The names are what the
	// config stores, so a name never changes once released.
	enum Icon {
		None,		// nothing at the marker itself, for a marker that is only its line
		Square,
		Circle,
		Diamond,	// lying along the automap's isometric grid
		Triangle,	// point up
		Star,		// five points, one up
		Cross,		// the X monsters have always been drawn with
		Plus,
		Chest,		// the game's own automap sprite for a super chest
		IconCount
	};

	// A size is a step, not pixels: each shape says what one step is worth to it.
	// A square at size 1 is the 2x2 box chests have always been drawn as, and the
	// X at size 4 is the monster X.
	const unsigned int MinSize = 1;
	const unsigned int MaxSize = 8;

	// How far in from a shape's edge its border reaches, in pixels.
	const unsigned int MinBorder = 1;
	const unsigned int MaxBorder = 4;

	// The palette's 256 entries are colours, black at 0 among them. No colour at
	// all is the number after the last, which is never drawn with.
	const unsigned int PaletteSize = 256;
	const unsigned int NoColor = PaletteSize;

	// Relative to the marker's centre. A line includes both its ends.
	struct Line {
		int x1, y1, x2, y2;
		bool operator==(const Line& other) const {
			return x1 == other.x1 && y1 == other.y1 && x2 == other.x2 && y2 == other.y2;
		}
	};

	// Relative to the marker's centre: left, top, and the extent, so a rect covers
	// x to x + width - 1.
	struct Rect {
		int x, y, width, height;
		bool operator==(const Rect& other) const {
			return x == other.x && y == other.y && width == other.width &&
				height == other.height;
		}
	};

	// The pixels a shape covers, as the rectangles they are drawn with: those
	// within the border's width of the shape's edge, and the rest inside them.
	// No pixel is in more than one rectangle.
	struct Raster {
		std::vector<Rect> fill;
		std::vector<Rect> border;
	};

	// What the icon covers at the given size, clamped to the size range, with a
	// border of the given width. A width of 0 leaves it all fill. Empty for None,
	// and for Chest, which is a sprite with a size of its own.
	Raster Rasterize(Icon icon, unsigned int size, unsigned int borderWidth);

	// The icon's edge a pixel wide, as lines, for drawing one in a single colour
	// as cheaply as it can be: the monster X is drawn for every monster on the
	// map. Empty for a shape without straight sides.
	std::vector<Line> Outline(Icon icon, unsigned int size);

	// Whether size, fill and border mean anything for the icon. Chest is drawn in
	// the sprite's own colours at the sprite's own size; None draws nothing.
	bool IsShape(Icon icon);

	// The config name, and back. Parsing ignores case; an unknown name is false and
	// leaves the icon as it was.
	const char* Name(Icon icon);
	bool Parse(const std::string& name, Icon* icon);

	// What the icon is called where it is offered, in sentence case.
	const char* Label(Icon icon);

	// The automap frame of the game's super chest sprite, and how much of it
	// shows.
	const unsigned int ChestCell = 318;
	const unsigned int ChestWidth = 14;
	const unsigned int ChestHeight = 23;

	// What a shape is drawn in. Either colour can be NoColor; with no border the
	// fill takes the whole shape.
	struct Look {
		unsigned int fill;
		unsigned int border;
		unsigned int borderWidth;

		Look() : fill(NoColor), border(NoColor), borderWidth(MinBorder) {};
		Look(unsigned int fill, unsigned int border, unsigned int borderWidth) :
			fill(fill), border(border), borderWidth(borderWidth) {};

		bool Draws() const { return fill != NoColor || border != NoColor; };
	};

	// All defined in MapIconDraw.cpp.
	//
	// Draws the icon centred on a point on screen, for showing what an icon looks
	// like away from the automap. The game's sprite, ChestWidth by ChestHeight,
	// stands taller than a shape of the sizes shown away from the map, so whatever
	// holds one leaves room.
	void Draw(Icon icon, unsigned int size, int x, int y, const Look& look);

	// Draws the icon at a position in the world on the automap. Apart from Draw()
	// because the game's sprite is placed the way the game places its own cells,
	// which is from the world position and not from a point on screen.
	void DrawAt(Icon icon, unsigned int size, int worldX, int worldY,
		const Look& look);

	// Draws Outline() centred on a point on screen.
	void DrawOutline(Icon icon, unsigned int size, int x, int y, unsigned int color);
}
