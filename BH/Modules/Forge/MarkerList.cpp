#include "MarkerList.h"
#include <algorithm>
#include "../../Constants.h"
#include "../../D2Ptrs.h"
#include "../../Drawing/Basic/Boxhook/Boxhook.h"
#include "../../Drawing/Basic/Scrollbar/Scrollbar.h"
#include "../../Drawing/Basic/Texthook/Texthook.h"

using namespace Drawing;

#define ML_ROW_TEXT_TOP		((MM_ROW_HEIGHT - MM_TEXT_HEIGHT) / 2)
static_assert(MM_ROW_HEIGHT >= MapIcon::ChestHeight + 4,
	"a row holds the game's sprite clear of its neighbours");

// The selected row's band reaches this far out past what is drawn on it, as a
// list's does, so the marker does not sit against the band's edge.
#define ML_ROW_INSET		2

// A row's marker is drawn in a column of its own, no larger than the row has
// room for, and the name starts after it. A marker that is only its line shows
// a short rule there, as the editor's icon list does for no icon.
#define ML_ROW_ICON_CENTRE	10
#define ML_ROW_ICON_SIZE	4
#define ML_ROW_LABEL_LEFT	24
#define ML_ROW_NONE_HALF	4
#define ML_ROW_NONE_COLOR	0xD0

// After the longest name, inside the band.
#define ML_LIST_PAD			8

// Gold for what can be clicked, white on a band for the one in hand, grey for a
// marker that draws nothing at all and for a caption, which is not to be clicked.
#define ML_ROW_COLOR		Gold
#define ML_ROW_HOVER		Tan
#define ML_ROW_SELECTED		White
#define ML_ROW_OFF			Grey
#define ML_CAPTION_COLOR	Grey
#define ML_ROW_BAND			UI_SURFACE_BAND

#define ML_THUMB_MIN		8

MarkerListhook::MarkerListhook(HookGroup* group, unsigned int x, unsigned int y) :
		Hook(group, x, y), fittedWidth(0), xSize(0), ySize(0), first(0), selected(-1),
		reveal(false), dragging(false), grabOffset(0) {
}

unsigned int MarkerListhook::RowHeight(const MarkerListRow& row) {
	return row.caption ? MM_CAPTION_HEIGHT : MM_ROW_HEIGHT;
}

void MarkerListhook::SetRows(const std::vector<MarkerListRow>& newRows) {
	Lock();
	if (!(newRows == rows)) {
		rows = newRows;
		fitted.clear();
		if (first > MaxFirst())
			first = MaxFirst();
	}
	Unlock();
}

void MarkerListhook::SetSize(unsigned int width, unsigned int height) {
	Lock();
	if (width != xSize || height != ySize) {
		xSize = width;
		ySize = height;
		if (first > MaxFirst())
			first = MaxFirst();
	}
	Unlock();
}

unsigned int MarkerListhook::GetNaturalXSize() {
	unsigned int widest = 0;
	Lock();
	for (const MarkerListRow& row : rows) {
		unsigned int text = (unsigned int)Texthook::GetTextSize(row.label, 0).x;
		unsigned int width = ML_ROW_INSET + (row.caption ? 0 : ML_ROW_LABEL_LEFT) + text +
			ML_LIST_PAD;
		widest = (std::max)(widest, width);
	}
	Unlock();
	return widest;
}

unsigned int MarkerListhook::GetNaturalYSize() {
	unsigned int height = 0;
	Lock();
	for (const MarkerListRow& row : rows)
		height += RowHeight(row);
	Unlock();
	return height;
}

// The first row the list can start at and still end on its last: the rows are
// whole, so it is counted back from the end.
unsigned int MarkerListhook::MaxFirst() const {
	unsigned int height = 0;
	for (unsigned int i = (unsigned int)rows.size(); i > 0; i--) {
		height += RowHeight(rows[i - 1]);
		if (height > ySize)
			return i;
	}
	return 0;
}

unsigned int MarkerListhook::VisibleCount() const {
	unsigned int height = 0, count = 0;
	for (unsigned int i = first; i < rows.size(); i++) {
		height += RowHeight(rows[i]);
		if (height > ySize)
			break;
		count++;
	}
	return count;
}

// The gutter is given up only by a list that scrolls, so one that holds a
// handful of markers is banded to its full width.
unsigned int MarkerListhook::RowWidth() const {
	unsigned int gutter = Scrolls() ? Scrollbar::GutterWidth() : 0;
	return xSize > gutter ? xSize - gutter : 0;
}

int MarkerListhook::IndexOf(int id) const {
	for (unsigned int i = 0; i < rows.size(); i++)
		if (!rows[i].caption && rows[i].id == id)
			return (int)i;
	return -1;
}

