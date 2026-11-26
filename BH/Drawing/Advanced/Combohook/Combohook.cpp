#include "Combohook.h"
#include "../../Basic/Framehook/Framehook.h"
#include "../../Basic/Scrollbar/Scrollbar.h"
#include "../../Basic/Texthook/Texthook.h"
#include "../../../D2Ptrs.h"

using namespace Drawing;
using namespace std;

// The shortest the list's thumb is drawn, so there is always something to take
// hold of.
#define COMBO_THUMB_MIN			8

Combohook* Combohook::current;

void Combohook::Init(unsigned int width, unsigned int* index, std::vector<std::string> opts) {
	SetXSize(width);
	currentIndex = index;
	options = opts;
	SetFont(0);
	arrow = true;
	maxVisible = 0;
	scroll = 0;
	dragging = false;
	dragOffset = 0;
	SetOpen(false);
}

void Combohook::SetOpen(bool open) {
	Lock();
	active = open;
	dragging = false;
	if (open) {
		current = this;
		// Opened with the option in force in view, moving the list no further
		// than it takes.
		unsigned int chosen = GetSelectedIndex();
		if (chosen < scroll)
			ScrollTo((int)chosen);
		else if (chosen >= scroll + VisibleRows())
			ScrollTo((int)chosen - (int)VisibleRows() + 1);
		else
			ScrollTo((int)scroll);
	} else if (current == this) {
		current = NULL;
	}
	Unlock();
}

Combohook::Combohook(HookVisibility visibility, unsigned int x, unsigned int y, unsigned int xSize, unsigned int* index, std::vector<std::string> opts)
	: Hook(visibility, x, y) {
	Init(xSize, index, opts);
}

Combohook::Combohook(HookGroup* group, unsigned int x, unsigned int y, unsigned int xSize, unsigned int* index, std::vector<std::string> opts)
	: Hook(group, x, y) {
	Init(xSize, index, opts);
}

Combohook::~Combohook() {
	if (Combohook::current == this)
		Combohook::current = NULL;
}

// Where the value and the options are drawn from, which is the one place the
// two ways of laying a box out are told apart: a box with an arrow keeps its
// value clear of the left frame, and one without puts it in the middle of the
// room the missing arrow leaves.
unsigned int Combohook::ValueX(unsigned int x, unsigned int width) {
	return arrow ? (x + COMBO_PADDING_X) : (x + (width / 2));
}

int Combohook::ValueAlignment() {
	return arrow ? None : Center;
}

unsigned int Combohook::VisibleRows() {
	unsigned int count = (unsigned int)options.size();
	return (maxVisible == 0 || count < maxVisible) ? count : maxVisible;
}

unsigned int Combohook::MaxScroll() {
	return (unsigned int)options.size() - VisibleRows();
}

void Combohook::ScrollTo(int first) {
	int most = (int)MaxScroll();
	scroll = (unsigned int)(first < 0 ? 0 : (first > most ? most : first));
}

unsigned int Combohook::GetListY() {
	unsigned int below = GetY() + GetYSize();
	unsigned int height = GetListHeight();
	if (below + height > Hook::GetScreenHeight() && GetY() >= height)
		return GetY() - height;
	return below;
}

unsigned int Combohook::BarLeft() {
	return GetX() + GetXSize() - COMBO_SCROLLBAR_INSET - Scrollbar::Width();
}

unsigned int Combohook::ThumbHeight() {
	return Scrollbar::ThumbHeight(GetListHeight(), VisibleRows(),
		(unsigned int)options.size(), COMBO_THUMB_MIN);
}

unsigned int Combohook::ThumbTop() {
	return Scrollbar::ThumbTop(GetListY(), GetListHeight(), ThumbHeight(), scroll,
		MaxScroll());
}

bool Combohook::InBar(unsigned int x, unsigned int y) {
	return Scrolls() && Scrollbar::InBar(x, y, BarLeft(), GetListY(), GetListHeight());
}

int Combohook::OptionAt(unsigned int x, unsigned int y) {
	unsigned int top = GetListY();
	if (x < GetX() || x >= GetX() + GetXSize() || y < top || y >= top + GetListHeight())
		return -1;
	if (InBar(x, y))
		return -1;
	unsigned int index = scroll + (y - top) / GetRowHeight();
	return index < options.size() ? (int)index : -1;
}

