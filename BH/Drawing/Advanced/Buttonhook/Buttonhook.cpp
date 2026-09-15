#include "Buttonhook.h"
#include "../../Basic/Boxhook/Boxhook.h"
#include "../../Basic/Framehook/Framehook.h"
#include "../../Basic/Linehook/Linehook.h"
#include "../../../D2Ptrs.h"

namespace Drawing {

// The palette the marks are drawn from. Grey while the button is idle is what
// every other unlit piece of chrome uses, and it is what a disabled button
// stays at whatever colour the owner asked for.
#define BUTTON_DISABLED_SHADE	0xD0

namespace {

// The nearest odd number at or below, and never nothing. A mark drawn out of
// odd widths has a middle pixel to be centred on.
unsigned int Odd(unsigned int size) {
	if (size < 1)
		return 1;
	return ((size % 2) == 0) ? (size - 1) : size;
}

}	// namespace

Buttonhook::Buttonhook(HookVisibility visibility, unsigned int x, unsigned int y,
		unsigned int size, ButtonIcon icon) :
	Hook(visibility, x, y), icon(icon), size(size), pressed(false),
	color(0xD0), hoverColor(0x20) {}

Buttonhook::Buttonhook(HookGroup* group, unsigned int x, unsigned int y,
		unsigned int size, ButtonIcon icon) :
	Hook(group, x, y), icon(icon), size(size), pressed(false),
	color(0xD0), hoverColor(0x20) {}

bool Buttonhook::IsHovered() {
	return IsActive() && IsEnabled() &&
		InRange((unsigned int)Hook::GetMouseX(), (unsigned int)Hook::GetMouseY());
}

// Three bars, each a third narrower than the one above and centred on it: the
// funnel a filter is drawn as everywhere, and the shape that survives being
// drawn at a dozen pixels, which a funnel with an outline does not.
//
// Everything is worked out from the side it is drawn in, so the mark keeps its
// proportions at whatever height the row it sits on turns out to be.
void Buttonhook::DrawFilter(unsigned int x, unsigned int y, unsigned int side,
		unsigned int shade) {
	unsigned int band = (side >= 12) ? 2 : 1;

	// Every bar an odd number of pixels wide and drawn about one middle, which
	// is what centres each on the one above to the pixel rather than half a
	// pixel off it.
	unsigned int width = Odd(side);
	unsigned int bars[] = { width, Odd(width * 3 / 5), Odd(width / 3) };
	unsigned int middle = x + ((side - width) / 2) + (width / 2);

	for (unsigned int bar = 0; bar < 3; bar++) {
		unsigned int barY = y + ((bar * (side - band)) / 2);
		Boxhook::Draw(middle - (bars[bar] / 2), barY, bars[bar], band, shade,
			BTNormal);
	}
}

// Two strokes corner to corner. Drawn as lines rather than as a glyph so it
// matches the funnel's weight instead of the font's.
void Buttonhook::DrawClose(unsigned int x, unsigned int y, unsigned int side,
		unsigned int shade) {
	Linehook::Draw(x, y, x + side, y + side, shade);
	Linehook::Draw(x + side, y, x, y + side, shade);
}

void Buttonhook::OnDraw() {
	if (!IsActive())
		return;
	Lock();

	// A pressed button gets a solid field and an unpressed one a translucent
	// field, which is how the search box beside it says whether it has the
	// caret. One convention for both, so the row reads as one row.
	BoxTrans fill = pressed ? BTFull : BTOneHalf;
	D2GFX_DrawRectangle(GetX(), GetY(), GetX() + size, GetY() + size, 0, fill);
	Framehook::DrawBorder(GetX(), GetY(), size, size, fill);

	unsigned int shade = !IsEnabled() ? BUTTON_DISABLED_SHADE :
		(IsHovered() ? hoverColor : color);

	// The mark is drawn inside the frame rather than against it, and only where
	// the button is big enough to have an inside at all. A share of the button
	// rather than a fixed number of pixels, so a small button is not left with a
	// mark too small to read.
	unsigned int inset = size / BUTTON_ICON_DIVISOR;
	if (inset < BUTTON_ICON_MIN_INSET)
		inset = BUTTON_ICON_MIN_INSET;
	if (size > 2 * inset) {
		unsigned int side = size - (2 * inset);
		if (icon == IconFilter)
			DrawFilter(GetX() + inset, GetY() + inset, side, shade);
		else
			DrawClose(GetX() + inset, GetY() + inset, side, shade);
	}

	Unlock();
}

// Claimed on the press so the release is paired back here, and acted on at the
// release, as every other control in the window is.
bool Buttonhook::OnLeftClick(bool up, unsigned int x, unsigned int y) {
	if (!InRange(x, y))
		return false;
	if (GetLeftClickHandler()) {
		Lock();
		GetLeftClickHandler()(up, this, GetLeftClickVoid());
		Unlock();
	}
	return true;
}

}
