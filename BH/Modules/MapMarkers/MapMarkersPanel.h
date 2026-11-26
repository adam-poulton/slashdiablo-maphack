#pragma once
#include <string>
#include <vector>
#include "../Window/UIPanel.h"
#include "../../Drawing/Advanced/MapIconhook/MapIconhook.h"

// One row of the marker list: the marker as it is drawn, and its name. A click
// makes it the one being edited.
class MapMarkerRowhook : public Drawing::Hook {
	private:
		MapIconStyle* style;
		std::string label;
		unsigned int index;
		unsigned int* selected;
		unsigned int xSize;
	public:
		MapMarkerRowhook(Drawing::HookGroup* group, unsigned int x, unsigned int y,
			unsigned int xSize, MapIconStyle* style, std::string label,
			unsigned int index, unsigned int* selected);

		bool OnLeftClick(bool up, unsigned int x, unsigned int y);
		void OnDraw();

		unsigned int GetXSize() { return xSize; };
		unsigned int GetYSize();
};

// How the markers on the automap are drawn, one marker at a time: the list of
// them down the left, and the one picked edited on the right.
//
// Built from what the modules register under Settings::Category::MapMarkers
// rather than from anything it knows itself, so it can stand in any window. A
// map icon setting is a marker; any other setting whose parent is a marker is
// shown with it, under its editor.
class MapMarkersPanel : public UIPanel {
	private:
		struct Attached {
			std::string key;
			Drawing::Texthook* label;
			Drawing::Hook* control;
		};

		struct Marker {
			std::string key;
			std::string label;
			MapIconStyle* style;
			MapMarkerRowhook* row;
			Drawing::MapIconhook* editor;
			std::vector<Attached> attached;

			// What the marker is, under its editor, a line to a hook.
			std::string help;
			std::vector<Drawing::Texthook*> helpLines;
			unsigned int helpWidth;		// what the lines were last broken to
		};

		std::vector<Marker> markers;
		unsigned int selected;
		unsigned int builtVersion;	// registry version the markers were built for

		// Adds a marker for every map icon registered since the last time, and
		// what is attached to each. Done as it draws rather than when built, since
		// modules register as they load and this may be built before they have.
		void EnsureMarkers();
		void Layout();

	public:
		MapMarkersPanel(Drawing::UI* ui);

		void OnDraw();
};
