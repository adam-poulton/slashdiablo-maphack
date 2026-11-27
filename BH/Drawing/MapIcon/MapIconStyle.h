#pragma once
#include <string>
#include <vector>
#include "MapIcon.h"

// How one kind of marker is drawn on the automap: the icon at the marker, and a
// line to it from the player.
//
// A colour of MapIcon::NoColor leaves that part of the icon out. The line keeps
// its colour while it is off, so turning it back on does not lose the colour that
// was chosen for it.
struct MapIconStyle {
	unsigned int icon;		// a MapIcon::Icon, held as the number the config reads into
	unsigned int size;
	unsigned int fillColor;
	unsigned int borderColor;
	unsigned int borderWidth;
	bool line;
	unsigned int lineColor;

	MapIconStyle() : icon(MapIcon::Square), size(1), fillColor(0xFF),
		borderColor(MapIcon::NoColor), borderWidth(MapIcon::MinBorder), line(false),
		lineColor(0xFF) {};

	MapIcon::Icon Icon() const {
		return icon < MapIcon::IconCount ? (MapIcon::Icon)icon : MapIcon::None;
	};

	MapIcon::Look Look() const {
		return MapIcon::Look(fillColor, borderColor, borderWidth);
	};

	// Whether the icon puts anything at the marker: the game's sprite always does,
	// and a shape does in whichever of its colours it has.
	bool DrawsIcon() const {
		MapIcon::Icon shown = Icon();
		return shown == MapIcon::Chest || (MapIcon::IsShape(shown) && Look().Draws());
	};

	// Whether the style puts anything on the map at all.
	bool Draws() const { return DrawsIcon() || line; };

	bool operator==(const MapIconStyle& other) const {
		return icon == other.icon && size == other.size &&
			fillColor == other.fillColor && borderColor == other.borderColor &&
			borderWidth == other.borderWidth && line == other.line &&
			lineColor == other.lineColor;
	};
	bool operator!=(const MapIconStyle& other) const { return !(*this == other); };
};

// A style that changes only part of another, where it is edited: whether it has
// an icon of its own, and what it does to the line. A part it leaves alone is
// shown as the base has it, the base being whatever the style is laid over.
struct MapIconOverride {
	// What line holds, in the order they are offered.
	enum Line { LineUnchanged, LineOff, LineOn };

	bool* setsIcon;
	unsigned int* line;
	const MapIconStyle* base;

	MapIconOverride() : setsIcon(NULL), line(NULL), base(NULL) {};
	MapIconOverride(bool* setsIcon, unsigned int* line, const MapIconStyle* base) :
		setsIcon(setsIcon), line(line), base(base) {};

	// The base with the style's own parts laid over it.
	MapIconStyle Over(const MapIconStyle& style) const {
		MapIconStyle shown = base ? *base : style;
		if (!setsIcon || *setsIcon) {
			shown.icon = style.icon;
			shown.size = style.size;
			shown.fillColor = style.fillColor;
			shown.borderColor = style.borderColor;
			shown.borderWidth = style.borderWidth;
		}
		if (line && *line == LineOff) {
			shown.line = false;
		} else if (line && *line == LineOn) {
			shown.line = true;
			shown.lineColor = style.lineColor;
		}
		return shown;
	};
};

// What can be changed about a style where it is edited. The colours always can,
// as they are what every marker has; the rest is the caller's to offer.
struct MapIconOptions {
	std::vector<MapIcon::Icon> icons;	// the icons offered, in order; none for a fixed icon
	bool size;
	bool line;

	MapIconOptions() : size(false), line(false) {};
	MapIconOptions(std::vector<MapIcon::Icon> icons, bool size, bool line) :
		icons(icons), size(size), line(line) {};
};
