#pragma once
#include <string>
#include "Basic/Texthook/Texthook.h"

namespace Drawing {
	// The look of a heading that folds away what is under it, shared by every
	// panel that draws one: the lists in the info and accounts windows and the
	// sections of the settings window. One of these rather than a set per panel,
	// so a heading reads the same wherever the user meets it.
	//
	// A heading is drawn as three parts: the fold marker, the label, and - only
	// while it is folded - how many things it is hiding. The marker and the count
	// keep the dim colour throughout, so they read as furniture rather than as the
	// first and last characters of the heading; the label alone carries the
	// heading's colour and lifts under the mouse.
	#define UI_GROUP_FOLDED			"+"
	#define UI_GROUP_UNFOLDED		"-"
	#define UI_GROUP_MARKER_GAP		3	// marker column to label
	#define UI_GROUP_COUNT_GAP		4	// label to the count after it
	#define UI_GROUP_COLOR			Gold
	#define UI_GROUP_HOVER_COLOR	White
	#define UI_GROUP_DIM_COLOR		Grey

	// How far what is under a heading sits in from the heading's own text.
	#define UI_GROUP_INDENT			8

	// The band drawn behind the heading the keyboard is on, which is what tells it
	// apart from whichever heading happens to be under the mouse: both are lit the
	// same way, so the band is the difference.
	#define UI_GROUP_FOCUS_BAND		BTOneHalf

	// The marker gets a column of its own rather than being pasted onto the front
	// of the label: the two markers are not the same width in the game's fonts, so
	// pasting them on shifted the whole heading sideways on every fold. The column
	// holds the wider of them at whatever font is asked for.
	inline unsigned int GroupMarkerColumn(unsigned int font) {
		unsigned int folded = (unsigned int)Texthook::GetTextSize(UI_GROUP_FOLDED, font).x;
		unsigned int unfolded = (unsigned int)Texthook::GetTextSize(UI_GROUP_UNFOLDED, font).x;
		return (folded > unfolded) ? folded : unfolded;
	}

	// Where a marker sits within that column, which is centred, so both states are
	// drawn in the same place.
	inline unsigned int GroupMarkerOffset(unsigned int markerWidth, unsigned int column) {
		return (column > markerWidth) ? ((column - markerWidth) / 2) : 0;
	}

	// Where the label starts, past the column the marker is drawn in.
	inline unsigned int GroupLabelOffset(unsigned int column) {
		return column + UI_GROUP_MARKER_GAP;
	}

	// What a folded heading says it is hiding, and nothing at all where there is
	// nothing to count: a heading over no countable rows would otherwise read [0].
	inline std::string GroupCountText(unsigned int count) {
		return (count > 0) ? ("[" + std::to_string(count) + "]") : std::string();
	}
};
