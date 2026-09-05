#include "../Hook.h"
#include "../../D2Ptrs.h"
#include "UITab.h"
#include "../../BH.h"
#include "../Basic/Texthook/Texthook.h"
#include "../Basic/Framehook/Framehook.h"
#include "../Basic/Boxhook/Boxhook.h"
#include "../Advanced/Inputhook/Inputhook.h"

using namespace Drawing;

std::list<UI*> UI::UIs;

// UIChrome sits on the window's content box: the full width less the margin down
// either side, measured from the top of the window so that a band can be placed
// by its distance from there.
unsigned int UIChrome::GetX() { return ui->GetX() + UI_CONTENT_MARGIN; }
unsigned int UIChrome::GetY() { return ui->GetY(); }

unsigned int UIChrome::GetXSize() {
	unsigned int width = ui->GetXSize();
	return (width > 2 * UI_CONTENT_MARGIN) ? (width - (2 * UI_CONTENT_MARGIN)) : 0;
}

unsigned int UIChrome::GetYSize() { return ui->GetYSize(); }

// Deliberately not the window's own IsActive(), which means "has focus". The
// chrome has to keep taking input while the window is merely unfocused, and to
// stop taking it while the window is collapsed or hidden.
bool UIChrome::IsActive() { return ui->IsVisible() && !ui->IsMinimized(); }

// The chrome is on whichever screen its window is.
HookVisibility UIChrome::GetVisibility() { return ui->GetVisibility(); }

UI::UI(std::string name, unsigned int xSize, unsigned int ySize) :
		UI(name, name, xSize, ySize) {
}

