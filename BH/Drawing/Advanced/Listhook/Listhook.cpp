#include "Listhook.h"
#include "../../Basic/Scrollbar/Scrollbar.h"
#include "../../Basic/Boxhook/Boxhook.h"
#include "../../../D2Ptrs.h"

using namespace std;
using namespace Drawing;

#define LIST_ROW_PADDING	2
#define LIST_HEADER_GAP		4
#define LIST_ELLIPSIS		".."

// How far the rows under a heading sit in from its text.
#define LIST_GROUP_INDENT	8

// The marker gets a column of its own rather than being pasted onto the front of
// the label: the two markers are not the same width in the game's fonts, so
// pasting them on shifted the whole heading sideways on every fold. The column is
// measured from the markers so it holds the wider at any font, and each is centred
// in it so both states sit in the same place.
//
// The marker and the count keep the dim colour throughout, so they read as
// controls rather than as the first and last characters of the heading.
#define LIST_GROUP_MARKER_GAP	3
#define LIST_GROUP_COUNT_GAP	4
#define LIST_GROUP_DIM_COLOR	Grey
#define LIST_GROUP_UNFOLDED		"-"
#define LIST_GROUP_FOLDED		"+"

// The scrollbar sits in a gutter on the right, kept clear of the columns whether
// or not there is anything to scroll. How it looks and where its thumb goes are
// Scrollbar's, so the list and the scrolling box cannot come to differ.

static unsigned int FontHeight(unsigned int font) {
	unsigned int height[] = {10,11,18,24,10,13,7,13,10,12,8,8,7,12};
	return height[font];
}

Listhook::Listhook(HookVisibility visibility, unsigned int x, unsigned int y, unsigned int xSize, unsigned int ySize) :
Hook(visibility, x, y), xSize(xSize), ySize(ySize), font(0), scrollTop(0), headerColor(Gold),
groupColor(Gold), groupHoverColor(White), groupIndent(LIST_GROUP_INDENT),
hasGroups(false), unfoldWidth(0), foldWidth(0), markerWidth(0),
foldingSuspended(false),
selectedRow(-1), draggingThumb(false), thumbGrabOffset(0) {
}

Listhook::Listhook(HookGroup* group, unsigned int x, unsigned int y, unsigned int xSize, unsigned int ySize) :
Hook(group, x, y), xSize(xSize), ySize(ySize), font(0), scrollTop(0), headerColor(Gold),
groupColor(Gold), groupHoverColor(White), groupIndent(LIST_GROUP_INDENT),
hasGroups(false), unfoldWidth(0), foldWidth(0), markerWidth(0),
foldingSuspended(false),
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

void Listhook::SetRows(const std::vector<ListRow>& newRows) {
	Lock();
	rows = newRows;
	hasGroups = false;
	for (unsigned int r = 0; r < rows.size() && !hasGroups; r++)
		hasGroups = rows[r].group;
	// Grouping decides the indent, which changes the width the columns share, so
	// the columns are resolved again rather than only the cells refitted.
	RebuildShown();
	Layout();
	selectedRow = -1;
	ClampScroll();
	Unlock();
}

void Listhook::SetRows(const std::vector<std::vector<std::string>>& newRows) {
	std::vector<ListRow> plain;
	plain.reserve(newRows.size());
	for (unsigned int r = 0; r < newRows.size(); r++)
		plain.push_back(ListRow(newRows[r]));
	SetRows(plain);
}

bool Listhook::IsGroupRow(int row) {
	return row >= 0 && row < (int)rows.size() && rows[row].group;
}

void Listhook::SetGroupIndent(unsigned int newIndent) {
	Lock();
	groupIndent = newIndent;
	Layout();
	Unlock();
}

// Measured on layout rather than on draw, so drawing costs no measurement.
void Listhook::MeasureMarkers() {
	if (!hasGroups) {
		unfoldWidth = foldWidth = markerWidth = 0;
		return;
	}
	unfoldWidth = (unsigned int)Texthook::GetTextSize(LIST_GROUP_UNFOLDED, font).x;
	foldWidth = (unsigned int)Texthook::GetTextSize(LIST_GROUP_FOLDED, font).x;
	markerWidth = (unfoldWidth > foldWidth) ? unfoldWidth : foldWidth;
}

// Zero without headings, so nothing is indented for a marker never drawn.
unsigned int Listhook::GroupLabelX() {
	return hasGroups ? (markerWidth + LIST_GROUP_MARKER_GAP) : 0;
}

unsigned int Listhook::GroupMarkerX(unsigned int row) {
	unsigned int width = IsFolded(row) ? foldWidth : unfoldWidth;
	return (markerWidth > width) ? ((markerWidth - width) / 2) : 0;
}

