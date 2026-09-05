#include "Checkhook.h"
#include "../../../Common.h"
#include "../../../D2Ptrs.h"
#include "../../Basic/Framehook/Framehook.h"

using namespace Drawing;

// The box, and the clear space between it and the label. The label was drawn at a
// bare 18 while the reported width was the label's alone, which put the hook's
// clickable area 18 pixels left of what it draws: the far end of a label could not
// be clicked, and a short label was not over the hook at all.
#define CHECK_BOX_SIZE		12
#define CHECK_LABEL_GAP		6

// The label sits a little below the top of the box, so the two read as one line
// rather than the text sitting on the box's rim.
#define CHECK_LABEL_TOP		2

/* Basic Hook Initializer
 *		Used for drawing a checkbox on screen.
 */
Checkhook::Checkhook(HookVisibility visibility, unsigned int x, unsigned int y, bool* checked, std::string formatString, ...) :
Hook(visibility, x, y) {
	//Correctly format the string from the given arguments.
	SetTextColor(White);
	state = checked;
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
Checkhook::Checkhook(HookGroup* group, unsigned int x, unsigned int y, bool* checked, std::string formatString, ...) :
Hook(group, x, y) {
	//Correctly format the string from the given arguments.
	SetTextColor(Gold);
	state = checked;
	SetHoverColor(Tan);
	SetDisabledColor(DISABLED_TEXT_COLOR);
	char buffer[4096];
	va_list arg;
	va_start(arg, formatString);
	vsprintf_s(buffer, 4096, formatString.c_str(), arg);
	va_end(arg);
	text = buffer;
}

/* GetColor()
 *	Returns what color the text will be drawn.
 */
TextColor Checkhook::GetTextColor() {
	return color;
}

/* SetColor()
 *	Sets what color the text will be drawn in.
 */
void Checkhook::SetTextColor(TextColor newColor) {
	Lock();
	color = newColor;
	Unlock();
}

/* GetHoverColor()
 *	Return what color the text will be when hovered.
 */
TextColor Checkhook::GetHoverColor() {
	return hoverColor;
}

/* SetHoverColor()
 *	Sets what color to draw when hovered.
 */
void Checkhook::SetHoverColor(TextColor newHoverColor) {
	Lock();
	hoverColor = newHoverColor;
	Unlock();
}

/* GetDisabledColor()
 *	Returns the color drawn while switched off.
 */
TextColor Checkhook::GetDisabledColor() {
	return disabledColor;
}

/* SetDisabledColor()
 *	Sets the color to draw while switched off.
 */
void Checkhook::SetDisabledColor(TextColor newColor) {
	Lock();
	disabledColor = newColor;
	Unlock();
}

/* GetCheck()
 *	Returns what text will be drawn.
 */
std::string Checkhook::GetText() {
	return text;
}

/* SetCheck(string formaString, ...)
 *	Sets a new formatted string as the text
 */
void Checkhook::SetText(std::string formatString, ...) {
	char buffer[4096];
	va_list arg;
	va_start(arg, formatString);
	vsprintf_s(buffer, 4096, formatString.c_str(), arg);
	va_end(arg);
	text = buffer;
}

/* GetXSize()
 *	The box and its label together, which is what the hook draws and so what
 *	it should answer for when it is clicked or measured.
 */
unsigned int Checkhook::GetXSize() {
	if (text.length() == 0)
		return CHECK_BOX_SIZE;

	DWORD width, fileNo;
	wchar_t* wString = AnsiToUnicode(text.c_str());
	DWORD oldFont = D2WIN_SetTextSize(0);
	D2WIN_GetTextWidthFileNo(wString, &width, &fileNo);
	D2WIN_SetTextSize(oldFont);
	delete[] wString;
	// Exactly the box, the gap and the label, which is what is drawn.
	return width + CHECK_BOX_SIZE + CHECK_LABEL_GAP;
}

/* GetXSize()
 *	Returns how tall the text is.
 */
unsigned int Checkhook::GetYSize() {
	return CHECK_BOX_SIZE;
}

unsigned int Checkhook::GetTextInset() {
	return CHECK_LABEL_TOP;
}

/* IsChecked()
 *	Returns if the check is checked
 */
bool Checkhook::IsChecked() {
	return *state;
}

/* SetState()
 *	Sets the state of the check box.
 */
void Checkhook::SetState(bool checked) {
	Lock();
	*state = checked;
	Unlock();
}

/* Draw()
 *	Draws the text, must be called inside a Draw Patch
 */
void Checkhook::OnDraw() {
	if (!IsActive())
		return;

	Lock();

	Framehook::Draw(GetX(), GetY(), CHECK_BOX_SIZE, CHECK_BOX_SIZE, 0, BTFull);


	unsigned int drawColor = color;
	unsigned int checkColor = White;
	if (!IsEnabled()) {
		drawColor = checkColor = disabledColor;
	} else if (InRange(Hook::GetMouseX(), Hook::GetMouseY()) && GetHoverColor() != Disabled) {
		drawColor = hoverColor;
		checkColor = hoverColor;
	}

	if (IsChecked())
		Texthook::Draw(GetX() + 3, GetY() + CHECK_LABEL_TOP, false, 0,
			(TextColor)checkColor, "X");

	Texthook::Draw(GetX() + CHECK_BOX_SIZE + CHECK_LABEL_GAP,
		GetY() + CHECK_LABEL_TOP, false, 0, (TextColor)drawColor, text);
	Unlock();
}

/* OnLeftClick(bool up, unsigned int x, unsigned int y)
 *	Check if the text hook has been clicked on.
 */
bool Checkhook::OnLeftClick(bool up, unsigned int x, unsigned int y) {
	if (InRange(x,y)) {
		Lock();
		if (up)
			SetState(!IsChecked());
		if (GetLeftClickHandler())
			GetLeftClickHandler()(up, this, GetLeftClickVoid());
		Unlock();
		return true;
	}
	return false;
}

/* OnRightClick(bool up, unsigned int x, unsigned int y)
 *	Check if the text hook has been clicked on.
 */
bool Checkhook::OnRightClick(bool up, unsigned int x, unsigned int y) {
	if (InRange(x,y) && GetRightClickHandler()) {
		bool block = false;
		Lock();
		block = GetRightClickHandler()(up, this, GetRightClickVoid());
		Unlock();
		return block;
	}
	return false;
}