unsigned int MarkerListhook::RowTop(unsigned int index) {
	unsigned int y = GetY();
	for (unsigned int i = first; i < index && i < rows.size(); i++)
		y += RowHeight(rows[i]);
	return y;
}

int MarkerListhook::IndexAt(unsigned int x, unsigned int y) {
	if (x < GetX() || x >= GetX() + RowWidth() || y < GetY())
		return -1;
	unsigned int top = GetY(), shown = VisibleCount();
	for (unsigned int i = first; i < first + shown; i++) {
		unsigned int height = RowHeight(rows[i]);
		if (y < top + height)
			return (int)i;
		top += height;
	}
	return -1;
}

// Far enough for the picked row to be shown whole, and for a caption straight
// above it to come with it, so the first marker under a caption is not shown
// with nothing saying what it is.
void MarkerListhook::Reveal() {
	int index = IndexOf(selected);
	if (index < 0)
		return;
	unsigned int from = (unsigned int)index;
	if (from > 0 && rows[from - 1].caption)
		from--;
	if (from < first) {
		first = from;
	} else {
		while (first < (unsigned int)index && (unsigned int)index >= first + VisibleCount())
			first++;
	}
	if (first > MaxFirst())
		first = MaxFirst();
	reveal = false;
}

int MarkerListhook::GetSelected() {
	return selected;
}

void MarkerListhook::SetSelected(int id) {
	Lock();
	selected = id;
	reveal = true;
	Unlock();
}

void MarkerListhook::MoveSelection(int delta) {
	Lock();
	int index = IndexOf(selected);
	int step = delta < 0 ? -1 : 1;
	int left = delta < 0 ? -delta : delta;
	int found = index;
	for (int i = index + step; i >= 0 && i < (int)rows.size() && left > 0; i += step) {
		if (rows[i].caption)
			continue;
		found = i;
		left--;
	}
	if (index < 0) {
		found = -1;
		for (unsigned int i = 0; i < rows.size() && found < 0; i++)
			if (!rows[i].caption)
				found = (int)i;
	}
	if (found >= 0) {
		selected = rows[found].id;
		reveal = true;
	}
	Unlock();
}

void MarkerListhook::SelectFirst() {
	Lock();
	for (unsigned int i = 0; i < rows.size(); i++) {
		if (rows[i].caption)
			continue;
		selected = rows[i].id;
		reveal = true;
		break;
	}
	Unlock();
}

void MarkerListhook::SelectLast() {
	Lock();
	for (unsigned int i = (unsigned int)rows.size(); i > 0; i--) {
		if (rows[i - 1].caption)
			continue;
		selected = rows[i - 1].id;
		reveal = true;
		break;
	}
	Unlock();
}

int MarkerListhook::GetHoveredRow() {
	if (!IsActive())
		return -1;
	unsigned int x = Hook::GetMouseX(), y = Hook::GetMouseY();
	if (IsCoveredAt(x, y))
		return -1;
	Lock();
	int index = IndexAt(x, y);
	Unlock();
	return index;
}

bool MarkerListhook::GetRowBox(unsigned int index, unsigned int& x, unsigned int& y,
		unsigned int& width, unsigned int& height) {
	Lock();
	bool shown = index >= first && index < first + VisibleCount();
	if (shown) {
		x = GetX();
		y = RowTop(index);
		width = RowWidth();
		height = RowHeight(rows[index]);
	}
	Unlock();
	return shown;
}

const MarkerListRow* MarkerListhook::GetRow(unsigned int index) {
	return index < rows.size() ? &rows[index] : NULL;
}

unsigned int MarkerListhook::ThumbHeight() {
	return Scrollbar::ThumbHeight(ySize, VisibleCount(), (unsigned int)rows.size(),
		ML_THUMB_MIN);
}

unsigned int MarkerListhook::ThumbTop() {
	return Scrollbar::ThumbTop(GetY(), ySize, ThumbHeight(), first, MaxFirst());
}

bool MarkerListhook::InBar(unsigned int x, unsigned int y) {
	return Scrolls() && Scrollbar::InBar(x, y, GetX() + xSize - Scrollbar::Width(),
		GetY(), ySize);
}

