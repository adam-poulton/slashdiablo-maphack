#include "../Hook.h"
#include "../../D2Ptrs.h"
#include "UITab.h"
#include "../../BH.h"
#include "../Basic/Texthook/Texthook.h"
#include "../Basic/Framehook/Framehook.h"
#include "../Basic/Boxhook/Boxhook.h"

using namespace Drawing;

std::list<UI*> UI::UIs;

UI::UI(std::string name, unsigned int xSize, unsigned int ySize) {
	InitializeCriticalSection(&crit);
	// Start from a known state; the setters below read these back and windows
	// are constructed before the game has told us the screen size.
	x = y = 0;
	this->xSize = this->ySize = 0;
	active = minimized = dragged = visible = false;
	dragX = dragY = startX = startY = 0;
	resizable = resizing = false;
	resizeGrabX = resizeGrabY = 0;
	SetName(name);
	string path = BH::path + "UI.ini";
	// The size passed in is the default, which a window the user has resized
	// overrides. Like the position, it is taken as given here and brought into
	// range by EnsureInBounds() once there is a screen size to go by.
	SetXSize(GetPrivateProfileInt(name.c_str(), "XSize", xSize, path.c_str()));
	SetYSize(GetPrivateProfileInt(name.c_str(), "YSize", ySize, path.c_str()));
	int x = GetPrivateProfileInt(name.c_str(), "X", 0, path.c_str());
	SetX(x);
	int y = GetPrivateProfileInt(name.c_str(), "Y", 0, path.c_str());
	SetY(y);
	int minX = GetPrivateProfileInt(name.c_str(), "minimizedX", MINIMIZED_X_POS, path.c_str());
	SetMinimizedX(minX);
	// Stack the default positions by creation order so windows that have never
	// been moved don't sit on top of each other. Once saved to UI.ini the
	// position is used as-is, so a collapsed window never moves on its own.
	int minY = GetPrivateProfileInt(name.c_str(), "minimizedY",
		MINIMIZED_Y_POS - (int)(UIs.size() * (TITLE_BAR_HEIGHT + 4)), path.c_str());
	SetMinimizedY(minY);
	char activeStr[20];
	GetPrivateProfileString(name.c_str(), "Minimized", "true", activeStr, 20, path.c_str());
	// Set the initial state directly rather than through SetMinimized(), which
	// would write the config back out before the modules have finished loading.
	minimized = StringToBool(activeStr);
	SetActive(false);
	zOrder = UIs.size();
	UIs.push_back(this);
}
UI::~UI() {
	Lock();
	WritePrivateProfileString(name.c_str(), "X", to_string<unsigned int>(GetX()).c_str(), string(BH::path + "UI.ini").c_str());
	WritePrivateProfileString(name.c_str(), "Y", to_string<unsigned int>(GetY()).c_str(), string(BH::path + "UI.ini").c_str());
	WritePrivateProfileString(name.c_str(), "XSize", to_string<unsigned int>(GetXSize()).c_str(), string(BH::path + "UI.ini").c_str());
	WritePrivateProfileString(name.c_str(), "YSize", to_string<unsigned int>(GetYSize()).c_str(), string(BH::path + "UI.ini").c_str());
	WritePrivateProfileString(name.c_str(), "Minimized", to_string<bool>(IsMinimized()).c_str(), string(BH::path + "UI.ini").c_str());
	WritePrivateProfileString(name.c_str(), "minimizedX", to_string<unsigned int>(GetMinimizedX()).c_str(), string(BH::path + "UI.ini").c_str());
	WritePrivateProfileString(name.c_str(), "minimizedY", to_string<unsigned int>(GetMinimizedY()).c_str(), string(BH::path + "UI.ini").c_str());

	while(Tabs.size() > 0) {
		delete (*Tabs.begin());
	}
		
	UIs.remove(this);

	Unlock();
	DeleteCriticalSection(&crit);
}

