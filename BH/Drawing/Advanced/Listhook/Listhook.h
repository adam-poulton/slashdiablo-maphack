#pragma once

#include <string>
#include <vector>
#include "../../Hook.h"
#include "../../Basic/Texthook/Texthook.h"

namespace Drawing {
	// One column of a Listhook. x and width are in pixels, relative to the
	// list's left edge; text that does not fit width is cut with an ellipsis.
	struct ListColumn {
		std::string header;
		unsigned int x;
		unsigned int width;
		TextColor color;

		ListColumn() : x(0), width(0), color(White) {};
		ListColumn(std::string header, unsigned int x, unsigned int width, TextColor color) :
			header(header), x(x), width(width), color(color) {};
	};

	// A paged, column oriented list of text rows. The caller supplies the column
	// layout and the rows; the list works out how many rows fit, cuts cells that
	// are too wide for their column and draws the header.
	//
	// SetColumns()/SetRows()/SetFont() measure text with the game's font
	// routines, so call them from the draw thread (a module's OnDraw).
	class Listhook : public Hook {
		private:
			std::vector<ListColumn> columns;
			std::vector<std::vector<std::string>> rows;		// as supplied
			std::vector<std::vector<std::string>> fitted;	// cut to column widths
			unsigned int xSize, ySize;
			unsigned int font;
			unsigned int page;
			TextColor headerColor;
			TextColor selectedColor;
			int selectedRow;	// -1 when nothing is selected

			void FitRows();
			std::string FitCell(const std::string& text, unsigned int width);

		public:
			Listhook(HookVisibility visibility, unsigned int x, unsigned int y, unsigned int xSize, unsigned int ySize);
			Listhook(HookGroup* group, unsigned int x, unsigned int y, unsigned int xSize, unsigned int ySize);

			unsigned int GetXSize() { return xSize; };
			void SetXSize(unsigned int newXSize) { Lock(); xSize = newXSize; Unlock(); };

			unsigned int GetYSize() { return ySize; };
			void SetYSize(unsigned int newYSize) { Lock(); ySize = newYSize; Unlock(); };

			unsigned int GetFont() { return font; };
			void SetFont(unsigned int newFont);

			TextColor GetHeaderColor() { return headerColor; };
			void SetHeaderColor(TextColor newColor) { Lock(); headerColor = newColor; Unlock(); };

			// Colour used for the selected row, which also gets a band drawn
			// behind it.
			TextColor GetSelectedColor() { return selectedColor; };
			void SetSelectedColor(TextColor newColor) { Lock(); selectedColor = newColor; Unlock(); };

			// Selection. Rows are indexed as supplied to SetRows(), which clears
			// the selection because the rows no longer mean the same thing.
			int GetSelectedRow() { return selectedRow; };
			void SetSelectedRow(int row);
			void ClearSelection() { SetSelectedRow(-1); };

			const std::vector<ListColumn>& GetColumns() { return columns; };
			void SetColumns(const std::vector<ListColumn>& newColumns);

			unsigned int GetRowCount() { return rows.size(); };
			void SetRows(const std::vector<std::vector<std::string>>& newRows);

			// Height of one row, how much room the header takes, and how many
			// rows fit below it.
			unsigned int GetRowHeight();
			unsigned int GetHeaderHeight();
			unsigned int GetVisibleRows();

			// Paging. Pages are zero based; ChangePage() wraps at both ends.
			unsigned int GetPage() { return page; };
			unsigned int GetPageCount();
			unsigned int GetFirstVisibleRow() { return page * GetVisibleRows(); };
			unsigned int GetLastVisibleRow();
			void SetPage(unsigned int newPage);
			void ChangePage(int delta);

			bool OnLeftClick(bool up, unsigned int x, unsigned int y);
			void OnDraw();
	};
};