bool Combohook::OnLeftClick(bool up, unsigned int x, unsigned int y) {
	if (!active) {
		if (!InHook(x, y))
			return false;
		if (up)
			SetOpen(true);
		return true;
	}

	if (dragging) {
		if (up)
			dragging = false;
		return true;
	}
	if (InBar(x, y)) {
		// The thumb is taken where it was pressed; the rail either side of it
		// moves the list a page that way.
		if (!up) {
			unsigned int thumbTop = ThumbTop();
			if (y >= thumbTop && y < thumbTop + ThumbHeight()) {
				dragging = true;
				dragOffset = (int)(y - thumbTop);
			} else {
				int page = (int)VisibleRows();
				ScrollTo((int)scroll + (y < thumbTop ? -page : page));
			}
		}
		return true;
	}
	int option = OptionAt(x, y);
	if (option >= 0) {
		if (up) {
			SetSelectedIndex((unsigned int)option);
			SetOpen(false);
			OnSelected((unsigned int)option);
		}
		return true;
	}
	// The box itself shuts the list it opened.
	if (InHook(x, y)) {
		if (up)
			SetOpen(false);
		return true;
	}
	SetOpen(false);
	return false;
}

// A notch a row, rather than the rows a long list moves by: the list is short,
// and a page of it at a time would skip past most of what it holds.
bool Combohook::OnMouseWheel(int notches, unsigned int x, unsigned int y) {
	if (!active || !Scrolls())
		return false;
	unsigned int top = GetListY();
	if (x < GetX() || x >= GetX() + GetXSize() || y < top || y >= top + GetListHeight())
		return false;
	ScrollTo((int)scroll - notches);
	return true;
}

void Combohook::DrawOption(unsigned int index, unsigned int x, unsigned int y,
		unsigned int width, TextColor color) {
	Texthook::Draw(ValueX(x, width), y + COMBO_PADDING_TOP, ValueAlignment(), GetFont(),
		color, options.at(index));
}

void Combohook::OnDraw() {
	// Every other hook checks this for itself, and a box that ignored it was
	// drawn wherever it had last been laid out however switched off it was.
	if (!IsActive() || options.empty())
		return;

	TextColor valueColor = IsEnabled() ? Gold : DISABLED_TEXT_COLOR;
	Framehook::Draw(GetX(), GetY(), GetXSize(), GetYSize(), 0, BTNormal);
	DrawOption(GetSelectedIndex(), GetX(), GetY(), GetXSize(), valueColor);
	if (arrow) {
		// As far in from the right of the frame as the value is from the left.
		Texthook::Draw(GetX() + GetXSize() - COMBO_PADDING_X, GetY() + GetTextInset(), Right, GetFont(),
			IsEnabled() ? (InHook(Hook::GetMouseX(), Hook::GetMouseY())||active?Tan:Gold)
				: DISABLED_TEXT_COLOR, "v");
	}
	// The open list is not drawn here; see DrawOpenList().
}

void Combohook::DrawOpenList() {
	if (!active)
		return;
	// Scrolled out of view, or its window closed, while the list was open.
	if (!IsActive()) {
		SetOpen(false);
		return;
	}

	unsigned int mouseX = Hook::GetMouseX();
	unsigned int mouseY = Hook::GetMouseY();
	unsigned int top = GetListY();
	unsigned int height = GetListHeight();
	if (dragging) {
		unsigned int travel = height > ThumbHeight() ? height - ThumbHeight() : 0;
		scroll = Scrollbar::ScrollForThumbTop((int)mouseY - dragOffset - (int)top, travel,
			MaxScroll());
	}

	Framehook::Draw(GetX(), top, GetXSize(), height, 0, BTNormal);
	int hovered = dragging ? -1 : OptionAt(mouseX, mouseY);
	unsigned int rows = VisibleRows();
	for (unsigned int n = scroll; n < scroll + rows && n < options.size(); n++) {
		// An option is drawn at the same inset as the value in the closed box, so
		// the one the box is showing does not shift as the list opens over it.
		DrawOption(n, GetX(), GetOptionY(n), GetXSize(), hovered == (int)n ? Tan : Gold);
	}
	if (Scrolls()) {
		bool lit = dragging || InBar(mouseX, mouseY);
		Scrollbar::Draw(BarLeft(), top, height, ThumbTop(), ThumbHeight(), lit);
	}
}