// Positions and sizes are stored as given. Windows are constructed while BH is
// injecting, before the game has reported its resolution, so validating against
// the screen here threw away the position read back from UI.ini and left every
// window at the origin. EnsureInBounds() keeps them on screen instead, once
// there is a screen size to go by.
void UI::SetX(unsigned int newX) {
	Lock();
	x = newX;
	Unlock();
}

void UI::SetY(unsigned int newY) {
	Lock();
	y = newY;
	Unlock();
}

void UI::SetXSize(unsigned int newXSize) {
	Lock();
	xSize = newXSize;
	Unlock();
}

void UI::SetYSize(unsigned int newYSize) {
	Lock();
	ySize = newYSize;
	Unlock();
}

void UI::SetMinimizedX(unsigned int newX) {
	// If we limit by width injecting on load won't work
	// Just don't give invalid params :)
	// if (newX >= 0 && newX <= Hook::GetScreenWidth()) {
	if (newX >= 0) {
		Lock();
		minimizedX = newX;
		Unlock();
	}
}

void UI::SetMinimizedY(unsigned int newY) {
	// If we limit by height injecting on load won't work
	// Just don't give invalid params :)
	// if (newY >= 0 && newY <= Hook::GetScreenHeight()) {
	if (newY >= 0) {
		Lock();
		minimizedY = newY;
		Unlock();
	}
}

// Never larger than the screen, so a window saved on a big monitor can still be
// resized back on a small one.
unsigned int UI::GetMinXSize() {
	unsigned int screenWidth = Hook::GetScreenWidth();
	if (screenWidth > 0 && UI_MIN_WIDTH > screenWidth)
		return screenWidth;
	return UI_MIN_WIDTH;
}

unsigned int UI::GetMinYSize() {
	unsigned int screenHeight = Hook::GetScreenHeight();
	if (screenHeight > 0 && UI_MIN_HEIGHT > screenHeight)
		return screenHeight;
	return UI_MIN_HEIGHT;
}

void UI::SetResizing(bool state, bool write_file) {
	Lock();
	resizing = state;
	if (!state && write_file) {
		WritePrivateProfileString(name.c_str(), "XSize", to_string<unsigned int>(GetXSize()).c_str(), string(BH::path + "UI.ini").c_str());
		WritePrivateProfileString(name.c_str(), "YSize", to_string<unsigned int>(GetYSize()).c_str(), string(BH::path + "UI.ini").c_str());
	}
	Unlock();
}

void UI::SetResizing(bool state) {
	SetResizing(state, false);
}

// The corner follows the cursor, keeping hold of it wherever it was grabbed, and
// stops at the minimum size and at the edges of the screen.
void UI::DragResizeTo(unsigned int mouseX, unsigned int mouseY) {
	unsigned int screenWidth = Hook::GetScreenWidth();
	unsigned int screenHeight = Hook::GetScreenHeight();

	int left = (int)GetX(), top = (int)GetY();
	int newXSize = (int)mouseX + (int)resizeGrabX - left;
	int newYSize = (int)mouseY + (int)resizeGrabY - top;

	// Not past the edge of the screen, and never smaller than the minimum, in
	// that order so the minimum wins on a screen too small to hold it.
	if (screenWidth > 0 && left + newXSize > (int)screenWidth)
		newXSize = (int)screenWidth - left;
	if (screenHeight > 0 && top + newYSize > (int)screenHeight)
		newYSize = (int)screenHeight - top;
	if (newXSize < (int)GetMinXSize())
		newXSize = (int)GetMinXSize();
	if (newYSize < (int)GetMinYSize())
		newYSize = (int)GetMinYSize();

	SetXSize((unsigned int)newXSize);
	SetYSize((unsigned int)newYSize);
}

// Dots stacked into the corner, the usual sign that a corner can be dragged.
void UI::DrawResizeGrip() {
	if (!IsResizable())
		return;

	unsigned int right = GetX() + GetXSize(), bottom = GetY() + GetYSize();
	bool lit = resizing ||
		InResizeGrip((*p_D2CLIENT_MouseX), (*p_D2CLIENT_MouseY));

	for (unsigned int row = 0; row < 3; row++) {
		for (unsigned int col = 0; col + row < 3; col++) {
			Boxhook::Draw(right - 4 - (col * 3), bottom - 4 - (row * 3), 2, 2,
				lit ? 0x20 : 0xD0, BTNormal);
		}
	}
}