UI::UI(std::string name, std::string configKey, unsigned int xSize, unsigned int ySize) {
	InitializeCriticalSection(&crit);
	// Start from a known state; the setters below read these back and windows
	// are constructed before the game has told us the screen size.
	x = y = 0;
	this->xSize = this->ySize = 0;
	active = minimized = dragged = visible = false;
	visibility = InGame;
	dragX = dragY = startX = startY = 0;
	resizable = resizing = false;
	resizeGrabX = resizeGrabY = 0;
	minXOverride = minYOverride = 0;
	askedXSize = xSize;
	askedYSize = ySize;
	sizeResolved = false;
	searchBox = NULL;
	footerLeft = footerRight = footerAction = NULL;
	chromeWidth = chromeHeight = 0;
	chrome = new UIChrome(this);
	SetName(name);
	this->configKey = configKey;
	string path = BH::path + "UI.ini";
	// A remembered size overrides the one asked for. Read with a default of zero
	// so that no remembered size can be told apart from one that happens to
	// equal the default: with none, ResolveDefaultSize() works one out from the
	// canvas once there is a canvas to work from.
	unsigned int savedXSize = GetPrivateProfileInt(configKey.c_str(), "XSize", 0, path.c_str());
	unsigned int savedYSize = GetPrivateProfileInt(configKey.c_str(), "YSize", 0, path.c_str());
	sizeRemembered = (savedXSize > 0 && savedYSize > 0);
	SetXSize(sizeRemembered ? savedXSize : xSize);
	SetYSize(sizeRemembered ? savedYSize : ySize);
	int x = GetPrivateProfileInt(configKey.c_str(), "X", 0, path.c_str());
	SetX(x);
	int y = GetPrivateProfileInt(configKey.c_str(), "Y", 0, path.c_str());
	SetY(y);
	int minX = GetPrivateProfileInt(configKey.c_str(), "minimizedX", MINIMIZED_X_POS, path.c_str());
	SetMinimizedX(minX);
	// Stack the default positions by creation order so windows that have never
	// been moved don't sit on top of each other. Once saved to UI.ini the
	// position is used as-is, so a collapsed window never moves on its own.
	int minY = GetPrivateProfileInt(configKey.c_str(), "minimizedY",
		MINIMIZED_Y_POS - (int)(UIs.size() * (TITLE_BAR_HEIGHT + 4)), path.c_str());
	SetMinimizedY(minY);
	char activeStr[20];
	GetPrivateProfileString(configKey.c_str(), "Minimized", "true", activeStr, 20, path.c_str());
	// Set the initial state directly rather than through SetMinimized(), which
	// would write the config back out before the modules have finished loading.
	minimized = StringToBool(activeStr);
	SetActive(false);
	zOrder = UIs.size();
	UIs.push_back(this);
}
UI::~UI() {
	Lock();
	WritePrivateProfileString(configKey.c_str(), "X", to_string<unsigned int>(GetX()).c_str(), string(BH::path + "UI.ini").c_str());
	WritePrivateProfileString(configKey.c_str(), "Y", to_string<unsigned int>(GetY()).c_str(), string(BH::path + "UI.ini").c_str());
	WritePrivateProfileString(configKey.c_str(), "XSize", to_string<unsigned int>(GetXSize()).c_str(), string(BH::path + "UI.ini").c_str());
	WritePrivateProfileString(configKey.c_str(), "YSize", to_string<unsigned int>(GetYSize()).c_str(), string(BH::path + "UI.ini").c_str());
	WritePrivateProfileString(configKey.c_str(), "Minimized", to_string<bool>(IsMinimized()).c_str(), string(BH::path + "UI.ini").c_str());
	WritePrivateProfileString(configKey.c_str(), "minimizedX", to_string<unsigned int>(GetMinimizedX()).c_str(), string(BH::path + "UI.ini").c_str());
	WritePrivateProfileString(configKey.c_str(), "minimizedY", to_string<unsigned int>(GetMinimizedY()).c_str(), string(BH::path + "UI.ini").c_str());

	while(Tabs.size() > 0) {
		delete (*Tabs.begin());
	}

	// The bands the window drew for itself. Each hook takes itself back out of
	// chrome->Hooks as it goes, so the list is walked from the front each time
	// rather than iterated.
	while (chrome->Hooks.size() > 0)
		delete (*chrome->Hooks.begin());
	searchBox = NULL;
	footerLeft = footerRight = footerAction = NULL;
	delete chrome;
	chrome = NULL;
		
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

void UI::SetMinSize(unsigned int minX, unsigned int minY) {
	Lock();
	minXOverride = minX;
	minYOverride = minY;
	Unlock();
}

void UI::SetFixedSize(unsigned int width, unsigned int height) {
	SetResizable(false);
	Lock();
	xSize = width;
	ySize = height;
	// Nothing left to resolve: the size is the one asked for, whatever the canvas
	// turns out to be and whatever UI.ini remembers.
	askedXSize = width;
	askedYSize = height;
	sizeResolved = true;
	sizeRemembered = false;
	Unlock();
}

// Never larger than the screen, so a window saved on a big monitor can still be
// resized back on a small one.
unsigned int UI::GetMinXSize() {
	unsigned int wanted = minXOverride ? minXOverride : UI_MIN_WIDTH;
	unsigned int screenWidth = Hook::GetScreenWidth();
	if (screenWidth > 0 && wanted > screenWidth)
		return screenWidth;
	return wanted;
}

// Enough for the chrome the window actually draws plus a usable amount of panel
// below it, rather than a fixed number: a window with a search band and a footer
// needs more room before its panel has any height at all.
unsigned int UI::GetMinYSize() {
	unsigned int wanted = minYOverride ? minYOverride :
		(GetChromeAboveHeight() + GetChromeBelowHeight() + UI_MIN_CONTENT_HEIGHT);
	unsigned int screenHeight = Hook::GetScreenHeight();
	if (screenHeight > 0 && wanted > screenHeight)
		return screenHeight;
	return wanted;
}

// A window UI.ini had no size for takes a share of the canvas, the first time
// there is a canvas to take a share of. Only ever larger than the size it asked
// for, and only for a window that can be resized at all: a fixed window is the
// size its contents were laid out for.
void UI::ResolveDefaultSize() {
	if (sizeResolved)
		return;
	unsigned int screenWidth = Hook::GetScreenWidth();
	unsigned int screenHeight = Hook::GetScreenHeight();
	if (screenWidth == 0 || screenHeight == 0)
		return;

	sizeResolved = true;
	if (sizeRemembered || !IsResizable())
		return;

	unsigned int wide = (screenWidth * UI_DEFAULT_WIDTH_PCT) / 100;
	unsigned int high = (screenHeight * UI_DEFAULT_HEIGHT_PCT) / 100;
	if (wide < askedXSize)
		wide = askedXSize;
	if (high < askedYSize)
		high = askedYSize;
	SetXSize(wide);
	SetYSize(high);
}

unsigned int UI::GetSearchBandHeight() {
	if (!searchBox)
		return 0;
	return SEARCH_BAND_TOP + searchBox->GetYSize() + SEARCH_BAND_GAP;
}

unsigned int UI::GetFooterBandHeight() {
	if (!footerLeft)
		return 0;
	return FOOTER_BAND_GAP + FOOTER_BAND_HEIGHT + UI_CONTENT_MARGIN;
}

void UI::EnableSearch(std::string placeholder) {
	Lock();
	if (!searchBox) {
		searchBox = new Inputhook(chrome, 0,
			TITLE_BAR_HEIGHT + TAB_HEIGHT + SEARCH_BAND_TOP, 0, "");
		// Selecting rather than clearing, so coming back to the box leaves the
		// previous query readable until it is typed over.
		searchBox->SetSelectOnFocus(true);
	}
	searchBox->SetPlaceholder(placeholder);
	chromeWidth = 0;	// so the next draw places it against the current size
	Unlock();
}

void UI::SetSearchPlaceholder(std::string placeholder) {
	if (searchBox)
		searchBox->SetPlaceholder(placeholder);
}

void UI::EnableFooter() {
	Lock();
	if (!footerLeft) {
		footerLeft = new Texthook(chrome, 0, 0, "");
		footerLeft->SetColor(Grey);
		footerRight = new Texthook(chrome, 0, 0, "");
		footerRight->SetColor(Grey);
		// Laid out from the far end of the content box, which the corrected
		// grouped Right alignment puts on the margin rather than on the frame.
		footerRight->SetAlignment(Right);
		// Held clear of the corner, where the resize grip is drawn over anything
		// that reaches it.
		footerRight->SetBaseX(RESIZE_GRIP_SIZE);
		chromeWidth = 0;
	}
	Unlock();
}

// Fired by the footer's clickable line. A plain function with the window as its
// context, since the hook callbacks predate anything that could carry a closure.
static bool FooterActionClicked(bool up, Hook* hook, void* context) {
	if (up && context)
		((UI*)context)->InvokeFooterAction();
	return true;
}

void UI::SetFooterAction(std::string text, std::function<void()> onClick) {
	// Nothing to put it in until there is a footer band.
	if (!footerLeft)
		return;

	Lock();
	onFooterAction = onClick;
	if (!footerAction) {
		footerAction = new Texthook(chrome, 0, 0, "");
		footerAction->SetColor(Gold);
		footerAction->SetHoverColor(Tan);
		footerAction->SetLeftCallback(FooterActionClicked, this);
		chromeWidth = 0;	// so the next draw places it
	}

	if (text.length() > 0) {
		footerAction->SetText("%s", text.c_str());
		footerAction->SetActive(true);
	} else {
		// Switched off rather than merely emptied, so it stops taking clicks too.
		footerAction->SetText("");
		footerAction->SetActive(false);
	}
	Unlock();
}

void UI::InvokeFooterAction() {
	if (onFooterAction)
		onFooterAction();
}

// The text is passed as an argument rather than as the format string: a footer
// carries whatever a panel has to say, which may well contain a percent sign.
void UI::SetFooterLeft(std::string text) {
	if (footerLeft)
		footerLeft->SetText("%s", text.c_str());
}

void UI::SetFooterRight(std::string text) {
	if (footerRight)
		footerRight->SetText("%s", text.c_str());
}

// Only when the window has changed size, since placing the bands measures text.
void UI::LayoutChrome() {
	if (!chrome)
		return;
	if (chromeWidth == GetXSize() && chromeHeight == GetYSize())
		return;
	chromeWidth = GetXSize();
	chromeHeight = GetYSize();

	if (searchBox)
		searchBox->SetXSize(chrome->GetXSize());

	if (footerLeft) {
		unsigned int footerY = (chromeHeight > FOOTER_BAND_HEIGHT + UI_CONTENT_MARGIN) ?
			(chromeHeight - FOOTER_BAND_HEIGHT - UI_CONTENT_MARGIN) : 0;
		footerLeft->SetBaseY(footerY);
		footerRight->SetBaseY(footerY);
		// Held clear of the corner only where there is a grip drawn over it. A
		// fixed window has none, and the inset would read as a ragged right edge.
		footerRight->SetBaseX(IsResizable() ? RESIZE_GRIP_SIZE : 0);
		if (footerAction) {
			// After what the window says about itself, whose width does not change.
			footerAction->SetBaseX(footerLeft->GetXSize() + FOOTER_ACTION_GAP);
			footerAction->SetBaseY(footerY);
		}
	}
}

// Each hook checks for itself whether it should be drawn, as the hooks of a tab
// do, so a band that is switched off costs nothing here.
void UI::DrawChrome() {
	if (!chrome)
		return;
	DrawFooterRule();
	for (list<Hook*>::iterator it = chrome->Hooks.begin(); it != chrome->Hooks.end(); it++)
		(*it)->OnDraw();
}

// A line between the panel and the footer, so that what the window says about
// itself does not read as one more row of whatever the panel is listing.
void UI::DrawFooterRule() {
	if (!footerLeft)
		return;
	unsigned int ruleY = GetY() + GetYSize() - GetFooterBandHeight();
	Boxhook::Draw(chrome->GetX(), ruleY, chrome->GetXSize(), 1, Grey, BTNormal);
}

void UI::SetResizing(bool state, bool write_file) {
	Lock();
	resizing = state;
	if (!state && write_file) {
		WritePrivateProfileString(configKey.c_str(), "XSize", to_string<unsigned int>(GetXSize()).c_str(), string(BH::path + "UI.ini").c_str());
		WritePrivateProfileString(configKey.c_str(), "YSize", to_string<unsigned int>(GetYSize()).c_str(), string(BH::path + "UI.ini").c_str());
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
		InResizeGrip(Hook::GetMouseX(), Hook::GetMouseY());

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
			int newX = Hook::GetMouseX() - dragX;
			int newY = Hook::GetMouseY() - dragY;
			int screenWidth = (int)Hook::GetScreenWidth();
			int screenHeight = (int)Hook::GetScreenHeight();

			if (newX < 0)
				newX = 0;

			if (screenWidth > 0 && (newX + xSize + 2) > screenWidth)
				newX = screenWidth - xSize - 2;

			if (newY < 2)
				newY = 2;

			if (screenHeight > 0 && (newY + TITLE_BAR_HEIGHT) > screenHeight)
				newY = screenHeight - TITLE_BAR_HEIGHT;

			Hook::SetMousePosition(newX + dragX, newY + dragY);
			SetMinimizedX(newX);
			SetMinimizedY(newY);
		}
		int yPos = GetMinimizedY();
		int inPos = InPos(Hook::GetMouseX(), Hook::GetMouseY(), GetMinimizedX(), yPos, xSize, TITLE_BAR_HEIGHT);
		Framehook::Draw(GetMinimizedX(), yPos, xSize, TITLE_BAR_HEIGHT, 0, BTOneHalf);
		Texthook::Draw(GetMinimizedX() + 4, yPos + 3, false, 0, (inPos?Silver:White), GetName());
	} else {
		if (IsDragged()) {
			int newX = Hook::GetMouseX() - dragX;
			int newY = Hook::GetMouseY() - dragY;
			int screenWidth = (int)Hook::GetScreenWidth();
			int screenHeight = (int)Hook::GetScreenHeight();

			if (newX < 0)
				newX = 0;

			if (screenWidth > 0 && (newX + (int)GetXSize() + 2) > screenWidth)
				newX = screenWidth - (int)GetXSize() - 2;

			if (newY < 2)
				newY = 2;

			if (screenHeight > 0 && (newY + (int)GetYSize()) > screenHeight)
				newY = screenHeight - (int)GetYSize();

			Hook::SetMousePosition(newX + dragX, newY + dragY);
			SetX(newX);
			SetY(newY);
		}
		// There is no mouse move event to hang a resize off, so the corner
		// catches up with the cursor here, once per frame.
		if (IsResizing())
			DragResizeTo(Hook::GetMouseX(), Hook::GetMouseY());

		LayoutChrome();
		Framehook::Draw(GetX(), GetY(), GetXSize(), GetYSize(), 0, (IsActive()?BTNormal:BTOneHalf));
		Framehook::Draw(GetX(), GetY(), GetXSize(), TITLE_BAR_HEIGHT, 0, BTNormal);
		Texthook::Draw(GetX() + 4, GetY () + 3, false, 0, InTitle(Hook::GetMouseX(), Hook::GetMouseY())?Silver:White, GetName());
		for (list<UITab*>::iterator it = Tabs.begin(); it != Tabs.end(); it++)
			(*it)->OnDraw();
		DrawChrome();
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

	// There is a canvas to measure against now, so a window that had no
	// remembered size can be given one.
	ResolveDefaultSize();

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
		WritePrivateProfileString(configKey.c_str(), "X", to_string<unsigned int>(GetX()).c_str(), string(BH::path + "UI.ini").c_str());
		WritePrivateProfileString(configKey.c_str(), "Y", to_string<unsigned int>(GetY()).c_str(), string(BH::path + "UI.ini").c_str());
		WritePrivateProfileString(configKey.c_str(), "minimizedX", to_string<unsigned int>(GetMinimizedX()).c_str(), string(BH::path + "UI.ini").c_str());
		WritePrivateProfileString(configKey.c_str(), "minimizedY", to_string<unsigned int>(GetMinimizedY()).c_str(), string(BH::path + "UI.ini").c_str());
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
	// Whatever the owner wants done at this point, which for the window that owns
	// the settings is writing them back out. A drawing class has no business
	// knowing that.
	if (newState && onMinimized)
		onMinimized();
	minimized = newState;
	WritePrivateProfileString(configKey.c_str(), "Minimized", to_string<bool>(newState).c_str(), string(BH::path + "UI.ini").c_str());
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
		int inPos = InPos(Hook::GetMouseX(), Hook::GetMouseY(), GetMinimizedX(), yPos, xSize, TITLE_BAR_HEIGHT);
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

// Drawn once per screen, and only the windows that belong to the screen being
// drawn. Called from the in game draw and from the out of game one both, which
// is what lets a window be laid out on the login screen.
void UI::Draw(HookVisibility screen) {
	UIs.sort(ZSortDraw);
	for (list<UI*>::iterator it = UIs.begin(); it!=UIs.end(); ++it) {
			(*it)->Lock();
			if ((*it)->GetVisibility() == screen)
				(*it)->OnDraw();
			(*it)->Unlock();
	}
}	

bool UI::LeftClick(HookVisibility screen, bool up, unsigned int mouseX, unsigned int mouseY) {
	UIs.sort(ZSortClick);
	for (list<UI*>::iterator it = UIs.begin(); it!=UIs.end(); ++it) {
		(*it)->Lock();
		if ((*it)->GetVisibility() != screen) {
			(*it)->Unlock();
			continue;
		}
		if ((*it)->OnLeftClick(up, mouseX, mouseY)) {
			(*it)->Unlock();
			return true;
		}
		(*it)->Unlock();
	}
	return false;
}

bool UI::RightClick(HookVisibility screen, bool up, unsigned int mouseX, unsigned int mouseY) {
	UIs.sort(ZSortClick);
	for (list<UI*>::iterator it = UIs.begin(); it!=UIs.end(); ++it) {
		(*it)->Lock();
		if ((*it)->GetVisibility() != screen) {
			(*it)->Unlock();
			continue;
		}
		if ((*it)->OnRightClick(up, mouseX, mouseY)) {
			(*it)->Unlock();
			return true;
		}
		(*it)->Unlock();
	}
	return false;
}