bool MarkerListhook::OnLeftClick(bool up, unsigned int x, unsigned int y) {
	if (up && dragging) {
		Lock();
		dragging = false;
		Unlock();
		return true;
	}
	if (!IsActive())
		return false;

	Lock();
	if (InBar(x, y)) {
		// The thumb is taken where it was pressed; the rail either side of it
		// moves the list a screenful that way, then follows the mouse.
		if (!up) {
			unsigned int thumbTop = ThumbTop(), thumbHeight = ThumbHeight();
			if (y >= thumbTop && y <= thumbTop + thumbHeight) {
				grabOffset = (int)(y - thumbTop);
			} else {
				int page = (std::max)(1, (int)VisibleCount() - 1);
				int moved = (int)first + (y < thumbTop ? -page : page);
				first = (unsigned int)(std::min)((std::max)(moved, 0), (int)MaxFirst());
				grabOffset = (int)(ThumbHeight() / 2);
			}
			dragging = true;
		}
		Unlock();
		return true;
	}

	int index = IndexAt(x, y);
	if (index < 0) {
		Unlock();
		return false;
	}
	// The press is taken so the release comes back here, where the pick is made.
	if (up && !rows[index].caption)
		selected = rows[index].id;
	Unlock();
	return true;
}

bool MarkerListhook::OnMouseWheel(int notches, unsigned int x, unsigned int y) {
	if (!IsActive())
		return false;
	if (x < GetX() || x >= GetX() + xSize || y < GetY() || y >= GetY() + ySize)
		return false;
	Lock();
	bool scrolls = Scrolls();
	if (scrolls) {
		int moved = (int)first - notches * (int)Scrollbar::WheelRows();
		first = (unsigned int)(std::min)((std::max)(moved, 0), (int)MaxFirst());
	}
	Unlock();
	return scrolls;
}

void MarkerListhook::DrawRow(unsigned int index, unsigned int y, unsigned int width,
		bool hovered) {
	const MarkerListRow& row = rows[index];
	unsigned int x = GetX();
	if (row.caption) {
		Texthook::Draw(x + ML_ROW_INSET, y + MM_CAPTION_TEXT_TOP, None, 0,
			ML_CAPTION_COLOR, "%s", fitted[index].c_str());
		return;
	}

	bool chosen = (row.id == selected);
	if (chosen)
		Boxhook::Draw(x, y, width, MM_ROW_HEIGHT, ML_ROW_BAND, BTNormal);

	int iconX = (int)x + ML_ROW_INSET + ML_ROW_ICON_CENTRE;
	int centreY = (int)y + MM_ROW_HEIGHT / 2;
	if (!row.style.DrawsIcon()) {
		D2GFX_DrawLine(iconX - ML_ROW_NONE_HALF, centreY, iconX + ML_ROW_NONE_HALF,
			centreY, ML_ROW_NONE_COLOR, -1);
	} else {
		unsigned int size = (std::min)(row.style.size, (unsigned int)ML_ROW_ICON_SIZE);
		MapIcon::Draw(row.style.Icon(), size, iconX, centreY, row.style.Look());
	}

	TextColor color = ML_ROW_COLOR;
	if (chosen)
		color = ML_ROW_SELECTED;
	else if (hovered)
		color = ML_ROW_HOVER;
	else if (!row.style.Draws())
		color = ML_ROW_OFF;
	Texthook::Draw(x + ML_ROW_INSET + ML_ROW_LABEL_LEFT, y + ML_ROW_TEXT_TOP, None, 0,
		color, "%s", fitted[index].c_str());
}

void MarkerListhook::OnDraw() {
	if (!IsActive()) {
		dragging = false;
		return;
	}
	Lock();
	if (reveal)
		Reveal();
	// There is no mouse move event to hang a drag off, so the thumb catches up
	// with the cursor here, once per frame.
	if (dragging && Scrolls()) {
		unsigned int travel = ySize > ThumbHeight() ? ySize - ThumbHeight() : 0;
		int top = (int)Hook::GetMouseY() - grabOffset - (int)GetY();
		first = Scrollbar::ScrollForThumbTop(top, travel, MaxFirst());
	}

	unsigned int width = RowWidth();
	if (fitted.size() != rows.size() || fittedWidth != width) {
		fitted.clear();
		for (const MarkerListRow& row : rows) {
			unsigned int taken = ML_ROW_INSET + (row.caption ? 0 : ML_ROW_LABEL_LEFT) +
				ML_LIST_PAD;
			fitted.push_back(Texthook::Fit(row.label, width > taken ? width - taken : 0, 0));
		}
		fittedWidth = width;
	}

	unsigned int mouseX = Hook::GetMouseX(), mouseY = Hook::GetMouseY();
	int hovered = IsCoveredAt(mouseX, mouseY) ? -1 : IndexAt(mouseX, mouseY);
	unsigned int y = GetY(), shown = VisibleCount();
	for (unsigned int i = first; i < first + shown; i++) {
		DrawRow(i, y, width, (int)i == hovered);
		y += RowHeight(rows[i]);
	}

	if (Scrolls()) {
		bool lit = dragging || InBar(mouseX, mouseY);
		Scrollbar::Draw(GetX() + xSize - Scrollbar::Width(), GetY(), ySize, ThumbTop(),
			ThumbHeight(), lit);
	}
	Unlock();
}
