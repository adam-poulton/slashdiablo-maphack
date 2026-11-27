#include "Texthook.h"
#include "TextMetrics.h"
#include "../../../Common.h"
#include "../../../D2Ptrs.h"

using namespace std;
using namespace Drawing;

/* Basic Hook Initializer
 *		Used for just drawing basic text on screen.
 */
Texthook::Texthook(HookVisibility visibility, unsigned int x, unsigned int y, std::string formatString, ...) :
Hook(visibility, x, y) {
	//Correctly format the string from the given arguments.
	SetFont(0);
	SetColor(White);
	SetHoverColor(Disabled);
	SetDisabledColor(DISABLED_TEXT_COLOR);
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
Texthook::Texthook(HookGroup *group, unsigned int x, unsigned int y, std::string formatString, ...) :
Hook(group, x, y) {
	//Correctly format the string from the given arguments.
	SetFont(0);
	SetColor(White);
	SetHoverColor(Disabled);
	SetDisabledColor(DISABLED_TEXT_COLOR);
	char buffer[4096];
	va_list arg;
	va_start(arg, formatString);
	vsprintf_s(buffer, 4096, formatString.c_str(), arg);
	va_end(arg);
	text = buffer;
}

/* GetFont()
 *	Returns what d2 font we use to draw.
 */
unsigned int Texthook::GetFont() {
	return font;
}

/* SetFont(unsigned int newFont)
 *	Sets when the font to draw with.
 */
void Texthook::SetFont(unsigned int newFont) {
	Lock();
	font = newFont;
	Unlock();
}

/* GetColor()
 *	Returns what color the text will be drawn.
 */
TextColor Texthook::GetColor() {
	return color;
}

/* SetColor()
 *	Sets what color the text will be drawn in.
 */
void Texthook::SetColor(TextColor newColor) {
	Lock();
	color = newColor;
	Unlock();
}

/* GetHoverColor()
 *	Return what color the text will be when hovered.
 */
TextColor Texthook::GetHoverColor() {
	return hoverColor;
}

/* SetHoverColor()
 *	Sets what color to draw when hovered.
 */
void Texthook::SetHoverColor(TextColor newHoverColor) {
	Lock();
	hoverColor = newHoverColor;
	Unlock();
}

/* GetDisabledColor()
 *	Return what color the text will be while switched off.
 */
TextColor Texthook::GetDisabledColor() {
	return disabledColor;
}

/* SetDisabledColor()
 *	Sets what color to draw while switched off.
 */
void Texthook::SetDisabledColor(TextColor newDisabledColor) {
	Lock();
	disabledColor = newDisabledColor;
	Unlock();
}

/* GetText()
 *	Returns what text will be drawn.
 */
std::string Texthook::GetText() {
	return text;
}

/* SetText(string formaString, ...)
 *	Sets a new formatted string as the text
 */
void Texthook::SetText(std::string formatString, ...) {
	char buffer[4096];
	va_list arg;
	va_start(arg, formatString);
	vsprintf_s(buffer, 4096, formatString.c_str(), arg);
	va_end(arg);
	text = buffer;
}

/* GetXSize()
 *	Returns how long the text is.
 */
unsigned int Texthook::GetXSize() {
	DWORD width, fileNo;
	wchar_t* wString = AnsiToUnicode(text.c_str());
	DWORD oldFont = D2WIN_SetTextSize(font);
	D2WIN_GetTextWidthFileNo(wString, &width, &fileNo);
	D2WIN_SetTextSize(oldFont);
	delete[] wString;
	return width; 
}

/* GetXSize()
 *	Returns how tall the text is.
 */
unsigned int Texthook::GetYSize() {
	return BaselineOffset(font);
}

/* Draw()
 *	Draws the text, must be called inside a Draw Patch
 */
void Texthook::OnDraw() {
	if (!IsActive())
		return;

	Lock();
	wchar_t* wString = AnsiToUnicode(text.c_str());

	unsigned int drawColor = color;
	if (!IsEnabled())
		drawColor = disabledColor;
	else if (InRange(Hook::GetMouseX(), Hook::GetMouseY()) && GetHoverColor() != Disabled)
		drawColor = hoverColor;

	DWORD oldFont = D2WIN_SetTextSize(font);
	D2WIN_DrawText(wString, GetX(), GetY() + GetYSize(), drawColor, 0);
	D2WIN_SetTextSize(oldFont);
	delete[] wString;
	Unlock();
}

/* OnLeftClick(bool up, unsigned int x, unsigned int y)
 *	Check if the text hook has been clicked on.
 */
bool Texthook::OnLeftClick(bool up, unsigned int x, unsigned int y) {
	if (InRange(x,y) && GetLeftClickHandler()) {
		bool block = false;
		Lock();
		block = GetLeftClickHandler()(up, this, GetLeftClickVoid());
		Unlock();
		return block;
	}
	return false;
}

/* OnRightClick(bool up, unsigned int x, unsigned int y)
 *	Check if the text hook has been clicked on.
 */
bool Texthook::OnRightClick(bool up, unsigned int x, unsigned int y) {
	if (InRange(x,y) && GetRightClickHandler()) {
		bool block = false;
		Lock();
		block = GetRightClickHandler()(up, this, GetRightClickVoid());
		Unlock();
		return block;
	}
	return false;
}

/* Wrap(string text, unsigned int width, unsigned int font)
 *	Breaks text to a width, on spaces where it can.
 */
std::vector<std::string> Texthook::Wrap(const std::string& text,
		unsigned int width, unsigned int font) {
	std::vector<std::string> lines;
	if (width == 0 || text.empty()) {
		lines.push_back(text);
		return lines;
	}

	std::string line;
	std::string::size_type start = 0;
	while (start <= text.length()) {
		std::string::size_type space = text.find(' ', start);
		std::string word = (space == std::string::npos) ?
			text.substr(start) : text.substr(start, space - start);

		std::string candidate = line.empty() ? word : (line + " " + word);
		if (!line.empty() &&
				(unsigned int)GetTextSize(candidate, font).x > width) {
			lines.push_back(line);
			line = word;
		} else {
			line = candidate;
		}

		if (space == std::string::npos)
			break;
		start = space + 1;
	}
	if (!line.empty())
		lines.push_back(line);
	if (lines.empty())
		lines.push_back("");
	return lines;
}

/* WrapBalanced(string text, unsigned int width, unsigned int font)
 *	Breaks text to a width, into lines of about the same length.
 */
std::vector<std::string> Texthook::WrapBalanced(const std::string& text,
		unsigned int width, unsigned int font) {
	std::vector<std::string> lines = Wrap(text, width, font);
	if (lines.size() < 2)
		return lines;

	// The narrowest width that takes no more lines than the whole width does.
	// Narrowing never takes fewer, so it can be searched for.
	unsigned int low = 1, high = width;
	while (low < high) {
		unsigned int middle = (low + high) / 2;
		std::vector<std::string> narrower = Wrap(text, middle, font);
		if (narrower.size() > lines.size()) {
			low = middle + 1;
		} else {
			high = middle;
			lines = narrower;
		}
	}
	return lines;
}

// Starts from a proportional guess, so wide text doesn't need a measurement per
// character.
std::string Texthook::Fit(const std::string& text, unsigned int width,
		unsigned int font) {
	if (text.length() == 0 || width == 0)
		return "";

	unsigned int full = (unsigned int)GetTextSize(text, font).x;
	if (full <= width)
		return text;

	size_t length = (size_t)(((double)text.length() * width) / full) + 1;
	if (length > text.length())
		length = text.length();
	while (length > 0) {
		std::string candidate = text.substr(0, length) + TEXT_ELLIPSIS;
		if ((unsigned int)GetTextSize(candidate, font).x <= width)
			return candidate;
		length--;
	}
	return "";
}

/* GetTextSize(string text, unsigned int font)
 *	Returns the dimensions of the given text!
 */
POINT Texthook::GetTextSize(string text, unsigned int font) {
	DWORD width, fileNo;
	wchar_t* wString = AnsiToUnicode(text.c_str());
	DWORD oldFont = D2WIN_SetTextSize(font);
	D2WIN_GetTextWidthFileNo(wString, &width, &fileNo);
	D2WIN_SetTextSize(oldFont);
	POINT point = { static_cast<long>(width), static_cast<long>(BaselineOffset(font)) };
	delete[] wString;
	return point;
}

POINT Texthook::GetTextSize(wchar_t* text, unsigned int font) {
	DWORD width, fileNo;
	DWORD oldFont = D2WIN_SetTextSize(font);
	D2WIN_GetTextWidthFileNo(text, &width, &fileNo);
	D2WIN_SetTextSize(oldFont);
	POINT point = { static_cast<long>(width), static_cast<long>(BaselineOffset(font)) };
	return point;
}

/* Draw(unsigned int x, unsigned int y, unsigned int font, TextColor color, std::string text, ...)
 *	Static function to wrap drawing text on screen, to be used for things that are rapidly changing(Monsters on map, etc..)
 */
bool Texthook::Draw(unsigned int x, unsigned int y, int align, unsigned int font, TextColor color, std::string text, ...) {
	//Convert all %s's into proper values.
	char buffer[4096];
	va_list arg;
	va_start(arg, text);
	vsprintf_s(buffer, 4096, text.c_str(), arg);
	va_end(arg);

	//Convert multi-byte to wide character
	wchar_t* wString = AnsiToUnicode(buffer);

	unsigned int properX = x;
	if (align == Center)
		x = x - (GetTextSize(buffer, font).x / 2);

	if (align == Right)
		x = x - GetTextSize(buffer, font).x;

	//Draw the text!
	DWORD size = D2WIN_SetTextSize(font);
	D2WIN_DrawText(wString, x, y + BaselineOffset(font), color, 0);
	D2WIN_SetTextSize(size);

	delete[] wString;
	return true;
}

bool Texthook::Draw(unsigned int x, unsigned int y, int align, unsigned int font, TextColor color, wchar_t* text, ...) {
	//Convert all %s's into proper values.
	wchar_t* buffer = new wchar_t[4096];
	va_list arg;
	va_start(arg, text);
	vswprintf_s(buffer, 4096, text, arg);
	va_end(arg);

	unsigned int properX = x;
	if (align == Center)
		x = x - (GetTextSize(buffer, font).x / 2);

	if (align == Right)
		x = x - GetTextSize(buffer, font).x;

	//Draw the text!
	DWORD size = D2WIN_SetTextSize(font);
	D2WIN_DrawText(buffer, x, y + BaselineOffset(font), color, 0);
	D2WIN_SetTextSize(size);

	delete[] buffer;
	return true;
}