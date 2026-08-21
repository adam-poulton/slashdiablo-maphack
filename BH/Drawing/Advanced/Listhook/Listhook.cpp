#include "Listhook.h"
#include "../../Basic/Boxhook/Boxhook.h"
#include "../../../D2Ptrs.h"

using namespace std;
using namespace Drawing;

#define LIST_ROW_PADDING	2
#define LIST_HEADER_GAP		4
#define LIST_ELLIPSIS		".."

// The scrollbar sits in a gutter on the right, kept clear of the columns whether
// or not there is anything to scroll. LIST_WHEEL_ROWS is how far one notch of
// the wheel moves the view.
#define LIST_SCROLLBAR_WIDTH	5
#define LIST_SCROLLBAR_GAP		3
#define LIST_WHEEL_ROWS			3

// The rail is a shade of the panel behind it, so it reads as a groove rather than
// as another object; the thumb is a solid fill on top of it, inset by a pixel so
// a sliver of the rail shows either side of it. Palette indices are from the same
// set the automap markers are drawn with.
#define LIST_THUMB_COLOR		0xD0	// grey
#define LIST_THUMB_COLOR_LIT	0x20	// white
#define LIST_THUMB_INSET		1

static unsigned int FontHeight(unsigned int font) {
	unsigned int height[] = {10,11,18,24,10,13,7,13,10,12,8,8,7,12};
	return height[font];
}

Listhook::Listhook(HookVisibility visibility, unsigned int x, unsigned int y, unsigned int xSize, unsigned int ySize) :
Hook(visibility, x, y), xSize(xSize), ySize(ySize), font(0), scrollTop(0), headerColor(Gold),
selectedRow(-1), draggingThumb(false), thumbGrabOffset(0) {
}

Listhook::Listhook(HookGroup* group, unsigned int x, unsigned int y, unsigned int xSize, unsigned int ySize) :
Hook(group, x, y), xSize(xSize), ySize(ySize), font(0), scrollTop(0), headerColor(Gold),
selectedRow(-1), draggingThumb(false), thumbGrabOffset(0) {
}

void Listhook::SetSize(unsigned int newXSize, unsigned int newYSize) {
	Lock();
	xSize = newXSize;
	ySize = newYSize;
	// Width decides the column pixels, height decides how many rows are visible,
	// and both of those move the floor the view has to sit above.
	Layout();
	ClampScroll();
	Unlock();
}

void Listhook::SetFont(unsigned int newFont) {
	if (newFont < 14) {
		Lock();
		font = newFont;
		// A different font is a different row height and different text widths,
		// so both the columns and the view have to be worked out again.
		Layout();
		ClampScroll();
		Unlock();
	}
}

void Listhook::SetColumns(const std::vector<ListColumn>& newColumns) {
	Lock();
	columns = newColumns;
	Layout();
	// Column headers can come and go, which changes the room left for rows.
	ClampScroll();
	Unlock();
}

void Listhook::SetRows(const std::vector<std::vector<std::string>>& newRows) {
	Lock();
	rows = newRows;
	FitRows();
	selectedRow = -1;
	ClampScroll();
	Unlock();
}

void Listhook::ClampScroll() {
	unsigned int max = GetMaxScrollTop();
	if (scrollTop > max)
		scrollTop = max;
}

// Resolves each column's share of the list into pixels. Every column takes its
// gap and its minimum, and what is left over is handed out in proportion to the
// weights, so the columns always add up to exactly the width available and a
// resize does not leave a ragged edge or a gap.
void Listhook::Layout() {
	layout.assign(columns.size(), ColumnLayout());

	unsigned int available = GetContentWidth();
	unsigned int fixed = 0, totalWeight = 0;
	for (unsigned int c = 0; c < columns.size(); c++) {
		fixed += columns[c].gap + columns[c].minWidth;
		totalWeight += columns[c].weight;
	}

	unsigned int spare = (available > fixed) ? (available - fixed) : 0;
	unsigned int handedOut = 0, lastWeighted = columns.size();
	for (unsigned int c = 0; c < columns.size(); c++) {
		if (columns[c].weight > 0)
			lastWeighted = c;
	}

	unsigned int x = 0;
	for (unsigned int c = 0; c < columns.size(); c++) {
		x += columns[c].gap;

		unsigned int width = columns[c].minWidth;
		if (columns[c].weight > 0 && totalWeight > 0) {
			// The last weighted column takes the rounding remainder, so the row
			// finishes flush against the gutter rather than a pixel or two short.
			unsigned int share = (c == lastWeighted) ? (spare - handedOut) :
				((spare * columns[c].weight) / totalWeight);
			handedOut += share;
			width += share;
		}

		// A list too narrow to hold its minimums truncates on the right rather
		// than drawing cells out past the gutter.
		if (x >= available)
			width = 0;
		else if (x + width > available)
			width = available - x;

		layout[c] = ColumnLayout(x, width);
		x += width;
	}

	FitRows();
}

