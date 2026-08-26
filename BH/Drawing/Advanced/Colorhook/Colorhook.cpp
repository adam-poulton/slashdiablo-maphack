#include "../../../D2Ptrs.h"
#include "../../../Common.h"
#include "Colorhook.h"
#include "../../Basic/Boxhook/Boxhook.h"
#include "../../Basic/Framehook/Framehook.h"
#include "../../Basic/Texthook/Texthook.h"
#include "../../Basic/Crosshook/Crosshook.h"

using namespace Drawing;

// The swatch, and where the label starts after it. As with the checkbox, the
// reported width was the label's alone, so the hook answered for a box sitting to
// the left of everything it draws.
//
// The swatch is a diamond drawn about a centre, so it is placed half its width in
// from the hook's left edge. It used to be drawn about the edge itself, which put
// half of it outside the hook: that half could not be clicked, and it hung out
// into the margin past everything else on its row.
#define COLOR_SWATCH_HALF_WIDTH		8
#define COLOR_SWATCH_HALF_HEIGHT	4
#define COLOR_SWATCH_GAP			5
#define COLOR_SWATCH_WIDTH	((2 * COLOR_SWATCH_HALF_WIDTH) + COLOR_SWATCH_GAP)

Colorhook* Colorhook::current;

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
	char buffer[4096];
	va_list arg;
	va_start(arg, formatString);
	vsprintf_s(buffer, 4096, formatString.c_str(), arg);
	va_end(arg);
	text = buffer;
}

void Colorhook::SetText(std::string newText) {
	char buffer[4096];
	va_list arg;
	va_start(arg, newText);
	vsprintf_s(buffer, 4096, newText.c_str(), arg);
	va_end(arg);
	Lock();
	text = buffer;
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

void Colorhook::SetColor(unsigned int newColor) {
	if (newColor < 0 || newColor > 255)
		return;
	Lock();
	*currentColor = newColor;
	Unlock();
}

Colorhook::~Colorhook() {
	if (Colorhook::current == this)
		Colorhook::current = NULL;
}

bool Colorhook::OnLeftClick(bool up, unsigned int x, unsigned int y) {
	if (Colorhook::current == this && x >= 310 && y >= 205 && x <= 490 && y <= 385 && up) {
		SetColor(curColor);
		Colorhook::current = false;
		return true;
	} else if (InRange(x,y) && Colorhook::current == false) {
		if (up)
			Colorhook::current = this;
		return true;
	}
	return false;
}

bool Colorhook::OnRightClick(bool up, unsigned int x, unsigned int y) {
	if (Colorhook::current == this) {
		Colorhook::current = false;
		return true;
	}
	return false;
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

void Colorhook::OnDraw() {
	Lock();
	if (Colorhook::current == this) {
		//Draw the shaded background
		Boxhook::Draw(0, 0, Hook::GetScreenWidth(), Hook::GetScreenHeight(), 0, BTOneHalf);
		//Draw the actual choose color box
		Framehook::Draw(310, 180, 180, 220, 0, BTNormal);
		//Draw title
		Texthook::Draw(360, 186, false, 0, White, "Choose Color");
		int col = 1, boxX1, boxX2, boxY1, boxY2;
		int mX = (*p_D2CLIENT_MouseX);
		int mY = (*p_D2CLIENT_MouseY);
		for (int n = 1, row = 1; n <= 255; n++, row++) {
			if (row == 16) {
				col++;
				row = 0;
			}
			//Color square begin/end pixel coordinates
			boxX1 = 321 + (row * 10);
			boxX2 = 331 + (row * 10);
			boxY1 = 190 + (col * 10);
			boxY2 = 200 + (col * 10);
			//Set current color based on mouse location
			if (mX >= boxX1 && mY >= boxY1 && mX <= boxX2 && mY <= boxY2)
				curColor = n;
			//Draw each color box
			D2GFX_DrawRectangle(boxX1, boxY1, boxX2, boxY2, n, 5);
		}
		//Draw the +ish symbol showing the currently hovered color
		CHAR szLines[][2] = { 0,-2, 4,-4, 8,-2, 4,0, 8,2, 4,4, 0,2, -4,4, -8,2, -4,0, -8,-2, -4,-4, 0,-2 };
		for (unsigned int x = 0; x < 12; x++)
			D2GFX_DrawLine(457 + szLines[x][0], 380 + szLines[x][1], 457 + szLines[x + 1][0], 380 + szLines[x + 1][1], curColor, -1);
		//Draw instructions
		Texthook::Draw(320, 384, false, 0, White, "Left Click - Select");
		Texthook::Draw(320, 368, false, 0, White, "Right Click - Close");
	} else {
		DWORD size = D2WIN_SetTextSize(0);
		wchar_t* wText = AnsiToUnicode(GetText().c_str());
		unsigned int drawColor = IsEnabled() ?
			(InRange(*p_D2CLIENT_MouseX, *p_D2CLIENT_MouseY) ?
				hoverColor : textColor) :
			disabledColor;
		D2WIN_DrawText(wText, GetX() + COLOR_SWATCH_WIDTH, GetY() + GetYSize(),
			drawColor, 0);
		delete[] wText;
		D2WIN_SetTextSize(size);
		// Centred on the bottom of the line rather than the middle of it: the
		// glyphs beside it sit on their baseline, and reach nowhere near the top of
		// the room the font is given.
		Crosshook::Draw(GetX() + COLOR_SWATCH_HALF_WIDTH,
			GetY() + GetYSize() - COLOR_SWATCH_HALF_HEIGHT, GetColor());
	}
	Unlock();
}
