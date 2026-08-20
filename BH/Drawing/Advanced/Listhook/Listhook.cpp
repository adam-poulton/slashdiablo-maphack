#include "Listhook.h"

using namespace std;
using namespace Drawing;

#define LIST_ROW_PADDING	2
#define LIST_HEADER_GAP		4
#define LIST_ELLIPSIS		".."

static unsigned int FontHeight(unsigned int font) {
	unsigned int height[] = {10,11,18,24,10,13,7,13,10,12,8,8,7,12};
	return height[font];
}

Listhook::Listhook(HookVisibility visibility, unsigned int x, unsigned int y, unsigned int xSize, unsigned int ySize) :
Hook(visibility, x, y), xSize(xSize), ySize(ySize), font(0), page(0), headerColor(Gold) {
}

Listhook::Listhook(HookGroup* group, unsigned int x, unsigned int y, unsigned int xSize, unsigned int ySize) :
Hook(group, x, y), xSize(xSize), ySize(ySize), font(0), page(0), headerColor(Gold) {
}

void Listhook::SetFont(unsigned int newFont) {
	if (newFont < 14) {
		Lock();
		font = newFont;
		FitRows();
		Unlock();
	}
}

void Listhook::SetColumns(const std::vector<ListColumn>& newColumns) {
	Lock();
	columns = newColumns;
	FitRows();
	Unlock();
}

void Listhook::SetRows(const std::vector<std::vector<std::string>>& newRows) {
	Lock();
	rows = newRows;
	FitRows();
	if (page >= GetPageCount())
		page = GetPageCount() - 1;
	Unlock();
}

// Cut text down until it fits the column, ending in an ellipsis. Starts from a
// proportional guess so wide cells don't need a measurement per character.
std::string Listhook::FitCell(const std::string& text, unsigned int width) {
	if (text.length() == 0 || width == 0)
		return "";

	unsigned int full = Texthook::GetTextSize(text, font).x;
	if (full <= width)
		return text;

	size_t length = (size_t)(((double)text.length() * width) / full) + 1;
	if (length > text.length())
		length = text.length();
	while (length > 0) {
		std::string candidate = text.substr(0, length) + LIST_ELLIPSIS;
		if ((unsigned int)Texthook::GetTextSize(candidate, font).x <= width)
			return candidate;
		length--;
	}
	return "";
}

void Listhook::FitRows() {
	fitted.clear();
	fitted.reserve(rows.size());
	for (unsigned int r = 0; r < rows.size(); r++) {
		std::vector<std::string> cells;
		for (unsigned int c = 0; c < columns.size() && c < rows[r].size(); c++)
			cells.push_back(FitCell(rows[r][c], columns[c].width));
		fitted.push_back(cells);
	}
}

unsigned int Listhook::GetRowHeight() {
	return FontHeight(font) + LIST_ROW_PADDING;
}

unsigned int Listhook::GetVisibleRows() {
	unsigned int headerHeight = 0;
	for (unsigned int c = 0; c < columns.size(); c++) {
		if (columns[c].header.length() > 0) {
			headerHeight = FontHeight(font) + LIST_HEADER_GAP;
			break;
		}
	}
	if (ySize <= headerHeight)
		return 0;
	return (ySize - headerHeight) / GetRowHeight();
}

unsigned int Listhook::GetPageCount() {
	unsigned int visible = GetVisibleRows();
	if (visible == 0 || rows.size() <= visible)
		return 1;
	return ((rows.size() - 1) / visible) + 1;
}

unsigned int Listhook::GetLastVisibleRow() {
	unsigned int last = GetFirstVisibleRow() + GetVisibleRows();
	if (last > rows.size())
		last = rows.size();
	return last;
}

void Listhook::SetPage(unsigned int newPage) {
	Lock();
	page = (newPage >= GetPageCount()) ? GetPageCount() - 1 : newPage;
	Unlock();
}

void Listhook::ChangePage(int delta) {
	unsigned int pages = GetPageCount();
	Lock();
	if (delta < 0 && page == 0)
		page = pages - 1;
	else if (delta > 0 && page + 1 >= pages)
		page = 0;
	else
		page += delta;
	Unlock();
}

void Listhook::OnDraw() {
	if (!IsActive())
		return;

	Lock();
	unsigned int rowHeight = GetRowHeight();
	unsigned int y = GetY();

	bool hasHeader = false;
	for (unsigned int c = 0; c < columns.size(); c++) {
		if (columns[c].header.length() == 0)
			continue;
		hasHeader = true;
		Texthook::Draw(GetX() + columns[c].x, y, None, font, headerColor, "%s", columns[c].header.c_str());
	}
	if (hasHeader)
		y += FontHeight(font) + LIST_HEADER_GAP;

	unsigned int first = GetFirstVisibleRow(), last = GetLastVisibleRow();
	for (unsigned int r = first; r < last && r < fitted.size(); r++, y += rowHeight) {
		for (unsigned int c = 0; c < fitted[r].size(); c++) {
			if (fitted[r][c].length() == 0)
				continue;
			Texthook::Draw(GetX() + columns[c].x, y, None, font, columns[c].color, "%s", fitted[r][c].c_str());
		}
	}
	Unlock();
}