unsigned int Listhook::GetColumnX(unsigned int column) {
	return (column < layout.size()) ? layout[column].x : 0;
}

unsigned int Listhook::GetColumnWidth(unsigned int column) {
	return (column < layout.size()) ? layout[column].width : 0;
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
		for (unsigned int c = 0; c < layout.size() && c < rows[r].size(); c++)
			cells.push_back(FitCell(rows[r][c], layout[c].width));
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

// Always reserved, so the columns keep the same widths whether or not there is
// enough to scroll and the rows don't shuffle sideways as the list is filtered.
unsigned int Listhook::GetGutterWidth() {
	return LIST_SCROLLBAR_WIDTH + LIST_SCROLLBAR_GAP;
}

unsigned int Listhook::GetContentWidth() {
	unsigned int gutter = GetGutterWidth();
	return (xSize > gutter) ? (xSize - gutter) : 0;
}

void Listhook::SetSelectedRow(int row) {
	Lock();
	selectedRow = (row < 0 || row >= (int)rows.size()) ? -1 : row;
	ScrollSelectionIntoView();
	Unlock();
}

// Scrolls by as little as possible, so walking down the list with the arrow keys
// moves the view a row at a time instead of jumping the selection to the middle.
void Listhook::ScrollSelectionIntoView() {
	unsigned int visible = GetVisibleRows();
	if (selectedRow < 0 || visible == 0)
		return;
	unsigned int row = (unsigned int)selectedRow;
	if (row < scrollTop)
		scrollTop = row;
	else if (row >= scrollTop + visible)
		scrollTop = row - visible + 1;
}

void Listhook::MoveSelection(int delta) {
	if (rows.empty() || delta == 0)
		return;

	Lock();
	int target;
	if (selectedRow < 0) {
		// Nothing selected yet, so step in from the edge the user is heading
		// away from rather than from the top of the list they cannot see.
		unsigned int visible = GetVisibleRows();
		unsigned int last = (visible > 0) ? (scrollTop + visible - 1) : scrollTop;
		target = (delta > 0) ? (int)scrollTop : (int)last;
	} else {
		target = selectedRow + delta;
	}
	if (target < 0)
		target = 0;
	if (target > (int)rows.size() - 1)
		target = (int)rows.size() - 1;

	selectedRow = target;
	ScrollSelectionIntoView();
	Unlock();
}

int Listhook::GetHoveredRow() {
	unsigned int top = GetY() + GetHeaderHeight();
	unsigned int mouseX = (*p_D2CLIENT_MouseX), mouseY = (*p_D2CLIENT_MouseY);
	if (mouseX < GetX() || mouseX > GetX() + GetContentWidth() || mouseY < top)
		return -1;
	unsigned int offset = (mouseY - top) / GetRowHeight();
	if (offset >= GetVisibleRows())
		return -1;
	unsigned int index = GetFirstVisibleRow() + offset;
	return (index < rows.size()) ? (int)index : -1;
}

// Kept at least a row tall so there is always something to grab, however long
// the list gets.
unsigned int Listhook::ScrollThumbHeight() {
	unsigned int track = ScrollTrackHeight(), visible = GetVisibleRows();
	if (rows.empty() || visible == 0)
		return track;
	unsigned int height = (track * visible) / (unsigned int)rows.size();
	unsigned int minimum = GetRowHeight();
	return (height < minimum) ? minimum : height;
}

unsigned int Listhook::ScrollThumbTop() {
	unsigned int max = GetMaxScrollTop();
	if (max == 0)
		return ScrollTrackTop();
	unsigned int travel = ScrollTrackHeight() - ScrollThumbHeight();
	return ScrollTrackTop() + ((travel * scrollTop) / max);
}

bool Listhook::InScrollbar(unsigned int x, unsigned int y) {
	if (GetMaxScrollTop() == 0)
		return false;
	unsigned int left = GetX() + xSize - LIST_SCROLLBAR_WIDTH;
	unsigned int top = ScrollTrackTop();
	return x >= left && x <= left + LIST_SCROLLBAR_WIDTH &&
		y >= top && y <= top + ScrollTrackHeight();
}

// Turns a thumb position back into a top row. Works in whole pixels of travel,
// so the thumb sits under the cursor rather than snapping to row boundaries.
void Listhook::DragThumbTo(unsigned int mouseY) {
	unsigned int max = GetMaxScrollTop();
	unsigned int travel = ScrollTrackHeight() - ScrollThumbHeight();
	if (max == 0 || travel == 0)
		return;

	int top = (int)mouseY - thumbGrabOffset - (int)ScrollTrackTop();
	if (top <= 0) {
		scrollTop = 0;
	} else if ((unsigned int)top >= travel) {
		scrollTop = max;
	} else {
		// Rounded, so the halfway point of a row's worth of travel is where the
		// list actually turns over.
		scrollTop = (((unsigned int)top * max) + (travel / 2)) / travel;
	}
}

// Selects the row that was clicked, lets go of it if it was already selected, and
// lets go of the selection entirely for a click on the empty space past the last
// row. Clicks on the scrollbar move the view instead, and clicks outside the list
// are left alone.
bool Listhook::OnLeftClick(bool up, unsigned int x, unsigned int y) {
	// Let go of the thumb wherever the button comes up, since a drag usually
	// ends with the mouse somewhere off the bar.
	if (up && draggingThumb) {
		Lock();
		draggingThumb = false;
		Unlock();
		return true;
	}
	if (!IsActive())
		return false;

	if (InScrollbar(x, y)) {
		Lock();
		unsigned int thumbTop = ScrollThumbTop(), thumbHeight = ScrollThumbHeight();
		if (!up) {
			if (y >= thumbTop && y <= thumbTop + thumbHeight) {
				draggingThumb = true;
				thumbGrabOffset = (int)(y - thumbTop);
			} else {
				// Clicking the empty track jumps a screenful towards the click,
				// then follows the mouse from the middle of the thumb so the
				// drag carries on from there.
				unsigned int visible = GetVisibleRows();
				int page = (visible > 1) ? (int)(visible - 1) : 1;
				Scroll((y < thumbTop) ? -page : page);
				draggingThumb = true;
				thumbGrabOffset = (int)(ScrollThumbHeight() / 2);
			}
		}
		Unlock();
		return true;
	}

	if (!up)
		return false;

	unsigned int top = GetY() + GetHeaderHeight();
	unsigned int rowHeight = GetRowHeight();
	if (x < GetX() || x > GetX() + xSize || y < top || y > GetY() + ySize)
		return false;

	unsigned int offset = (y - top) / rowHeight;
	unsigned int index = GetFirstVisibleRow() + offset;
	if (offset >= GetVisibleRows() || index >= rows.size()) {
		// The empty space past the last row is still part of the list, and
		// clicking it lets go of the selection. That is the usual way to dismiss
		// whatever the selection was putting on screen.
		ClearSelection();
		return true;
	}

	// Clicking the selected row again lets go of it, so a selection can be
	// dropped without leaving the list or having to clear the search.
	SetSelectedRow((selectedRow == (int)index) ? -1 : (int)index);
	return true;
}

// The wheel scrolls the list under the mouse. The whole list area answers to it,
// including the header and any blank space below the last row, so the target
// doesn't shrink as the list is filtered down.
bool Listhook::OnMouseWheel(int notches, unsigned int x, unsigned int y) {
	if (!IsActive() || GetMaxScrollTop() == 0)
		return false;
	if (x < GetX() || x > GetX() + xSize || y < GetY() || y > GetY() + ySize)
		return false;

	Scroll(-notches * LIST_WHEEL_ROWS);
	return true;
}

unsigned int Listhook::GetLastVisibleRow() {
	unsigned int last = scrollTop + GetVisibleRows();
	if (last > rows.size())
		last = rows.size();
	return last;
}

unsigned int Listhook::GetMaxScrollTop() {
	unsigned int visible = GetVisibleRows();
	if (visible == 0 || rows.size() <= visible)
		return 0;
	return rows.size() - visible;
}

void Listhook::SetScrollTop(unsigned int row) {
	Lock();
	unsigned int max = GetMaxScrollTop();
	scrollTop = (row > max) ? max : row;
	Unlock();
}

void Listhook::Scroll(int rowDelta) {
	Lock();
	unsigned int max = GetMaxScrollTop();
	if (rowDelta < 0) {
		unsigned int back = (unsigned int)(-rowDelta);
		scrollTop = (back > scrollTop) ? 0 : (scrollTop - back);
	} else {
		scrollTop += (unsigned int)rowDelta;
		if (scrollTop > max)
			scrollTop = max;
	}
	Unlock();
}

void Listhook::OnDraw() {
	if (!IsActive()) {
		// A list that has stopped being drawn can't be being dragged either.
		draggingThumb = false;
		return;
	}

	Lock();
	unsigned int rowHeight = GetRowHeight();
	// There is no mouse move event to hang a drag off, so the thumb catches up
	// with the cursor here, once per frame.
	if (draggingThumb)
		DragThumbTo((*p_D2CLIENT_MouseY));
	unsigned int contentWidth = GetContentWidth();
	unsigned int y = GetY();

	bool hasHeader = false;
	for (unsigned int c = 0; c < columns.size(); c++) {
		if (columns[c].header.length() == 0)
			continue;
		hasHeader = true;
		Texthook::Draw(GetX() + layout[c].x, y, None, font, headerColor, "%s", columns[c].header.c_str());
	}
	if (hasHeader)
		y += FontHeight(font) + LIST_HEADER_GAP;

	int hovered = GetHoveredRow();
	unsigned int first = GetFirstVisibleRow(), last = GetLastVisibleRow();
	for (unsigned int r = first; r < last && r < fitted.size(); r++, y += rowHeight) {
		bool selected = ((int)r == selectedRow);
		// A band of shade behind the selected row, which is what tells it apart
		// from whichever row happens to be under the mouse.
		if (selected)
			Boxhook::Draw(GetX() - 2, y, contentWidth, rowHeight, 0, BTOneHalf);
		for (unsigned int c = 0; c < fitted[r].size(); c++) {
			if (fitted[r][c].length() == 0)
				continue;
			// The selected row is lit the same way as a hovered one, so the
			// columns keep their own colours instead of the whole row going flat.
			TextColor color = columns[c].color;
			if ((selected || (int)r == hovered) && columns[c].hoverColor != Disabled)
				color = columns[c].hoverColor;
			Texthook::Draw(GetX() + layout[c].x, y, None, font, color, "%s",
				fitted[r][c].c_str());
		}
	}

	// The scrollbar: a shaded rail down the gutter with a lighter thumb whose
	// length and position say how much of the list is on screen and where in it
	// we are. Only drawn when there is something to scroll, so a short list is
	// left clean.
	if (GetMaxScrollTop() > 0) {
		unsigned int trackX = GetX() + xSize - LIST_SCROLLBAR_WIDTH;
		// Whiter while it is being used, so it reads as something you hold
		// rather than a marker that happens to be under the cursor.
		bool lit = draggingThumb || InScrollbar((*p_D2CLIENT_MouseX), (*p_D2CLIENT_MouseY));

		Boxhook::Draw(trackX, ScrollTrackTop(), LIST_SCROLLBAR_WIDTH,
			ScrollTrackHeight(), 0, BTOneHalf);
		Boxhook::Draw(trackX + LIST_THUMB_INSET, ScrollThumbTop(),
			LIST_SCROLLBAR_WIDTH - (2 * LIST_THUMB_INSET), ScrollThumbHeight(),
			lit ? LIST_THUMB_COLOR_LIT : LIST_THUMB_COLOR, BTNormal);
	}
	Unlock();
}