// An unlabelled heading cannot have been folded: there is nothing to key it on.
bool Listhook::IsFolded(unsigned int row) {
	if (foldingSuspended || !rows[row].group || rows[row].cells.empty())
		return false;
	return folded.find(rows[row].cells[0]) != folded.end();
}

unsigned int Listhook::GroupRowCount(unsigned int row) {
	unsigned int count = 0;
	for (unsigned int r = row + 1; r < rows.size() && !rows[r].group; r++)
		count++;
	return count;
}

// No marker while folding is suspended: a list that cannot be folded should not
// offer to be. The label still starts past the empty column, so nothing moves.
std::string Listhook::GroupMarker(unsigned int row) {
	if (foldingSuspended || !rows[row].group || rows[row].cells.empty())
		return "";
	return IsFolded(row) ? LIST_GROUP_FOLDED : LIST_GROUP_UNFOLDED;
}

std::string Listhook::GroupLabel(unsigned int row) {
	return rows[row].cells.empty() ? "" : rows[row].cells[0];
}

// What is folded away cannot be counted off the screen, so the heading says.
std::string Listhook::GroupCount(unsigned int row) {
	if (!IsFolded(row))
		return "";
	unsigned int count = GroupRowCount(row);
	if (count == 0)
		return "";
	return "[" + std::to_string(count) + "]";
}

void Listhook::RebuildShown() {
	shown.clear();
	shown.reserve(rows.size());

	bool hiding = false;
	for (unsigned int r = 0; r < rows.size(); r++) {
		if (rows[r].group) {
			hiding = IsFolded(r);
			shown.push_back(r);
		} else if (!hiding) {
			shown.push_back(r);
		}
	}
}

int Listhook::ShownPosition(int row) {
	for (unsigned int i = 0; i < shown.size(); i++) {
		if ((int)shown[i] == row)
			return (int)i;
	}
	return -1;
}

int Listhook::PositionAtOrAfter(int row) {
	for (unsigned int i = 0; i < shown.size(); i++) {
		if ((int)shown[i] >= row)
			return (int)i;
	}
	return (int)shown.size();
}

// A selection folded away is let go of rather than quietly moved to a neighbour
// the user did not choose.
void Listhook::AfterFold() {
	RebuildShown();
	FitRows();	// the marker and count changed, even if no column did
	if (selectedRow >= 0 && ShownPosition(selectedRow) < 0)
		selectedRow = -1;
	ClampScroll();
	ScrollSelectionIntoView();
}

int Listhook::GetGroupRowFor(int row) {
	Lock();
	int group = -1;
	if (row >= 0 && row < (int)rows.size()) {
		for (int r = row; r >= 0; r--) {
			if (rows[r].group) {
				group = r;
				break;
			}
		}
	}
	Unlock();
	return group;
}

bool Listhook::IsGroupFolded(int groupRow) {
	return IsGroupRow(groupRow) && IsFolded((unsigned int)groupRow);
}

void Listhook::SetGroupFolded(int groupRow, bool fold) {
	if (!IsGroupRow(groupRow) || rows[groupRow].cells.empty())
		return;
	// Honouring this would record a fold nothing on screen reflects, which would
	// then spring open the moment folding resumed.
	if (foldingSuspended)
		return;

	Lock();
	const std::string& label = rows[groupRow].cells[0];
	if (fold)
		folded.insert(label);
	else
		folded.erase(label);
	AfterFold();
	Unlock();
}

void Listhook::ToggleGroup(int groupRow) {
	SetGroupFolded(groupRow, !IsGroupFolded(groupRow));
}

void Listhook::FoldAllGroups() {
	Lock();
	for (unsigned int r = 0; r < rows.size(); r++) {
		if (rows[r].group && !rows[r].cells.empty())
			folded.insert(rows[r].cells[0]);
	}
	AfterFold();
	Unlock();
}

void Listhook::UnfoldAllGroups() {
	Lock();
	folded.clear();
	AfterFold();
	Unlock();
}

