#pragma once

#include <string>
#include <vector>
#include "../../Hook.h"
#include "../../Basic/Texthook/Texthook.h"

namespace Drawing {
	// One column of a Listhook, described by how it should share the list's width
	// rather than by where it lands in pixels. The list works the pixels out from
	// its own width, so a list keeps its proportions when the panel it sits in is
	// resized and no caller has to restate a layout it cannot see.
	//
	// Columns are laid out left to right. Each takes gap pixels of clear space,
	// then minWidth, and then a share of whatever room is left over in proportion
	// to weight. A column with no weight stays at minWidth however wide the list
	// gets; a column with weight and no minWidth is purely proportional. Text
	// that does not fit the width a column ends up with is cut with an ellipsis.
	//
	// An empty header leaves the column unlabelled, and a header on no column at
	// all leaves the header row out entirely.
	//
	// hoverColor left as Disabled means the column doesn't react to the mouse;
	// set it to make the column read as something you can click. It is also the
	// colour the column takes on the selected row, so the selection and the
	// mouse lift a row the same way.
	struct ListColumn {
		std::string header;
		unsigned int minWidth;
		unsigned int weight;
		unsigned int gap;
		TextColor color;
		TextColor hoverColor;

		ListColumn() : minWidth(0), weight(0), gap(0), color(White),
			hoverColor(Disabled) {};
		ListColumn(std::string header, unsigned int minWidth, unsigned int weight,
				unsigned int gap, TextColor color, TextColor hoverColor = Disabled) :
			header(header), minWidth(minWidth), weight(weight), gap(gap),
			color(color), hoverColor(hoverColor) {};
	};

	// A scrollable, column oriented list of text rows. The caller supplies the
	// column proportions and the rows; the list works out the column pixels, how
	// many rows fit, cuts cells that are too wide for their column, draws the
	// header, and draws a scrollbar in its right hand gutter once there is more
	// than one screenful.
	//
	// Nothing about the list is fixed to a particular size. Everything that
	// depends on how big it is - column widths, how many rows are visible, the
	// scrollbar, how far it can scroll - is derived from xSize and ySize at the
	// point it is asked for, so resizing is just SetSize() and the rest follows.
	//
	// Columns are laid out inside GetContentWidth() rather than the full width,
	// so the gutter is left clear whether or not the scrollbar is showing and
	// the rows don't shift about as the list is filtered.
	//
	// SetColumns()/SetRows()/SetFont()/SetSize() measure text with the game's
	// font routines, so call them from the draw thread (a module's OnDraw).
	class Listhook : public Hook {
		private:
			// Where a column ended up once its share of the width was resolved.
			// Parallel to columns.
			struct ColumnLayout {
				unsigned int x;
				unsigned int width;

				ColumnLayout() : x(0), width(0) {};
				ColumnLayout(unsigned int x, unsigned int width) : x(x), width(width) {};
			};

			std::vector<ListColumn> columns;				// as supplied
			std::vector<ColumnLayout> layout;				// resolved to pixels
			std::vector<std::vector<std::string>> rows;		// as supplied
			std::vector<std::vector<std::string>> fitted;	// cut to column widths
			unsigned int xSize, ySize;
			unsigned int font;
			unsigned int scrollTop;	// row drawn at the top of the list
			TextColor headerColor;
			int selectedRow;	// -1 when nothing is selected
			bool draggingThumb;	// scrollbar thumb held by the mouse
			int thumbGrabOffset;	// where in the thumb it was grabbed, in pixels

			// Resolves the column proportions against the current width, then
			// refits the cells to what came out. Assumes the lock is held.
			void Layout();
			void FitRows();
			std::string FitCell(const std::string& text, unsigned int width);

			// Pulls the view back when it starts past the last full screenful,
			// which anything changing how many rows are visible can cause.
			// Assumes the lock is held.
			void ClampScroll();

			// Nudges the view just far enough for the selection to be on screen.
			// Assumes the lock is already held.
			void ScrollSelectionIntoView();