void UI::OnDraw() {
	if (!IsVisible()) return;
	EnsureInBounds();
	if (IsMinimized()) {
		int xSize = Texthook::GetTextSize(GetName(), 0).x + 8;

		if (IsDragged()) {
			int newX = (*p_D2CLIENT_MouseX) - dragX;
			int newY = (*p_D2CLIENT_MouseY) - dragY;

			if (newX < 0)
				newX = 0;

			if ((newX + xSize + 2) > (int)Hook::GetScreenWidth())
				newX = Hook::GetScreenWidth() - xSize - 2;

			if (newY < 2)
				newY = 2;

			if ((newY + TITLE_BAR_HEIGHT) > (int)Hook::GetScreenHeight())
				newY = Hook::GetScreenHeight() - TITLE_BAR_HEIGHT;

			*p_D2CLIENT_MouseX = newX + dragX;
			*p_D2CLIENT_MouseY = newY + dragY;
			SetMinimizedX(newX);
			SetMinimizedY(newY);
		}
		int yPos = GetMinimizedY();
		int inPos = InPos((*p_D2CLIENT_MouseX), (*p_D2CLIENT_MouseY), GetMinimizedX(), yPos, xSize, TITLE_BAR_HEIGHT);
		Framehook::Draw(GetMinimizedX(), yPos, xSize, TITLE_BAR_HEIGHT, 0, BTOneHalf);
		Texthook::Draw(GetMinimizedX() + 4, yPos + 3, false, 0, (inPos?Silver:White), GetName());
	} else {
		if (IsDragged()) {
			int newX = (*p_D2CLIENT_MouseX) - dragX;
			int newY = (*p_D2CLIENT_MouseY) - dragY;

			if (newX < 0)
				newX = 0;

			if ((newX + GetXSize() + 2) > Hook::GetScreenWidth())
				newX = Hook::GetScreenWidth() - GetXSize() - 2;

			if (newY < 2)
				newY = 2;

			if ((newY + GetYSize()) > Hook::GetScreenHeight())
				newY = Hook::GetScreenHeight() - GetYSize();

			*p_D2CLIENT_MouseX = newX + dragX;
			*p_D2CLIENT_MouseY = newY + dragY;
			SetX(newX);
			SetY(newY);
		}
		// There is no mouse move event to hang a resize off, so the corner
		// catches up with the cursor here, once per frame.
		if (IsResizing())
			DragResizeTo((*p_D2CLIENT_MouseX), (*p_D2CLIENT_MouseY));

		Framehook::Draw(GetX(), GetY(), GetXSize(), GetYSize(), 0, (IsActive()?BTNormal:BTOneHalf));
		Framehook::Draw(GetX(), GetY(), GetXSize(), TITLE_BAR_HEIGHT, 0, BTNormal);
		Texthook::Draw(GetX() + 4, GetY () + 3, false, 0, InTitle((*p_D2CLIENT_MouseX), (*p_D2CLIENT_MouseY))?Silver:White, GetName());
		for (list<UITab*>::iterator it = Tabs.begin(); it != Tabs.end(); it++)
			(*it)->OnDraw();
		// Last, so it sits over whatever the tab drew into the corner.
		DrawResizeGrip();
	}
}

