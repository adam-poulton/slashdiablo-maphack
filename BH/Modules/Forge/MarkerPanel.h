#pragma once
#include <string>
#include <vector>
#include "../Window/UIPanel.h"
#include "../../Config.h"
#include "../../Drawing/Advanced/MapIconhook/MapIconhook.h"

// One row of the marker list: the marker as it is drawn, and its name. A click
// makes it the one being edited.
class MarkerRowhook : public Drawing::Hook {
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
		MarkerRowhook(Drawing::HookGroup* group, unsigned int x, unsigned int y,
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

// How one kind of marker on the automap is drawn, one marker at a time: the list
// of them down the left, a rule, and the one picked edited on the right.
//
// Built from what the modules register under its category rather than from
// anything it knows itself, so it can stand in any window. A map icon setting is
// a marker; any other setting whose parent is a marker is shown with it, under
// its editor.
//
// The list is as wide as its names, and what each marker covers is the tip over
// its name. The editor takes the rest of the width, up to a width past which a
// form only gets harder to read. What the two need between them is the smallest
// the window can be made.
class MarkerPanel : public UIPanel {
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
			MarkerRowhook* row;
			Drawing::MapIconhook* editor;
			std::vector<Attached> attached;

			// What the marker covers, shown over its name while it is pointed at.
			std::string help;

			// The setting the marker hangs off, where that is one that can be
			// switched off, and what it is called.
			bool* parentBool;
			Toggle* parentToggle;
			std::string parentLabel;
		};

		std::string category;
		std::vector<Marker> markers;
		unsigned int selected;
		unsigned int builtVersion;	// registry version the markers were built for

		Drawing::Boxhook* divider;

		// Not in the tab: a tip sits wherever there is room for it, which may be
		// outside the window.
		Drawing::Tooltiphook* tip;
		int shownTip;				// the marker it was last filled for, or -1

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
		void UpdateTip();
		void HideTip();

		void Select(int index);

	public:
		// The tab is called name, and shows what is registered under category.
		MarkerPanel(Drawing::UI* ui, std::string name, std::string category);
		~MarkerPanel();

		void OnDraw();
		void OnClose();
		bool OnKey(bool up, BYTE key);
		std::string GetStatus();

		unsigned int GetMinWidth() { return minWidth; };
		unsigned int GetMinHeight() { return minHeight; };
};
