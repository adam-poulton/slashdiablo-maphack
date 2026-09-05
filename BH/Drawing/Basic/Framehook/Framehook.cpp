#include "Framehook.h"
#include "../../../Common.h"
#include "../../../D2Ptrs.h"

using namespace Drawing;

namespace {

// The ornate border is D2Client's own, drawn from the panel art a game loads.
// Outside a game there is no such art and the call reads it anyway, so it is
// only made where there is a game to have loaded it.
bool GameBorderAvailable() {
	return D2CLIENT_GetPlayerUnit() != NULL;
}

// What is drawn in its place: the same rectangle, edged a pixel at a time. A
// window on the login screen still reads as a window, without the flourish.
void DrawPlainEdges(unsigned int x, unsigned int y, unsigned int xSize,
		unsigned int ySize, BoxTrans trans) {
	if (xSize == 0 || ySize == 0)
		return;
	const unsigned int edge = 0xD0;
	D2GFX_DrawRectangle(x, y, x + xSize, y + 1, edge, trans);
	D2GFX_DrawRectangle(x, y + ySize - 1, x + xSize, y + ySize, edge, trans);
	D2GFX_DrawRectangle(x, y, x + 1, y + ySize, edge, trans);
	D2GFX_DrawRectangle(x + xSize - 1, y, x + xSize, y + ySize, edge, trans);
}

}  // namespace

void Framehook::DrawBorder(unsigned int x, unsigned int y, unsigned int xSize,
		unsigned int ySize, BoxTrans trans) {
	if (GameBorderAvailable()) {
		RECT pRect = { static_cast<long>(x), static_cast<long>(y),
			static_cast<long>(x + xSize), static_cast<long>(y + ySize) };
		Framehook::DrawRectStub(&pRect);
		return;
	}
	DrawPlainEdges(x, y, xSize, ySize, trans);
}

/* Basic Hook Initializer
 *		Used for just drawing basic framees on screen.
 */
Framehook::Framehook(HookVisibility visiblity, unsigned int x, unsigned int y, unsigned int xSize, unsigned int ySize) :
Hook(visiblity, x, y) {
	//Set the extra variables
	SetXSize(xSize);
	SetYSize(ySize);
	SetColor(0);
	SetTransparency(BTFull);
}

/* Group Hook Initializer
 *		Used in conjuction with other basic hooks to create an advanced hook.
 */
Framehook::Framehook(HookGroup* group, unsigned int x, unsigned int y, unsigned int xSize, unsigned int ySize) :
Hook(group, x, y) {
	//Set the extra variables
	SetXSize(xSize);
	SetYSize(ySize);
	SetColor(0);
	SetTransparency(BTFull);
}

/* GetColor()
 *	Returns the color of the framehook.
 */
unsigned int Framehook::GetColor() {
	return color;
}

/* SetColor(unsigned int color)
 *	Sets the color of the framehook.
 */
void Framehook::SetColor(unsigned int newColor) {
	if (newColor < 0 || newColor > 255)
		return;
	Lock();
	color = newColor;
	Unlock();
}

/* GetXSize()
 *	Returns the width of the frame hook.
 */
unsigned int Framehook::GetXSize() {
	return xSize;
}

/* SetXSize(unsigned int newX)
 *	Sets the new width of the framehook.
 */
void Framehook::SetXSize(unsigned int newX) {
	Lock();
	xSize = newX;
	Unlock();
}

/* GetYSize()
 *	Returns the height of the frame hook.
 */
unsigned int Framehook::GetYSize() {
	return ySize;
}

/* SetYSize(unsigned int ySize)
 *	Sets the height of the frame hook.
 */
void Framehook::SetYSize(unsigned int newY) {
	Lock();
	ySize = newY;
	Unlock();
}

/* GetTransparency()
 *	Returns the transparency of the frame hook.
 */
BoxTrans Framehook::GetTransparency() {
	return transparency;
}

/* SetTransparency(BoxTrans trans)
 *	Sets the transparency of the frame hook.
 */
void Framehook::SetTransparency(BoxTrans trans) {
	Lock();
	transparency = trans;
	Unlock();
}

DWORD __declspec(naked) _fastcall Framehook::DrawRectStub(RECT *pRect) {
	__asm
	{
		mov eax, ecx
		jmp D2CLIENT_DrawRectFrame
	}
}

/* OnDraw()
 *	Draws the rectangle
 */
void Framehook::OnDraw() {
	if (!IsActive())
		return;

	Lock();
	D2GFX_DrawRectangle(GetX(), GetY(), GetX() + GetXSize(), GetY() + GetYSize(), GetColor(), GetTransparency());
	Framehook::DrawBorder(GetX(), GetY(), GetXSize(), GetYSize(), GetTransparency());
	Unlock();
}

/* OnLeftClick(bool up, unsigned int x, unsigned int y)
 *	Check if the Frame hook has been clicked on.
 */
bool Framehook::OnLeftClick(bool up, unsigned int x, unsigned int y) {
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
 *	Check if the Frame hook has been clicked on.
 */
bool Framehook::OnRightClick(bool up, unsigned int x, unsigned int y) {
	if (InRange(x,y) && GetRightClickHandler()) {
		bool block = false;
		Lock();
		block = GetRightClickHandler()(up, this, GetRightClickVoid());
		Unlock();
		return block;
	}
	return false;
}

bool Framehook::Draw(unsigned int x, unsigned int y, unsigned int xSize, unsigned int ySize, unsigned int color, BoxTrans trans) {
	D2GFX_DrawRectangle(x, y, x + xSize, y + ySize, color, trans);
	Framehook::DrawBorder(x, y, xSize, ySize, trans);
	return true;
}