void UI::EnsureInBounds() {
	unsigned int screenWidth = Hook::GetScreenWidth();
	unsigned int screenHeight = Hook::GetScreenHeight();

	// With no resolution to go by there is nothing meaningful to clamp against,
	// and guessing would move the window off its saved position.
	if (screenWidth == 0 || screenHeight == 0)
		return;

	if (IsMinimized()) {
		// A collapsed window is only as wide as its title bar, so clamping it
		// against the full window width would drag wide windows back off their
		// saved position every frame.
		unsigned int titleWidth = Texthook::GetTextSize(GetName(), 0).x + 8;
		if (titleWidth < screenWidth && GetMinimizedX() + titleWidth > screenWidth)
			SetMinimizedX(screenWidth - titleWidth);
		if (TITLE_BAR_HEIGHT < screenHeight && GetMinimizedY() + TITLE_BAR_HEIGHT > screenHeight)
			SetMinimizedY(screenHeight - TITLE_BAR_HEIGHT);
	}
	else {
		// A window resized on a bigger screen, or saved before a resolution
		// change, is brought back to something that fits before its position is
		// considered, since the position depends on the size. Only for windows
		// that can be resized at all: a fixed one is the size its contents were
		// laid out for and must be left alone.
		if (IsResizable()) {
			if (GetXSize() > screenWidth)
				SetXSize(screenWidth);
			if (GetYSize() > screenHeight)
				SetYSize(screenHeight);
			if (GetXSize() < GetMinXSize())
				SetXSize(GetMinXSize());
			if (GetYSize() < GetMinYSize())
				SetYSize(GetMinYSize());
		}

		// Only pull a window back if it would hang off the screen, and only if
		// it fits at all, so that the arithmetic can't wrap round.
		if (GetXSize() < screenWidth && GetX() + GetXSize() > screenWidth)
			SetX(screenWidth - GetXSize());
		if (GetYSize() < screenHeight && GetY() + GetYSize() > screenHeight)
			SetY(screenHeight - GetYSize());
	}
}

void UI::SetDragged(bool state, bool write_file) {
	Lock(); 
	dragged = state; 
	if (!state && write_file) {
		WritePrivateProfileString(name.c_str(), "X", to_string<unsigned int>(GetX()).c_str(), string(BH::path + "UI.ini").c_str());
		WritePrivateProfileString(name.c_str(), "Y", to_string<unsigned int>(GetY()).c_str(), string(BH::path + "UI.ini").c_str());
		WritePrivateProfileString(name.c_str(), "minimizedX", to_string<unsigned int>(GetMinimizedX()).c_str(), string(BH::path + "UI.ini").c_str());
		WritePrivateProfileString(name.c_str(), "minimizedY", to_string<unsigned int>(GetMinimizedY()).c_str(), string(BH::path + "UI.ini").c_str());
	}
	Unlock(); 
}

void UI::SetDragged(bool state) {
    SetDragged(state, false);
}

void UI::SetVisible(bool newState) {
	visible = newState;
}

void UI::SetMinimized(bool newState) {
	if (newState == minimized)
		return;
	Lock();
	if (newState)
		BH::config->Write();
	minimized = newState;
	WritePrivateProfileString(name.c_str(), "Minimized", to_string<bool>(newState).c_str(), string(BH::path + "UI.ini").c_str());
	Unlock(); 
};

