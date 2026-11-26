#include "../../../D2Ptrs.h"
#include "../../../Common.h"
#include "Colorhook.h"
#include "../../Basic/Boxhook/Boxhook.h"
#include "../../Basic/Framehook/Framehook.h"
#include "../../Basic/Texthook/Texthook.h"

using namespace Drawing;

// The swatch, and where the label starts after it. As with the checkbox, the
// reported width was the label's alone, so the hook answered for a box sitting to
// the left of everything it draws.
//
// The swatch is drawn about a centre, so it is placed half its width in from the
// hook's left edge. The X is as wide as the slot; anything else is narrower and
// sits in the middle of it, so the labels stay in one column whatever each row's
// swatch is.
#define COLOR_SWATCH_HALF_WIDTH		8
#define COLOR_SWATCH_HALF_HEIGHT	4
#define COLOR_SWATCH_GAP			5
#define COLOR_SWATCH_WIDTH	((2 * COLOR_SWATCH_HALF_WIDTH) + COLOR_SWATCH_GAP)

// A swatch that is a shape is drawn at the size that fills the slot's height.
#define COLOR_SWATCH_ICON_SIZE		4

// How far below the top of the line the swatch is centred. Font 0 stands its
// letters on a baseline above the bottom of its line, so what reads of the label
// is centred here, which is also where a checkbox's middle sits beside its label.
#define COLOR_SWATCH_CENTRE_Y		4

// The filled square's rim, in the grey every other control's edge is drawn in, so a
// dark colour still reads as a square against the panel.
#define COLOR_SWATCH_EDGE			0xD0

// A swatch whose colour is not in use: a square shown through, so the colour is
// still there to see but plainly not live, and a shape in the rim's grey, since a
// line cannot be drawn see-through.
#define COLOR_SWATCH_MUTED			BTOneHalf

// No colour, wherever it is shown: a slash, in red, as a "no" is drawn the world
// over, from the bottom left corner to the top right.
#define COLOR_NONE_SLASH			0x0A	// red

// The picker: a frame centred on the screen holding the palette as a grid, with
// what a click does written under it.
#define PICKER_WIDTH			180
#define PICKER_HEIGHT			220
#define PICKER_GRID_LEFT		11
#define PICKER_GRID_TOP			20
#define PICKER_CELL				10
#define PICKER_COLUMNS			16
#define PICKER_ENTRIES			256
#define PICKER_TEXT_LEFT		10
#define PICKER_SELECT_TOP		188
#define PICKER_CLOSE_TOP		204
#define PICKER_PREVIEW_X		158
#define PICKER_PREVIEW_Y		201

// What the hovered entry and the one already chosen are outlined in.
#define PICKER_HOVER_EDGE		0x20	// white
#define PICKER_CHOSEN_EDGE		0xD0	// grey

Colorhook* Colorhook::current;

static int PickerLeft() {
	return ((int)Hook::GetScreenWidth() - PICKER_WIDTH) / 2;
}

static int PickerTop() {
	return ((int)Hook::GetScreenHeight() - PICKER_HEIGHT) / 2;
}

static int PickerRows() {
	return PICKER_ENTRIES / PICKER_COLUMNS;
}

// A one pixel rim around the given extent, inside it. The sides stop short of the
// top and bottom rows, so a see-through rim is not drawn twice at its corners.
static void DrawEdges(int x, int y, int width, int height, unsigned int color,
		BoxTrans trans = BTNormal) {
	D2GFX_DrawRectangle(x, y, x + width, y + 1, color, trans);
	D2GFX_DrawRectangle(x, y + height - 1, x + width, y + height, color, trans);
	D2GFX_DrawRectangle(x, y + 1, x + 1, y + height - 1, color, trans);
	D2GFX_DrawRectangle(x + width - 1, y + 1, x + width, y + height - 1, color, trans);
}

// A pixel at a time, so a muted slash can be drawn see-through as the rim is.
static void DrawSlash(int x, int y, int side, BoxTrans trans) {
	for (int i = 0; i < side; i++)
		D2GFX_DrawRectangle(x + i, y + side - 1 - i, x + i + 1, y + side - i,
			COLOR_NONE_SLASH, trans);
}

static void DrawPaletteEdge(unsigned int entry, unsigned int color) {
	int x = PickerLeft() + PICKER_GRID_LEFT + (int)(entry % PICKER_COLUMNS) * PICKER_CELL;
	int y = PickerTop() + PICKER_GRID_TOP + (int)(entry / PICKER_COLUMNS) * PICKER_CELL;
	DrawEdges(x, y, PICKER_CELL, PICKER_CELL, color);
}

