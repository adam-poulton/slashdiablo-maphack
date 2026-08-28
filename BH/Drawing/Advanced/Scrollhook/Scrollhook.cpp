#include "Scrollhook.h"
#include "../../Basic/Scrollbar/Scrollbar.h"
#include "../../../D2Ptrs.h"

using namespace std;
using namespace Drawing;

// Smallest the thumb is allowed to get, so there is always something to take hold
// of however long the contents are.
#define SCROLL_MIN_THUMB	8

unsigned int ScrollContent::GetX() {
	return box->GetX();
}

// The top of the box, less how far down the contents we are. That subtraction
// underflows once scrolled past the box's own distance down the screen, which is
// deliberate and harmless: unsigned arithmetic wraps consistently, so adding a
// row's own offset back on lands at the right place again. Only rows that are
// actually shown are ever asked, and for those the true position is positive.
unsigned int ScrollContent::GetY() {
	return box->GetY() - box->GetRowY(box->GetFirstVisibleRow());
}

// The width contents lay out in, so a hook aligned Right within the group lands
// beside the scrollbar gutter rather than under it.
unsigned int ScrollContent::GetXSize() {
	return box->GetContentWidth();
}

unsigned int ScrollContent::GetYSize() {
	return box->GetYSize();
}

bool ScrollContent::IsActive() {
	return box->IsActive();
}

Scrollhook::Scrollhook(HookVisibility visibility, unsigned int x, unsigned int y,
		unsigned int xSize, unsigned int ySize) :
	Hook(visibility, x, y), xSize(xSize), ySize(ySize), scrollRow(0),
	draggingThumb(false), thumbGrabOffset(0) {
	content = new ScrollContent(this);
}

Scrollhook::Scrollhook(HookGroup* group, unsigned int x, unsigned int y,
		unsigned int xSize, unsigned int ySize) :
	Hook(group, x, y), xSize(xSize), ySize(ySize), scrollRow(0),
	draggingThumb(false), thumbGrabOffset(0) {
	content = new ScrollContent(this);
}

Scrollhook::~Scrollhook() {
	Lock();
	// Everything ever put in a row, whether or not it is in one now. Each hook
	// takes itself out of the group's list as it goes, so the group has to outlive
	// them all.
	for (unsigned int i = 0; i < owned.size(); i++)
		delete owned[i];
	owned.clear();
	rows.clear();
	delete content;
	content = NULL;
	Unlock();
}

HookGroup* Scrollhook::GetContent() {
	return content;
}

void Scrollhook::SetSize(unsigned int newXSize, unsigned int newYSize) {
	Lock();
	xSize = newXSize;
	ySize = newYSize;
	ClampScroll();
	Unlock();
}

void Scrollhook::ClearRows() {
	Lock();
	rows.clear();
	Unlock();
}

void Scrollhook::ClearContents() {
	Lock();
	for (unsigned int i = 0; i < owned.size(); i++)
		delete owned[i];
	owned.clear();
	rows.clear();
	scrollRow = 0;
	Unlock();
}

unsigned int Scrollhook::AddRow(unsigned int height) {
	Lock();
	Row row;
	row.height = height;
	row.y = rows.empty() ? 0 : (rows.back().y + rows.back().height);
	rows.push_back(row);
	unsigned int index = (unsigned int)rows.size() - 1;
	Unlock();
	return index;
}

void Scrollhook::AddToRow(unsigned int row, Hook* hook) {
	if (!hook)
		return;
	Lock();
	if (row < rows.size()) {
		rows[row].hooks.push_back(hook);
		bool known = false;
		for (unsigned int i = 0; i < owned.size() && !known; i++)
			known = (owned[i] == hook);
		if (!known)
			owned.push_back(hook);
	}
	Unlock();
}

unsigned int Scrollhook::GetRowY(unsigned int row) {
	return (row < rows.size()) ? rows[row].y : 0;
}

unsigned int Scrollhook::GetRowHeight(unsigned int row) {
	return (row < rows.size()) ? rows[row].height : 0;
}

unsigned int Scrollhook::GetContentHeight() {
	return rows.empty() ? 0 : (rows.back().y + rows.back().height);
}

unsigned int Scrollhook::GetGutterWidth() {
	return Scrollbar::GutterWidth();
}

unsigned int Scrollhook::GetContentWidth() {
	unsigned int gutter = GetGutterWidth();
	return (xSize > gutter) ? (xSize - gutter) : 0;
}

// Whole rows only, counted from the one at the top of the box.
unsigned int Scrollhook::GetVisibleRowCount() {
	unsigned int used = 0, count = 0;
	for (unsigned int r = scrollRow; r < rows.size(); r++) {
		if (used + rows[r].height > ySize)
			break;
		used += rows[r].height;
		count++;
	}
	// A row taller than the whole box is shown anyway. Hiding it would leave the
	// box blank with no way to reach what is in it.
	if (count == 0 && scrollRow < rows.size())
		count = 1;
	return count;
}

// The furthest down the contents can start and still fill the box, so scrolling
// stops with the last row against the bottom rather than running on into space.
unsigned int Scrollhook::GetMaxScrollRow() {
	unsigned int used = 0;
	for (unsigned int idx = (unsigned int)rows.size(); idx-- > 0; ) {
		if (used + rows[idx].height > ySize)
			return idx + 1;
		used += rows[idx].height;
	}
	return 0;
}

