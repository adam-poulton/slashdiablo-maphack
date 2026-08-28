#pragma once

#include <vector>
#include "../../Hook.h"

namespace Drawing {
	class Scrollhook;

	// The origin the contents of a scrolling box measure themselves from: the top
	// left of the box, less however far it has been scrolled. Scrolling is
	// therefore only a matter of the group reporting a different origin, and
	// nothing inside the box ever has to be moved.
	//
	// A group of its own rather than the box itself, because a Hook and a
	// HookGroup both answer to GetX and GetY and a class that is both cannot say
	// which of the two a caller meant.
	class ScrollContent : public HookGroup {
		private:
			Scrollhook* box;
		public:
			ScrollContent(Scrollhook* owner) : box(owner) {};

			unsigned int GetX();
			unsigned int GetY();
			unsigned int GetXSize();
			unsigned int GetYSize();
			bool IsActive();
	};

	// A vertically scrolling box that other hooks live inside, for contents taller
	// than the room there is for them.
	//
	// Contents are described as rows, and a row is the unit of scrolling: the box
	// shows whole rows only. That is what makes it simple - there is no clipping,
	// because nothing is ever partly outside the box, and a row half over the edge
	// can never be half clickable. Rows may be of different heights, which a row
	// carrying wrapped text needs.
	//
	// Whoever builds the contents decides what goes in which row; the box only
	// needs telling, so that it knows what to show and what to switch off. Note
	// that the box drives the active flag of everything in it, so a caller cannot
	// also use SetActive() on those hooks for its own purposes.
	class Scrollhook : public Hook {
		private:
			// One row: how tall it is, where it starts in content space, and what
			// is in it.
			struct Row {
				unsigned int height;
				unsigned int y;
				std::vector<Hook*> hooks;
				Row() : height(0), y(0) {};
			};

			unsigned int xSize, ySize;
			std::vector<Row> rows;

			// Everything ever added to a row. Rows are forgotten and rebuilt as
			// the contents are laid out again, so this is what the box has to
			// destroy, rather than whatever happened to be in a row at the end.
			std::vector<Hook*> owned;

			unsigned int scrollRow;	// first row shown
			bool draggingThumb;
			int thumbGrabOffset;
			ScrollContent* content;

			void ClampScroll();
			void ApplyRowVisibility();

			// The scrollbar runs the full height of the box. Assume the lock is
			// held where they are called from drawing.
			unsigned int ScrollTrackTop() { return GetY(); };
			unsigned int ScrollTrackHeight() { return ySize; };
			unsigned int ThumbHeight();
			unsigned int ThumbTop();
			bool InScrollbar(unsigned int x, unsigned int y);
			void DragThumbTo(unsigned int mouseY);
		public:
			Scrollhook(HookVisibility visibility, unsigned int x, unsigned int y,
				unsigned int xSize, unsigned int ySize);
			Scrollhook(HookGroup* group, unsigned int x, unsigned int y,
				unsigned int xSize, unsigned int ySize);
			~Scrollhook();

			// The group the contents are built against.
			HookGroup* GetContent();

			unsigned int GetXSize() { return xSize; };
			void SetXSize(unsigned int newXSize) { SetSize(newXSize, ySize); };
			unsigned int GetYSize() { return ySize; };
			void SetYSize(unsigned int newYSize) { SetSize(xSize, newYSize); };
			void SetSize(unsigned int newXSize, unsigned int newYSize);

			// Describing the contents. ClearRows() forgets the rows without
			// destroying what was in them, so contents can be laid out again and
			// again against the same hooks.
			void ClearRows();

			// Destroys everything the box has been given, not just the rows it is
			// in. For contents that are not merely being laid out again but
			// replaced: a hook left over from the last set would be drawn by
			// nothing and still be clicked where it used to be.
			void ClearContents();
			unsigned int AddRow(unsigned int height);
			void AddToRow(unsigned int row, Hook* hook);

			unsigned int GetRowCount() { return (unsigned int)rows.size(); };
			unsigned int GetRowY(unsigned int row);
			unsigned int GetRowHeight(unsigned int row);
			unsigned int GetContentHeight();

			// Width the contents have to lay out in, and the gutter kept clear on
			// the right for the scrollbar. The gutter is held clear whether or not
			// the bar is showing, so contents do not shift as rows are added.
			unsigned int GetGutterWidth();
			unsigned int GetContentWidth();

			// Scrolling, counted in rows.
			unsigned int GetFirstVisibleRow() { return scrollRow; };
			unsigned int GetVisibleRowCount();
			unsigned int GetMaxScrollRow();
			void SetScrollRow(unsigned int row);
			void Scroll(int rowDelta);

			// Nudges the view just far enough for a row to be shown whole.
			void ScrollRowIntoView(unsigned int row);

			bool CanScroll() { return GetMaxScrollRow() > 0; };

			// True while the thumb is held, in which case the box is following the
			// mouse and clicks belong to the scrollbar.
			bool IsScrolling() { return draggingThumb; };

			bool OnLeftClick(bool up, unsigned int x, unsigned int y);
			bool OnMouseWheel(int notches, unsigned int x, unsigned int y);
			void OnDraw();
	};
};