/* Basic Hook Initializer
 *		Used for just drawing basics.
 */
Colorhook::Colorhook(HookVisibility visibility, unsigned int x, unsigned int y, unsigned int* color, std::string formatString, ...) :
Hook(visibility, x, y) {
	//Correctly format the string from the given arguments.
	currentColor = color;
	textColor = Gold;
	hoverColor = Tan;
	disabledColor = DISABLED_TEXT_COLOR;
	swatch = MapIcon::Square;
	allowNone = false;
	char buffer[4096];
	va_list arg;
	va_start(arg, formatString);
	vsprintf_s(buffer, 4096, formatString.c_str(), arg);
	va_end(arg);
	text = buffer;
}

/* Group Hook Initializer
 *		Used in conjuction with other basic hooks to create an advanced hook.
 */
Colorhook::Colorhook(HookGroup *group, unsigned int x, unsigned int y, unsigned int* color, std::string formatString, ...) :
Hook(group, x, y) {
	//Correctly format the string from the given arguments.
	currentColor = color;
	textColor = Gold;
	hoverColor = Tan;
	disabledColor = DISABLED_TEXT_COLOR;
	swatch = MapIcon::Square;
	allowNone = false;
	char buffer[4096];
	va_list arg;
	va_start(arg, formatString);
	vsprintf_s(buffer, 4096, formatString.c_str(), arg);
	va_end(arg);
	text = buffer;
}

void Colorhook::SetText(std::string newText) {
	Lock();
	text = newText;
	Unlock();
}

void Colorhook::SetTextColor(TextColor newColor) {
	Lock();
	textColor = newColor;
	Unlock();
}

void Colorhook::SetHoverColor(TextColor newColor) {
	Lock();
	hoverColor = newColor;
	Unlock();
}

void Colorhook::SetDisabledColor(TextColor newColor) {
	Lock();
	disabledColor = newColor;
	Unlock();
}

void Colorhook::SetSwatch(MapIcon::Icon newSwatch) {
	Lock();
	swatch = newSwatch;
	Unlock();
}

void Colorhook::SetColor(unsigned int newColor) {
	if (newColor >= PICKER_ENTRIES)
		return;
	Lock();
	*currentColor = newColor;
	Unlock();
}

Colorhook::~Colorhook() {
	if (Colorhook::current == this)
		Colorhook::current = NULL;
}

int Colorhook::PaletteAt(int x, int y) {
	int left = PickerLeft() + PICKER_GRID_LEFT;
	int top = PickerTop() + PICKER_GRID_TOP;
	if (x < left || y < top)
		return -1;
	int column = (x - left) / PICKER_CELL;
	int row = (y - top) / PICKER_CELL;
	if (column >= PICKER_COLUMNS || row >= PickerRows())
		return -1;
	int entry = row * PICKER_COLUMNS + column;
	if (entry == (int)MapIcon::NoColor && !allowNone)
		return -1;
	return entry;
}

/* The entry is the one under the release, wherever the pointer was last seen
 * before it, and a release anywhere but on an entry leaves the picker open.
 */
bool Colorhook::OnLeftClick(bool up, unsigned int x, unsigned int y) {
	if (Colorhook::current == this) {
		int entry = PaletteAt((int)x, (int)y);
		if (up && entry >= 0) {
			SetColor((unsigned int)entry);
			Colorhook::current = NULL;
		}
		return true;
	}
	if (InRange(x, y) && !Colorhook::current) {
		if (up)
			Colorhook::current = this;
		return true;
	}
	return false;
}

bool Colorhook::OnRightClick(bool up, unsigned int x, unsigned int y) {
	if (Colorhook::current == this) {
		Colorhook::current = NULL;
		return true;
	}
	return false;
}

/* Escape closes the picker and leaves the colour as it was, as it closes every
 * other interface of ours. Taken on the release, and the press swallowed with
 * it, so the window the picker was opened from doesn't close on the same key.
 */
bool Colorhook::OnKey(bool up, BYTE key, LPARAM lParam) {
	if (Colorhook::current != this || key != VK_ESCAPE)
		return false;
	if (up)
		Colorhook::current = NULL;
	return true;
}

/* GetXSize()
 *	Returns how long the text is.
 */
