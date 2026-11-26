#pragma once
#include <string>
#include <vector>
#include "MapIcon.h"

class Config;

// How one kind of marker is drawn on the automap: the icon at the marker, and a
// line to it from the player.
//
// The line keeps its colour while it is off, so turning it back on does not lose
// the colour that was chosen for it.
struct MapIconStyle {
	unsigned int icon;		// a MapIcon::Icon, held as the number the config reads into
	unsigned int size;
	unsigned int color;
	bool line;
	unsigned int lineColor;

	MapIconStyle() : icon(MapIcon::Square), size(1), color(0xFF), line(false),
		lineColor(0xFF) {};
	MapIconStyle(MapIcon::Icon icon, unsigned int size, unsigned int color,
		bool line, unsigned int lineColor) : icon(icon), size(size), color(color),
		line(line), lineColor(lineColor) {};

	MapIcon::Icon Icon() const {
		return icon < MapIcon::IconCount ? (MapIcon::Icon)icon : MapIcon::None;
	};

	// Whether the style puts anything on the map at all.
	bool Draws() const { return Icon() != MapIcon::None || line; };

	bool operator==(const MapIconStyle& other) const {
		return icon == other.icon && size == other.size && color == other.color &&
			line == other.line && lineColor == other.lineColor;
	};
	bool operator!=(const MapIconStyle& other) const { return !(*this == other); };
};

// What can be changed about a style where it is edited. The colour always can,
// as it is what every marker has; the rest is the caller's to offer.
struct MapIconOptions {
	std::vector<MapIcon::Icon> icons;	// the icons offered, in order; none for a fixed icon
	bool size;
	bool line;

	MapIconOptions() : size(false), line(false) {};
	MapIconOptions(std::vector<MapIcon::Icon> icons, bool size, bool line) :
		icons(icons), size(size), line(line) {};
};

// Reads a style from the config under five keys that start with the prefix:
// Icon, Size, Color, Line and Line Color. A key that is not there is the default.
// Defined in MapIconStyle.cpp, which needs the config.
void ReadMapIconStyle(Config* config, const std::string& prefix,
	MapIconStyle& style, const MapIconStyle& defaults);