void Listhook::SetFoldingSuspended(bool suspend) {
	if (suspend == foldingSuspended)
		return;
	Lock();
	foldingSuspended = suspend;
	AfterFold();
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
	MeasureMarkers();

	// Headings ignore all of this and run the full width from the edge.
	unsigned int indent = hasGroups ? (GroupLabelX() + groupIndent) : 0;
	unsigned int content = GetContentWidth();
	unsigned int available = (content > indent) ? (content - indent) : 0;
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

	unsigned int x = indent;
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
		unsigned int edge = indent + available;
		if (x >= edge)
			width = 0;
		else if (x + width > edge)
			width = edge - x;

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
	cellX.clear();
	cellX.reserve(rows.size());
	groupCountX.assign(rows.size(), 0);
	unsigned int content = GetContentWidth();
	for (unsigned int r = 0; r < rows.size(); r++) {
		std::vector<std::string> cells;
		std::vector<unsigned int> positions;
		if (rows[r].group) {
			// The count is never cut, so the label is fitted to what is left of
			// the full width once the marker and the count have had their share.
			std::string count = GroupCount(r);
			unsigned int taken = GroupLabelX();
			if (count.length() > 0) {
				taken += LIST_GROUP_COUNT_GAP +
					(unsigned int)Texthook::GetTextSize(count, font).x;
			}
			unsigned int width = (content > taken) ? (content - taken) : 0;

			std::string label = FitCell(GroupLabel(r), width);
			groupCountX[r] = GroupLabelX() + LIST_GROUP_COUNT_GAP +
				(unsigned int)Texthook::GetTextSize(label, font).x;
			cells.push_back(label);
			cells.push_back(count);
		} else {
			// Where the cell before this one ended, for a column that flows on
			// from it rather than starting at a column of its own.
			unsigned int cursor = 0;
			for (unsigned int c = 0; c < layout.size() && c < rows[r].cells.size(); c++) {
				bool flows = (c > 0 && columns[c].flow);
				unsigned int x = flows ? (cursor + columns[c].gap) : layout[c].x;

				// A flow is laid out against the rest of the line rather than
				// against a share of it, from the column it runs on from onward,
				// so the line is only cut where it runs out.
				unsigned int width = layout[c].width;
				if (flows || (c + 1 < columns.size() && columns[c + 1].flow))
					width = (content > x) ? (content - x) : 0;

				std::string cell = FitCell(rows[r].cells[c], width);
				cursor = x + (unsigned int)Texthook::GetTextSize(cell, font).x;
				cells.push_back(cell);
				positions.push_back(x);
			}
		}
		fitted.push_back(cells);
		cellX.push_back(positions);
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
	return Scrollbar::GutterWidth();
}

unsigned int Listhook::GetContentWidth() {
	unsigned int gutter = GetGutterWidth();
	return (xSize > gutter) ? (xSize - gutter) : 0;
}

// Naming a folded row lands on the nearest row that is not, so a caller need not
// know how the list is folded.
void Listhook::SetSelectedRow(int row) {
	Lock();
	if (row < 0 || row >= (int)rows.size()) {
		selectedRow = -1;
	} else if (ShownPosition(row) >= 0) {
		selectedRow = row;
	} else {
		int at = PositionAtOrAfter(row);
		if (at >= (int)shown.size())
			at = (int)shown.size() - 1;
		selectedRow = (at >= 0) ? (int)shown[at] : -1;
	}
	ScrollSelectionIntoView();
	Unlock();
}

// Scrolls by as little as possible, so walking down the list with the arrow keys
// moves the view a row at a time instead of jumping the selection to the middle.
void Listhook::ScrollSelectionIntoView() {
	unsigned int visible = GetVisibleRows();
	int position = ShownPosition(selectedRow);
	if (position < 0 || visible == 0)
		return;
	unsigned int row = (unsigned int)position;
	if (row < scrollTop)
		scrollTop = row;
	else if (row >= scrollTop + visible)
		scrollTop = row - visible + 1;
}

void Listhook::MoveSelection(int delta) {
	if (shown.empty() || delta == 0)
		return;

	Lock();
	int last = (int)shown.size() - 1;

	// Counted in rows on screen, so a folded group costs one row of travel.
	int target;
	if (selectedRow < 0) {
		// Nothing selected yet, so step in from the edge the user is heading
		// away from rather than from the top of the list they cannot see.
		unsigned int visible = GetVisibleRows();
		int edge = (visible > 0) ? (int)(scrollTop + visible - 1) : (int)scrollTop;
		target = (delta > 0) ? (int)scrollTop : edge;
	} else {
		target = ShownPosition(selectedRow) + delta;
	}
	if (target < 0)
		target = 0;
	if (target > last)
		target = last;

	selectedRow = (int)shown[target];
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
	unsigned int position = GetFirstVisibleRow() + offset;
	return (position < shown.size()) ? (int)shown[position] : -1;
}

// Kept at least a row tall so there is always something to grab, however long
// the list gets.
unsigned int Listhook::ScrollThumbHeight() {
	return Scrollbar::ThumbHeight(ScrollTrackHeight(), GetVisibleRows(),
		(unsigned int)shown.size(), GetRowHeight());
}

unsigned int Listhook::ScrollThumbTop() {
	return Scrollbar::ThumbTop(ScrollTrackTop(), ScrollTrackHeight(),
		ScrollThumbHeight(), scrollTop, GetMaxScrollTop());
}

bool Listhook::InScrollbar(unsigned int x, unsigned int y) {
	if (GetMaxScrollTop() == 0)
		return false;
	return Scrollbar::InBar(x, y, GetX() + xSize - Scrollbar::Width(),
		ScrollTrackTop(), ScrollTrackHeight());
}

// Turns a thumb position back into a top row. Works in whole pixels of travel,
// so the thumb sits under the cursor rather than snapping to row boundaries.
void Listhook::DragThumbTo(unsigned int mouseY) {
	unsigned int max = GetMaxScrollTop();
	unsigned int travel = ScrollTrackHeight() - ScrollThumbHeight();
	if (max == 0 || travel == 0)
		return;

	int top = (int)mouseY - thumbGrabOffset - (int)ScrollTrackTop();
	scrollTop = Scrollbar::ScrollForThumbTop(top, travel, max);
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
	unsigned int position = GetFirstVisibleRow() + offset;
	unsigned int index = (position < shown.size()) ? shown[position] : rows.size();
	if (offset >= GetVisibleRows() || index >= rows.size()) {
		// The empty space past the last row is still part of the list, and
		// clicking it lets go of the selection. That is the usual way to dismiss
		// whatever the selection was putting on screen.
		ClearSelection();
		return true;
	}

	// The heading takes the selection too, so carrying on by keyboard starts from
	// the heading the mouse just used.
	if (rows[index].group) {
		SetSelectedRow((int)index);
		ToggleGroup((int)index);
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

	Scroll(-notches * (int)Scrollbar::WheelRows());
	return true;
}

unsigned int Listhook::GetLastVisibleRow() {
	unsigned int last = scrollTop + GetVisibleRows();
	if (last > shown.size())
		last = shown.size();
	return last;
}

unsigned int Listhook::GetMaxScrollTop() {
	unsigned int visible = GetVisibleRows();
	if (visible == 0 || shown.size() <= visible)
		return 0;
	return shown.size() - visible;
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
	for (unsigned int position = first; position < last; position++, y += rowHeight) {
		unsigned int r = shown[position];
		if (r >= fitted.size())
			continue;

		if (rows[r].group) {
			bool selected = ((int)r == selectedRow);
			if (selected)
				Boxhook::Draw(GetX() - 2, y, contentWidth, rowHeight, 0, BTOneHalf);

			TextColor color = ((selected || (int)r == hovered) &&
				groupHoverColor != Disabled) ? groupHoverColor : groupColor;

			std::string marker = GroupMarker(r);
			if (marker.length() > 0) {
				Texthook::Draw(GetX() + GroupMarkerX(r), y, None, font,
					LIST_GROUP_DIM_COLOR, "%s", marker.c_str());
			}
			if (fitted[r].size() > 0 && fitted[r][0].length() > 0) {
				Texthook::Draw(GetX() + GroupLabelX(), y, None, font, color, "%s",
					fitted[r][0].c_str());
			}
			if (fitted[r].size() > 1 && fitted[r][1].length() > 0) {
				Texthook::Draw(GetX() + groupCountX[r], y, None, font,
					LIST_GROUP_DIM_COLOR, "%s", fitted[r][1].c_str());
			}
			continue;
		}

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
			if (c < rows[r].colors.size() && rows[r].colors[c] != Disabled)
				color = rows[r].colors[c];
			// Over the top of either, so the lift is the last word.
			if ((selected || (int)r == hovered) && columns[c].hoverColor != Disabled)
				color = columns[c].hoverColor;
			Texthook::Draw(GetX() + cellX[r][c], y, None, font, color, "%s",
				fitted[r][c].c_str());
		}
	}

	// The scrollbar: a shaded rail down the gutter with a lighter thumb whose
	// length and position say how much of the list is on screen and where in it
	// we are. Only drawn when there is something to scroll, so a short list is
	// left clean.
	if (GetMaxScrollTop() > 0) {
		bool lit = draggingThumb || InScrollbar((*p_D2CLIENT_MouseX), (*p_D2CLIENT_MouseY));
		Scrollbar::Draw(GetX() + xSize - Scrollbar::Width(), ScrollTrackTop(),
			ScrollTrackHeight(), ScrollThumbTop(), ScrollThumbHeight(), lit);
	}
	Unlock();
}
