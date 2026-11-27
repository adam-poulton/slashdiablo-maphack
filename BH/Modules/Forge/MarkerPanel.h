#pragma once
#include <string>
#include <vector>
#include "MarkerList.h"
#include "../Window/UIPanel.h"
#include "../../Config.h"
#include "../../Drawing/Advanced/MapIconhook/MapIconhook.h"

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
//
// A tab with more to list than the registry's markers is a panel derived from
// this one. It gives rows of its own after the markers, under ids from
// ExtraIdBase up, and lays out whatever edits the one of them that is picked.
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
			MapIconOptions options;
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
		unsigned int builtVersion;	// registry version the markers were built for

		Drawing::Boxhook* divider;

		// Not in the tab: a tip sits wherever there is room for it, which may be
		// outside the window.
		Drawing::Tooltiphook* tip;
		int shownTip;				// the row it was last filled for, or -1

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
		void PushRows();
		void Layout();

		void UpdateTip();
		void HideTip();

	protected:
		// A derived panel's own rows are numbered from here, clear of the
		// markers, which are numbered as they were registered.
		static const int ExtraIdBase = 1000;

		MarkerListhook* list;

		unsigned int MarkerCount() const { return (unsigned int)markers.size(); };
		const MapIconStyle& MarkerStyle(unsigned int index) const { return *markers[index].style; };
		const MapIconOptions& MarkerOptions(unsigned int index) const { return markers[index].options; };

		// Why the markers are not on the map whatever they are set to: the
		// setting they hang off is off. Empty while it is on.
		std::string ParentOffStatus() const;

		// Works out the sizes again, for a derived panel whose own needs have
		// changed.
		void Measure();

		// What a derived panel adds. All but Sync() and AppendRows() are asked
		// only once the markers are built.
		//
		// Sync() is called every frame before anything is laid out, for bringing
		// what the panel edits and what it shows into step.
		virtual void Sync() {};

		// The caption over the markers, for a tab that lists more than them.
		virtual std::string MarkersCaption() { return std::string(); };

		// Its rows, after the markers.
		virtual void AppendRows(std::vector<MarkerListRow>& rows) {};

		// Widens what was measured to what its own controls need: the list, the
		// label column it shares with the editors, the least the editor column
		// can be, the tallest the editor column gets, and the least the list can
		// be shown in, which is all of its rows unless the panel says less.
		virtual void MeasureExtra(unsigned int& listWidth, unsigned int& labelWidth,
			unsigned int& detailMinWidth, unsigned int& detailHeight,
			unsigned int& listMinHeight) {};

		// Room kept under the list for the panel's own controls there.
		virtual unsigned int ListFooterHeight() { return 0; };

		// Places its controls, and puts away the ones the row picked has no use
		// for. footerY is where the room kept under the list starts.
		virtual void LayoutExtra(int selectedId, unsigned int listX, unsigned int listWidth,
			unsigned int footerY, unsigned int detailX, unsigned int detailY,
			unsigned int detailWidth, unsigned int labelWidth) {};

		// Whether something of the panel's is open over the tab, which tips and
		// the list's keys keep out from under.
		virtual bool OverlayOpen() { return false; };

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