unsigned int Colorhook::GetXSize() {
	DWORD width, fileNo;
	wchar_t* wString = AnsiToUnicode(GetText().c_str());
	DWORD oldFont = D2WIN_SetTextSize(0);
	D2WIN_GetTextWidthFileNo(wString, &width, &fileNo);
	D2WIN_SetTextSize(oldFont);
	delete[] wString;
	return width + COLOR_SWATCH_WIDTH;
}

/* GetYSize()
 *	Returns how tall the text is.
 */
unsigned int Colorhook::GetYSize() {
	return 10;
}

/* DrawSwatch()
 *	The colour as the given shape, centred on a point. A filled square is drawn
 *	opaque, as the palette is, with a rim; a shape is drawn as the map draws it.
 *	No colour is the square's rim with a slash through it, whatever the shape.
 */
void Colorhook::DrawSwatch(MapIcon::Icon swatch, int x, int y, unsigned int color,
		bool muted) {
	if (swatch != MapIcon::Square && color != MapIcon::NoColor) {
		MapIcon::DrawOutline(swatch, COLOR_SWATCH_ICON_SIZE, x, y,
			muted ? COLOR_SWATCH_EDGE : color);
		return;
	}
	int half = COLOR_SWATCH_HALF_HEIGHT;
	int side = 2 * half + 1;
	BoxTrans trans = muted ? COLOR_SWATCH_MUTED : BTNormal;
	DrawEdges(x - half, y - half, side, side, COLOR_SWATCH_EDGE, trans);
	if (color == MapIcon::NoColor)
		DrawSlash(x - half + 1, y - half + 1, side - 2, trans);
	else
		D2GFX_DrawRectangle(x - half + 1, y - half + 1, x + half, y + half, color, trans);
}

void Colorhook::OnDraw() {
	Lock();
	if (Colorhook::current == this) {
		int left = PickerLeft();
		int top = PickerTop();
		//Draw the shaded background
		Boxhook::Draw(0, 0, Hook::GetScreenWidth(), Hook::GetScreenHeight(), 0, BTOneHalf);
		Framehook::Draw(left, top, PICKER_WIDTH, PICKER_HEIGHT, 0, BTNormal);
		Texthook::Draw(left + PICKER_WIDTH / 2, top + 6, Center, 0, White, "Choose color");
		for (unsigned int n = 1; n < PICKER_ENTRIES; n++) {
			int x = left + PICKER_GRID_LEFT + (int)(n % PICKER_COLUMNS) * PICKER_CELL;
			int y = top + PICKER_GRID_TOP + (int)(n / PICKER_COLUMNS) * PICKER_CELL;
			D2GFX_DrawRectangle(x, y, x + PICKER_CELL, y + PICKER_CELL, n, BTNormal);
		}
		if (allowNone)
			DrawSlash(left + PICKER_GRID_LEFT, top + PICKER_GRID_TOP, PICKER_CELL, BTNormal);
		int hovered = PaletteAt(Hook::GetMouseX(), Hook::GetMouseY());
		unsigned int chosen = GetColor();
		if (chosen < PICKER_ENTRIES && (chosen != MapIcon::NoColor || allowNone))
			DrawPaletteEdge(chosen, PICKER_CHOSEN_EDGE);
		if (hovered >= 0)
			DrawPaletteEdge((unsigned int)hovered, PICKER_HOVER_EDGE);
		// What a click would choose, or what is chosen while the pointer is off
		// the palette.
		DrawSwatch(swatch, left + PICKER_PREVIEW_X, top + PICKER_PREVIEW_Y,
			hovered >= 0 ? (unsigned int)hovered : chosen, false);
		Texthook::Draw(left + PICKER_TEXT_LEFT, top + PICKER_SELECT_TOP, None, 0,
			White, "Left click - Select");
		Texthook::Draw(left + PICKER_TEXT_LEFT, top + PICKER_CLOSE_TOP, None, 0,
			White, "Right click / Esc - Close");
	} else {
		DWORD size = D2WIN_SetTextSize(0);
		wchar_t* wText = AnsiToUnicode(GetText().c_str());
		unsigned int drawColor = IsEnabled() ?
			(InRange(Hook::GetMouseX(), Hook::GetMouseY()) ?
				hoverColor : textColor) :
			disabledColor;
		D2WIN_DrawText(wText, GetX() + COLOR_SWATCH_WIDTH, GetY() + GetYSize(),
			drawColor, 0);
		delete[] wText;
		D2WIN_SetTextSize(size);
		DrawSwatch(swatch, GetX() + COLOR_SWATCH_HALF_WIDTH,
			GetY() + COLOR_SWATCH_CENTRE_Y, GetColor(), !IsEnabled());
	}
	Unlock();
}
