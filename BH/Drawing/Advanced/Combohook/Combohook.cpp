#include "Combohook.h"
#include "../../Basic/Framehook/Framehook.h"
#include "../../Basic/Texthook/Texthook.h"
#include "../../../D2Ptrs.h"

using namespace Drawing;
using namespace std;

Combohook* Combohook::current;

void Combohook::SetOpen(bool open) {
	Lock();
	active = open;
	if (open)
		current = this;
	else if (current == this)
		current = NULL;
	Unlock();
}

Combohook::Combohook(HookVisibility visibility, unsigned int x, unsigned int y, unsigned int xSize, unsigned int* index, std::vector<std::string> opts)
	: Hook(visibility, x, y) {
		SetXSize(xSize);
		currentIndex = index;
		options = opts;
		SetFont(0);
		SetOpen(false);
}

Combohook::Combohook(HookGroup* group, unsigned int x, unsigned int y, unsigned int xSize, unsigned int* index, std::vector<std::string> opts)
	: Hook(group, x, y) {
		SetXSize(xSize);
		currentIndex = index;
		options = opts;
		SetFont(0);
		SetOpen(false);
}

Combohook::~Combohook() {
	if (Combohook::current == this)
		Combohook::current = NULL;
}

bool Combohook::OnLeftClick(bool up, unsigned int x, unsigned int y) {
	// Check if we clicked on a inactive combo box.
	if (InHook(x, y) && !active) {
		if (up)
			SetOpen(true);

		return true;
	}
	if (active && x >= GetX() && y >= GetY() && x < GetX() + GetXSize() &&
			y < GetOptionY((unsigned int)options.size())) {
		int n = 0;
		for (vector<string>::iterator it = options.begin(); it < options.end(); it++,n++) {
			unsigned int optionY = GetOptionY(n);
			bool hovering = y >= optionY && y < optionY + GetYSize();
			if (hovering && up) {
				SetSelectedIndex(n);
				SetOpen(false);
				return true;
			}
		}
		if (up)
			SetOpen(false);
		return true;
	}
	SetOpen(false);
	return false;
}

void Combohook::OnDraw() {
	TextColor valueColor = IsEnabled() ? Gold : DISABLED_TEXT_COLOR;
	Framehook::Draw(GetX(), GetY(), GetXSize(), GetYSize(), 0, BTNormal);
	Texthook::Draw(GetX() + COMBO_PADDING_X, GetY() + COMBO_PADDING_TOP, 0, GetFont(),
		valueColor, options.at(GetSelectedIndex()));
	Texthook::Draw(GetX() + GetXSize() - COMBO_ARROW_GAP, GetY() + COMBO_PADDING_TOP, 0, GetFont(),
		IsEnabled() ? (InHook((*p_D2CLIENT_MouseX), (*p_D2CLIENT_MouseY))||active?Tan:Gold)
			: DISABLED_TEXT_COLOR, "v");
	// The open list is not drawn here; see DrawOpenList().
}

void Combohook::DrawOpenList() {
	if (!active)
		return;
	// Scrolled out of view, or its window closed, while the list was open. It
	// takes no clicks in that state, so it would never be closed by one and
	// would come back open the next time it was shown.
	if (!IsActive()) {
		SetOpen(false);
		return;
	}

	Framehook::Draw(GetX(), GetListY(), GetXSize(),
		(unsigned int)options.size() * GetYSize(), 0, BTNormal);
	unsigned int mouseX = (*p_D2CLIENT_MouseX);
	unsigned int mouseY = (*p_D2CLIENT_MouseY);
	unsigned int n = 0;
	for (vector<string>::iterator it = options.begin(); it < options.end(); it++,n++) {
		unsigned int optionY = GetOptionY(n);
		// An option is drawn at the same inset as the value in the closed box, so
		// the one the box is showing does not shift as the list opens over it.
		bool hovering = mouseX >= GetX() && mouseX < GetX() + GetXSize() &&
			mouseY >= optionY && mouseY < optionY + GetYSize();
		Texthook::Draw(GetX() + COMBO_PADDING_X, optionY + COMBO_PADDING_TOP, 0,
			GetFont(), hovering ? Tan : Gold, *it);
	}
}