void Scrollhook::ClampScroll() {
	unsigned int max = GetMaxScrollRow();
	if (scrollRow > max)
		scrollRow = max;
}

void Scrollhook::SetScrollRow(unsigned int row) {
	Lock();
	unsigned int max = GetMaxScrollRow();
	scrollRow = (row > max) ? max : row;
	Unlock();
}

void Scrollhook::Scroll(int rowDelta) {
	Lock();
	int target = (int)scrollRow + rowDelta;
	if (target < 0)
		target = 0;
	unsigned int max = GetMaxScrollRow();
	scrollRow = ((unsigned int)target > max) ? max : (unsigned int)target;
	Unlock();
}

void Scrollhook::ScrollRowIntoView(unsigned int row) {
	Lock();
	if (row >= rows.size()) {
		Unlock();
		return;
	}
	if (row < scrollRow) {
		scrollRow = row;
	} else {
		// Walk the top of the view down until the row is shown whole.
		while (row >= scrollRow + GetVisibleRowCount() && scrollRow < GetMaxScrollRow())
			scrollRow++;
	}
	Unlock();
}

// The box drives the active flag of what is in it: a row out of view must be
// neither drawn nor offered input, and that flag is what both are gated on.
void Scrollhook::ApplyRowVisibility() {
	unsigned int shown = GetVisibleRowCount();
	for (unsigned int r = 0; r < rows.size(); r++) {
		bool visible = (r >= scrollRow && r < scrollRow + shown);
		for (unsigned int h = 0; h < rows[r].hooks.size(); h++)
			rows[r].hooks[h]->SetActive(visible);
	}
}

unsigned int Scrollhook::ThumbHeight() {
	return Scrollbar::ThumbHeight(ScrollTrackHeight(), GetVisibleRowCount(),
		(unsigned int)rows.size(), SCROLL_MIN_THUMB);
}

unsigned int Scrollhook::ThumbTop() {
	return Scrollbar::ThumbTop(ScrollTrackTop(), ScrollTrackHeight(),
		ThumbHeight(), scrollRow, GetMaxScrollRow());
}

bool Scrollhook::InScrollbar(unsigned int x, unsigned int y) {
	if (GetMaxScrollRow() == 0)
		return false;
	return Scrollbar::InBar(x, y, GetX() + xSize - Scrollbar::Width(),
		ScrollTrackTop(), ScrollTrackHeight());
}

void Scrollhook::DragThumbTo(unsigned int mouseY) {
	unsigned int max = GetMaxScrollRow();
	unsigned int track = ScrollTrackHeight(), thumb = ThumbHeight();
	if (max == 0 || track <= thumb)
		return;
	int top = (int)mouseY - thumbGrabOffset - (int)ScrollTrackTop();
	scrollRow = Scrollbar::ScrollForThumbTop(top, track - thumb, max);
}

bool Scrollhook::OnLeftClick(bool up, unsigned int x, unsigned int y) {
	if (!IsActive())
		return false;

	// Let go wherever the button comes up, since a drag usually ends with the
	// cursor away from the bar it started on.
	if (up && draggingThumb) {
		Lock();
		draggingThumb = false;
		Unlock();
		return true;
	}

	if (InScrollbar(x, y)) {
		if (!up) {
			Lock();
			unsigned int thumbTop = ThumbTop(), thumbHeight = ThumbHeight();
			// Grabbing the rail rather than the thumb jumps the thumb to the
			// cursor, taking hold of it in the middle.
			if (y < thumbTop || y > thumbTop + thumbHeight)
				thumbGrabOffset = (int)(thumbHeight / 2);
			else
				thumbGrabOffset = (int)(y - thumbTop);
			draggingThumb = true;
			DragThumbTo(y);
			Unlock();
		}
		return true;
	}
	return false;
}

bool Scrollhook::OnMouseWheel(int notches, unsigned int x, unsigned int y) {
	if (!IsActive() || GetMaxScrollRow() == 0)
		return false;
	if (x < GetX() || x > GetX() + xSize || y < GetY() || y > GetY() + ySize)
		return false;

	Scroll(-notches * (int)Scrollbar::WheelRows());
	return true;
}

void Scrollhook::OnDraw() {
	if (!IsActive())
		return;

	Lock();
	// There is no mouse move event to hang a drag off, so the thumb catches up
	// with the cursor here, once per frame.
	if (draggingThumb)
		DragThumbTo((*p_D2CLIENT_MouseY));
	ClampScroll();
	ApplyRowVisibility();

	unsigned int shown = GetVisibleRowCount();
	for (unsigned int r = scrollRow; r < scrollRow + shown && r < rows.size(); r++) {
		for (unsigned int h = 0; h < rows[r].hooks.size(); h++)
			rows[r].hooks[h]->OnDraw();
	}

	// Only when there is something to scroll, so a short panel is left clean.
	if (GetMaxScrollRow() > 0) {
		bool lit = draggingThumb ||
			InScrollbar((*p_D2CLIENT_MouseX), (*p_D2CLIENT_MouseY));
		Scrollbar::Draw(GetX() + xSize - Scrollbar::Width(), ScrollTrackTop(),
			ScrollTrackHeight(), ThumbTop(), ThumbHeight(), lit);
	}
	Unlock();
}