bool UI::OnLeftClick(bool up, unsigned int mouseX, unsigned int mouseY) {
	// A window that isn't being drawn must not swallow the click.
	if (!IsVisible())
		return false;

	// Let go of the grip wherever the button comes up, since a resize usually
	// ends with the cursor away from the corner it started at.
	if (up && IsResizing()) {
		SetResizing(false, true);
		return true;
	}
	if (IsMinimized()) {
		int yPos = GetMinimizedY();
		int xSize = Texthook::GetTextSize(GetName(), 0).x + 8;
		int inPos = InPos((*p_D2CLIENT_MouseX), (*p_D2CLIENT_MouseY), GetMinimizedX(), yPos, xSize, TITLE_BAR_HEIGHT);
		if (inPos /*&& GetAsyncKeyState(VK_CONTROL)*/) 
		{
			if(GetAsyncKeyState(VK_CONTROL))
			{
				if (up) {
					SetMinimized(false);
					Sort(this);
				}
				return true;
			}
			else if (GetAsyncKeyState(VK_SHIFT) && !up) {
				SetDragged(true);
				dragX = mouseX - GetMinimizedX();
				dragY = mouseY - GetMinimizedY();
				startX = mouseX;
				startY = mouseY;
			}
			else
			{
				SetDragged(false, true);
				if(!up) {
					PrintText(7, "CTRL-click to open settings" );
					PrintText(7, "Shift-drag to move" );
				}
			}
			return true;
		}
	}
	// The grip sits inside the window, so it has to be offered the click before
	// the window itself takes it and starts a drag or a tab switch.
	if (InResizeGrip(mouseX, mouseY)) {
		SetActive(true);
		Sort(this);
		if (!up) {
			SetResizing(true);
			// Hold the corner wherever it was taken hold of, so the window
			// doesn't jump to put its corner under the cursor.
			resizeGrabX = (GetX() + GetXSize()) - mouseX;
			resizeGrabY = (GetY() + GetYSize()) - mouseY;
		}
		return true;
	}

	if (InTitle(mouseX, mouseY) && !IsMinimized()) {
		if (!up)
		{
			SetDragged(true);
			dragX = mouseX - GetX();
			dragY = mouseY - GetY();
			startX = mouseX;
			startY = mouseY;
		} 
		else
		{
			SetDragged(false, true);
			if( startX == mouseX && startY == mouseY && GetAsyncKeyState(VK_CONTROL) )
			{
				PrintText(135, "Right Click to Close" );
			}
		}
		SetActive(true);
		Sort(this);
		return true;
	} else if (InWindow(mouseX, mouseY) && !IsMinimized()) {
		SetActive(true);
		Sort(this);
		if (up) {
			for (list<UITab*>::iterator it = Tabs.begin(); it != Tabs.end(); it++) {
				if ((*it)->IsHovering(mouseX, mouseY)) {
					SetCurrentTab(*it);
					return true;
				}
			}
		}
		return true;
	}
	SetActive(false);
	SetDragged(false, false);
	SetResizing(false);
	return false;
}

bool UI::OnRightClick(bool up, unsigned int mouseX, unsigned int mouseY) {
	// A window that isn't being drawn must not swallow the click.
	if (!IsVisible())
		return false;
	if (InTitle(mouseX, mouseY) && !IsMinimized()) {
		if (up) 
			SetMinimized(true);
		return true;
	}
	return false;
}

void UI::Sort(UI* zero) {
	int zOrder = 1;
	for (list<UI*>::iterator it = UIs.begin(); it != UIs.end(); it++, zOrder++) {
		if ((*it) == zero) {
			(*it)->SetZOrder(0);
			zOrder--;
		} else {
			(*it)->SetZOrder(zOrder);
		}
	}
}

/* UI Helper functions 
 *	Static functions to aid in sending events to UIs.
 */
 
bool ZSortClick (UI* one, UI* two) {
	return one->GetZOrder() < two->GetZOrder();
}
bool ZSortDraw (UI* one, UI* two) {
	return one->GetZOrder() > two->GetZOrder();
}

void UI::Draw() {
	UIs.sort(ZSortDraw);
	for (list<UI*>::iterator it = UIs.begin(); it!=UIs.end(); ++it) {
			(*it)->Lock();
			(*it)->OnDraw();
			(*it)->Unlock();
	}
}	

bool UI::LeftClick(bool up, unsigned int mouseX, unsigned int mouseY) {
	UIs.sort(ZSortClick);
	for (list<UI*>::iterator it = UIs.begin(); it!=UIs.end(); ++it) {
		(*it)->Lock();
		if ((*it)->OnLeftClick(up, mouseX, mouseY)) {
			(*it)->Unlock();
			return true;
		}
		(*it)->Unlock();
	}
	return false;
}

bool UI::RightClick(bool up, unsigned int mouseX, unsigned int mouseY) {
	UIs.sort(ZSortClick);
	for (list<UI*>::iterator it = UIs.begin(); it!=UIs.end(); ++it) {
		(*it)->Lock();
		if ((*it)->OnRightClick(up, mouseX, mouseY)) {
			(*it)->Unlock();
			return true;
		}
		(*it)->Unlock();
	}
	return false;
}
