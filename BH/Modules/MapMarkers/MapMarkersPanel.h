#pragma once
#include <string>
#include <vector>
#include "../Window/UIPanel.h"
#include "../../Config.h"
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

		// The name cut to the row, and the width it was cut for.
		std::string fitted;
		unsigned int fittedWidth;
	public:
		MapMarkerRowhook(Drawing::HookGroup* group, unsigned int x, unsigned int y,
			MapIconStyle* style, std::string label, unsigned int index,
			unsigned int* selected);

		// How wide the name is with the marker in front of it, which is what the
		// list is sized to.
		unsigned int GetNaturalXSize();

		bool OnLeftClick(bool up, unsigned int x, unsigned int y);
		void OnDraw();

		unsigned int GetXSize() { return xSize; };
		void SetXSize(unsigned int width) { xSize = width; };
		unsigned int GetYSize();
};

// How the markers on the automap are drawn, one marker at a time: the list of
// them down the left, a rule, and the one picked edited on the right.
//
// Built from what the modules register under Settings::Category::MapMarkers
// rather than from anything it knows itself, so it can stand in any window. A
// map icon setting is a marker; any other setting whose parent is a marker is
// shown with it, under its editor.
//
// The list is as wide as its names; the editor takes the rest, up to a width
// past which a form only gets harder to read. What the two need between them is
// the smallest the window can be made.
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

			// The setting the marker hangs off, where that is one that can be
			// switched off, and what it is called.
			bool* parentBool;
			Toggle* parentToggle;
			std::string parentLabel;
		};

		std::vector<Marker> markers;
		unsigned int selected;
		unsigned int builtVersion;	// registry version the markers were built for

		Drawing::Boxhook* divider;

		// Measured when the markers are built: the list's width, the label column
		// the editors and what is attached to them share, and the least the
		// editor column and the whole panel can be.
		unsigned int listWidth;
		unsigned int labelWidth;
		unsigned int detailMinWidth;
		unsigned int minWidth, minHeight;

		// Adds a marker for every map icon registered since the last time, and
		// what is attached to each. Done as it draws rather than when built, since
		// modules register as they load and this may be built before they have.
		void EnsureMarkers();
		void FindParent(Marker& marker, const std::string& parentKey);
		void Measure();
		void Layout();

		// The help broken to a width, no more lines than there are hooks for.
		std::vector<std::string> HelpLines(const Marker& marker, unsigned int width);
		unsigned int DetailHeight(const Marker& marker, unsigned int width);

		void Select(int index);

	public:
		MapMarkersPanel(Drawing::UI* ui);

		void OnDraw();
		bool OnKey(bool up, BYTE key);
		std::string GetStatus();

		unsigned int GetMinWidth() { return minWidth; };
		unsigned int GetMinHeight() { return minHeight; };
};
