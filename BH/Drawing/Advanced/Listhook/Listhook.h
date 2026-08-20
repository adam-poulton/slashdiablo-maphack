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

			const std::vector<ListColumn>& GetColumns() { return columns; };
			void SetColumns(const std::vector<ListColumn>& newColumns);

			unsigned int GetRowCount() { return rows.size(); };
			void SetRows(const std::vector<std::vector<std::string>>& newRows);

			// Height of one row, and how many of them fit below the header.
			unsigned int GetRowHeight();
			unsigned int GetVisibleRows();

			// Paging. Pages are zero based; ChangePage() wraps at both ends.
			unsigned int GetPage() { return page; };
			unsigned int GetPageCount();
			unsigned int GetFirstVisibleRow() { return page * GetVisibleRows(); };
			unsigned int GetLastVisibleRow();
			void SetPage(unsigned int newPage);
			void ChangePage(int delta);

			void OnDraw();
	};
};
