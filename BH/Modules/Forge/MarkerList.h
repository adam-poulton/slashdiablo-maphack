#pragma once
#include <string>
#include <vector>
#include "../../Drawing/Hook.h"
#include "../../Drawing/MapIcon/MapIconStyle.h"

// Above a tab's first row and below whatever is lowest in it, clear of the
// footer's rule.
#define MM_TOP				8
#define MM_BOTTOM			8
#define MM_TEXT_HEIGHT		10		// font 0

// Tall enough for the game's sprite with room above and below it, so a marker
// drawn as it stays clear of the rows on either side. The name is centred on it.
#define MM_ROW_HEIGHT		28

// A caption over the rows that follow it, which only names them: shorter than a
// row, with its text low in it so it sits with what it names.
#define MM_CAPTION_HEIGHT	20
#define MM_CAPTION_TEXT_TOP	7

// One row of the marker list.
struct MarkerListRow {
	// A caption is not a marker: it cannot be picked, and has no id.
	bool caption;

	// The panel's own number for the marker, which is what the list says is
	// picked. It outlives the rows being given again.
	int id;

	std::string label;
	std::string help;		// shown over the row while it is pointed at
	MapIconStyle style;		// the marker, as the row draws it ahead of its name

	MarkerListRow() : caption(false), id(-1) {};

	static MarkerListRow Caption(const std::string& label, const std::string& help = "") {
		MarkerListRow row;
		row.caption = true;
		row.label = label;
		row.help = help;
		return row;
	};

	static MarkerListRow Marker(int id, const std::string& label, const MapIconStyle& style,
			const std::string& help = "") {
		MarkerListRow row;
		row.id = id;
		row.label = label;
		row.style = style;
		row.help = help;
		return row;
	};

	bool operator==(const MarkerListRow& other) const {
		return caption == other.caption && id == other.id && label == other.label &&
			help == other.help && style == other.style;
	};
};

// The list down the left of a Forge tab: each marker as it is drawn and its name,
// under captions where the tab has more than one kind of them. A click makes a
// marker the one being edited, and the one being edited stands on a band.
//
// The rows are given whole, as often as every frame, and only measured again
// when they differ. A list with more rows than room scrolls by whole rows, with
// the bar every scrolling thing here draws.
class MarkerListhook : public Drawing::Hook {
	private:
		std::vector<MarkerListRow> rows;

		// Each name cut to the width the list was last drawn at.
		std::vector<std::string> fitted;
		unsigned int fittedWidth;

		unsigned int xSize, ySize;
		unsigned int first;			// the row drawn at the top
		int selected;				// the id picked, or -1
		bool reveal;				// bring the picked row into view when next drawn
		bool dragging;
		int grabOffset;

		static unsigned int RowHeight(const MarkerListRow& row);

		// All of these assume the lock is held.
		unsigned int MaxFirst() const;
		unsigned int VisibleCount() const;
		bool Scrolls() const { return MaxFirst() > 0; };
		unsigned int RowWidth() const;
		int IndexOf(int id) const;
		int IndexAt(unsigned int x, unsigned int y);
		unsigned int RowTop(unsigned int index);
		void Reveal();
		unsigned int ThumbHeight();
		unsigned int ThumbTop();
		bool InBar(unsigned int x, unsigned int y);
		void DrawRow(unsigned int index, unsigned int y, unsigned int width, bool hovered);

	public:
		MarkerListhook(Drawing::HookGroup* group, unsigned int x, unsigned int y);

		void SetRows(const std::vector<MarkerListRow>& newRows);

		unsigned int GetXSize() { return xSize; };
		unsigned int GetYSize() { return ySize; };
		void SetSize(unsigned int width, unsigned int height);

		// How wide the widest row is with its marker ahead of its name, and how
		// tall every row is together: what the list would be with room for all
		// of it.
		unsigned int GetNaturalXSize();
		unsigned int GetNaturalYSize();

		// The marker picked, by id, -1 for none. Picking one brings it into view,
		// whether its row has been given yet or is given next.
		int GetSelected();
		void SetSelected(int id);

		// Moves the pick by whole markers, past the captions, stopping at the
		// ends. With nothing picked it starts from the first.
		void MoveSelection(int delta);
		void SelectFirst();
		void SelectLast();

		// The row under the mouse, as an index into the rows as given, captions
		// included; -1 for none. And where a row is on screen, false for one that
		// is scrolled out of view.
		int GetHoveredRow();
		bool GetRowBox(unsigned int index, unsigned int& x, unsigned int& y,
			unsigned int& width, unsigned int& height);
		const MarkerListRow* GetRow(unsigned int index);

		bool OnLeftClick(bool up, unsigned int x, unsigned int y);
		bool OnMouseWheel(int notches, unsigned int x, unsigned int y);
		void OnDraw();
};