			// Scrollbar geometry, shared by drawing, hit testing and dragging so
			// the bar the user grabs is exactly the one they can see.
			unsigned int ScrollTrackTop() { return GetY() + GetHeaderHeight(); };
			unsigned int ScrollTrackHeight() { return GetVisibleRows() * GetRowHeight(); };
			unsigned int ScrollThumbHeight();
			unsigned int ScrollThumbTop();
			bool InScrollbar(unsigned int x, unsigned int y);
			void DragThumbTo(unsigned int mouseY);

		public:
			Listhook(HookVisibility visibility, unsigned int x, unsigned int y, unsigned int xSize, unsigned int ySize);
			Listhook(HookGroup* group, unsigned int x, unsigned int y, unsigned int xSize, unsigned int ySize);

			// Resizing relays the columns out and brings the view back into
			// range. Prefer SetSize() when both change, so the list is only
			// measured once.
			unsigned int GetXSize() { return xSize; };
			void SetXSize(unsigned int newXSize) { SetSize(newXSize, ySize); };

			unsigned int GetYSize() { return ySize; };
			void SetYSize(unsigned int newYSize) { SetSize(xSize, newYSize); };

			void SetSize(unsigned int newXSize, unsigned int newYSize);

			unsigned int GetFont() { return font; };
			void SetFont(unsigned int newFont);

			TextColor GetHeaderColor() { return headerColor; };
			void SetHeaderColor(TextColor newColor) { Lock(); headerColor = newColor; Unlock(); };

			// Selection. Rows are indexed as supplied to SetRows(), which clears
			// the selection because the rows no longer mean the same thing.
			// Selecting a row scrolls it into view.
			//
			// The mouse can let go of a selection as well as make one: clicking
			// the selected row again, or the empty space past the last row,
			// clears it. So whatever a caller hangs off the selection can always
			// be dismissed from the list itself.
			int GetSelectedRow() { return selectedRow; };
			void SetSelectedRow(int row);
			void ClearSelection() { SetSelectedRow(-1); };

			// Moves the selection by whole rows, as the arrow keys do, stopping
			// at the ends rather than wrapping. With nothing selected it starts
			// from the top of the visible rows, so the first press lands
			// somewhere the user can see.
			void MoveSelection(int delta);

			const std::vector<ListColumn>& GetColumns() { return columns; };
			void SetColumns(const std::vector<ListColumn>& newColumns);

			// Where a column actually landed once its share of the width was
			// resolved. Both are 0 for an out of range index.
			unsigned int GetColumnX(unsigned int column);
			unsigned int GetColumnWidth(unsigned int column);

			unsigned int GetRowCount() { return rows.size(); };
			void SetRows(const std::vector<std::vector<std::string>>& newRows);

			// Height of one row, how much room the header takes, and how many
			// rows fit below it.
			unsigned int GetRowHeight();
			unsigned int GetHeaderHeight();
			unsigned int GetVisibleRows();

			// Width the columns have to lay out in, and the gutter kept clear on
			// the right for the scrollbar.
			unsigned int GetGutterWidth();
			unsigned int GetContentWidth();

			// Scrolling. The view is a window of GetVisibleRows() rows starting
			// at the top row; Scroll() stops at either end rather than wrapping.
			unsigned int GetFirstVisibleRow() { return scrollTop; };
			unsigned int GetLastVisibleRow();
			unsigned int GetMaxScrollTop();
			void SetScrollTop(unsigned int row);
			void Scroll(int rowDelta);
			bool CanScrollUp() { return scrollTop > 0; };
			bool CanScrollDown() { return scrollTop < GetMaxScrollTop(); };

			// The row under the mouse, or -1 if the mouse is elsewhere.
			int GetHoveredRow();

			// True while the scrollbar thumb is being dragged, in which case the
			// list is following the mouse and clicks belong to the scrollbar.
			bool IsScrolling() { return draggingThumb; };

			bool OnLeftClick(bool up, unsigned int x, unsigned int y);
			bool OnMouseWheel(int notches, unsigned int x, unsigned int y);
			void OnDraw();
	};
};
