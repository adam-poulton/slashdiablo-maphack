#include "Listhook.h"
#include "../../Basic/Boxhook/Boxhook.h"
#include "../../../D2Ptrs.h"

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
Hook(visibility, x, y), xSize(xSize), ySize(ySize), font(0), page(0), headerColor(Gold),
selectedColor(Silver), selectedRow(-1) {
}

Listhook::Listhook(HookGroup* group, unsigned int x, unsigned int y, unsigned int xSize, unsigned int ySize) :
Hook(group, x, y), xSize(xSize), ySize(ySize), font(0), page(0), headerColor(Gold),
selectedColor(Silver), selectedRow(-1) {
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
	selectedRow = -1;
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

unsigned int Listhook::GetHeaderHeight() {
	for (unsigned int c = 0; c < columns.size(); c++) {
		if (columns[c].header.length() > 0)
			return FontHeight(font) + LIST_HEADER_GAP;
	}
	return 0;
}

unsigned int Listhook::GetVisibleRows() {
	unsigned int headerHeight = GetHeaderHeight();
	if (ySize <= headerHeight)
		return 0;
	return (ySize - headerHeight) / GetRowHeight();
}

void Listhook::SetSelectedRow(int row) {
	Lock();
	selectedRow = (row < 0 || row >= (int)rows.size()) ? -1 : row;
	// Bring the selection into view so it is always the row being described.
	if (selectedRow >= 0 && GetVisibleRows() > 0)
		page = (unsigned int)selectedRow / GetVisibleRows();
	Unlock();
}

int Listhook::GetHoveredRow() {
	unsigned int top = GetY() + GetHeaderHeight();
	unsigned int mouseX = (*p_D2CLIENT_MouseX), mouseY = (*p_D2CLIENT_MouseY);
	if (mouseX < GetX() || mouseX > GetX() + xSize || mouseY < top)
		return -1;
	unsigned int offset = (mouseY - top) / GetRowHeight();
	if (offset >= GetVisibleRows())
		return -1;
	unsigned int index = GetFirstVisibleRow() + offset;
	return (index < rows.size()) ? (int)index : -1;
}

// Selects the row that was clicked. Clicks below the last row fall through so
// the window can handle them.
bool Listhook::OnLeftClick(bool up, unsigned int x, unsigned int y) {
	if (!up || !IsActive())
		return false;

	unsigned int top = GetY() + GetHeaderHeight();
	unsigned int rowHeight = GetRowHeight();
	if (x < GetX() || x > GetX() + xSize || y < top)
		return false;
	unsigned int offset = (y - top) / rowHeight;
	if (offset >= GetVisibleRows())
		return false;

	unsigned int index = GetFirstVisibleRow() + offset;
	if (index >= rows.size())
		return false;

	SetSelectedRow((int)index);
	return true;
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

	int hovered = GetHoveredRow();
	unsigned int first = GetFirstVisibleRow(), last = GetLastVisibleRow();
	for (unsigned int r = first; r < last && r < fitted.size(); r++, y += rowHeight) {
		bool selected = ((int)r == selectedRow);
		if (selected)
			Boxhook::Draw(GetX() - 2, y, xSize, rowHeight, 0, BTHighlight);
		for (unsigned int c = 0; c < fitted[r].size(); c++) {
			if (fitted[r][c].length() == 0)
				continue;
			TextColor color = columns[c].color;
			if (selected)
				color = selectedColor;
			else if ((int)r == hovered && columns[c].hoverColor != Disabled)
				color = columns[c].hoverColor;
			Texthook::Draw(GetX() + columns[c].x, y, None, font, color, "%s",
				fitted[r][c].c_str());
		}
	}
	Unlock();
}
