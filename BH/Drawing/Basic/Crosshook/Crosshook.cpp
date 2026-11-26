#include "Crosshook.h"
#include "../../../Common.h"
#include "../../../D2Ptrs.h"
#include "../../MapIcon/MapIcon.h"

using namespace std;
using namespace Drawing;

/* Basic Hook Initializer
 *		Used for just drawing basic crosses of screen.
 */
Crosshook::Crosshook(HookVisibility visiblity, unsigned int x, unsigned int y) :
Hook(visiblity, x, y) {
	//Set the extra variables
	SetColor(0);
}

/* Group Hook Initializer
 *		Used in conjuction with other basic hooks to create an advanced hook.
 */
Crosshook::Crosshook(HookGroup* group, unsigned int x, unsigned int y) :
Hook(group, x, y) {
	//Set the extra variables
	SetColor(0);
}

/* GetColor()
 *	Returns the color of the crosshook.
 */
unsigned int Crosshook::GetColor() {
	return color;
}

/* SetColor(unsigned int color)
 *	Sets the color of the crosshook.
 */
void Crosshook::SetColor(unsigned int newColor) {
	if (newColor < 0 || newColor > 255)
		return;
	Lock();
	color = newColor;
	Unlock();
}

/* Get(X/Y)Size()
 *	Used by click handlers, fuck clicking crosss.
 */
unsigned int Crosshook::GetXSize() { return 0; };
unsigned int Crosshook::GetYSize() { return 0; };


/* OnDraw()
 *	Draws the cross
 */
void Crosshook::OnDraw() {
	if (!IsActive())
		return;
	
	Lock();
	Draw(GetX(), GetY(), GetColor());
	Unlock();
}

bool Crosshook::Draw(unsigned int x, unsigned int y, unsigned int color) {
	MapIcon::Draw(MapIcon::Cross, MONSTER_CROSS_SIZE, x, y, color);
	return true;
}