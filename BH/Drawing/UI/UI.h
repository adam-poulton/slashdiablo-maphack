#pragma once

#include <Windows.h>
#include <functional>
#include <string>
#include <list>
#include "../Hook.h"

namespace Drawing {
	class UI;
	class UITab;
	class Inputhook;
	class Texthook;

	// Window chrome: the title bar across the top, the row of tab headings under
	// it, an optional search box below that, and an optional footer line along
	// the bottom. A tab measures itself against the window less all four.
	#define TITLE_BAR_HEIGHT 15
	#define TAB_HEIGHT 13
	#define MINIMIZED_Y_POS 585
	#define MINIMIZED_X_POS 234

	// The inset kept down either side of a window's contents, and the spacing of
	// the search and footer bands. Every panel of the Info window held its own
	// copies of these same numbers before the window owned the bands.
	#define UI_CONTENT_MARGIN	6
	#define SEARCH_BAND_TOP		3	// tab row to search box
	#define SEARCH_BAND_GAP		7	// search box to contents
	#define FOOTER_BAND_GAP		6	// contents to footer line
	#define FOOTER_BAND_HEIGHT	8	// the footer line itself
	#define FOOTER_ACTION_GAP	10	// between what the window says and what it offers

	// A window that has never been sized takes a share of the canvas rather than
	// a fixed number of pixels. What the game reports is its own render
	// resolution, which a resolution mod raises and a glide wrapper merely
	// upscales, so there is no one sensible pixel default. Never smaller than the
	// size the window asked for.
	#define UI_DEFAULT_WIDTH_PCT	25
	#define UI_DEFAULT_HEIGHT_PCT	40

	// The grip in the bottom right corner that resizes the window, and how small
	// a window may be dragged. The minimum has to leave room below the chrome for
	// a tab to have any height at all, since a tab measures itself against the
	// window. A window may raise its own: one minimum for every window is how two
	// windows with different contents come to disagree about it.
	#define RESIZE_GRIP_SIZE		12
	#define UI_MIN_WIDTH			350
	#define UI_MIN_CONTENT_HEIGHT	60

	// The bands a window draws around its panels. A group of their own rather
	// than the window itself, because UI::IsActive() means "has focus": the
	// search box has to keep taking input while the window is merely unfocused,
	// and has to stop taking it while the window is collapsed or hidden.
	//
	// The group's box is the window's content area, inset by the margin, so a
	// hook aligned Right within it lands on the margin rather than flush against
	// the frame.
	class UIChrome : public HookGroup {
		private:
			UI* ui;
		public:
			UIChrome(UI* owner) : ui(owner) {};

			unsigned int GetX();
			unsigned int GetY();
			unsigned int GetXSize();
			unsigned int GetYSize();
			bool IsActive();
	};

	class UI : public HookGroup {
		private:
			static std::list<UI*> UIs;
			unsigned int x, y, xSize, ySize, zOrder;//Position and Size and Order
			unsigned int minimizedX, minimizedY;//Position when minimized
			bool active, minimized, dragged, visible;//If UI is active or minimized or dragged
			unsigned int dragX, dragY;//Position where we grabbed it.
			unsigned int startX, startY;//Position where we grabbed it.
			bool resizable;//Whether the window offers a resize grip at all
			bool resizing;//Corner grip held by the mouse
			unsigned int resizeGrabX, resizeGrabY;//Corner to grab point, in pixels
			std::string name;//Name of the UI, as drawn in its title bar
			std::string configKey;//Section of UI.ini the window is remembered under
			UITab* currentTab;//Current tab open at the time.
			CRITICAL_SECTION crit;//Critical section

			// Smallest this particular window may be dragged to, or 0 to take the
			// shared default.
			unsigned int minXOverride, minYOverride;

			// The size asked for at construction, and whether UI.ini had one to
			// override it. Without a remembered size the default is worked out
			// from the canvas, which is not known until the game reports it.
			unsigned int askedXSize, askedYSize;
			bool sizeRemembered, sizeResolved;

			// The optional bands. Each is NULL until switched on, and a band that
			// is off takes no height, so a window wanting neither is laid out
			// exactly as it always was.
			UIChrome* chrome;
			Inputhook* searchBox;
			Texthook* footerLeft;
			Texthook* footerRight;

			// Something in the footer the user can click, and what to do about it.
			// A callback rather than the call, so a drawing class needs to know
			// nothing about what the window it belongs to is for.
			Texthook* footerAction;
			std::function<void()> onFooterAction;


			// Size the bands were last placed against, so a resize is noticed.
			unsigned int chromeWidth, chromeHeight;

			// Whatever should happen when the window is collapsed - writing the
			// settings back out, for the window that owns them. A callback rather
			// than the call itself, so a drawing class need know nothing about
			// configuration.
			std::function<void()> onMinimized;

			void EnsureInBounds();

			// Takes a share of the canvas the first time there is a canvas to take
			// a share of, for a window UI.ini had no size for.
			void ResolveDefaultSize();

			// Places the bands against the current size, and draws them. Both
			// measure text, so both belong on the draw thread.
			void LayoutChrome();
			void DrawChrome();

			// Follows the mouse while the grip is held, and draws the grip.
			void DragResizeTo(unsigned int mouseX, unsigned int mouseY);
			void DrawResizeGrip();
		public:
			std::list<UITab*> Tabs;

			// The name is what the title bar reads; the config key is the section
			// of UI.ini the geometry is remembered under. They are told apart so a
			// window can be titled whatever suits it without its remembered
			// position being lost every time that title changes.
			UI(std::string name, std::string configKey, unsigned int xSize, unsigned int ySize);
			UI(std::string name, unsigned int xSize, unsigned int ySize);
			~UI();

			void Lock() { EnterCriticalSection(&crit); };
			void Unlock() { LeaveCriticalSection(&crit); };

			unsigned int GetX() { return x; };
			unsigned int GetY() { return y; };
			unsigned int GetXSize() { return xSize; };
			unsigned int GetYSize() { return ySize; };
			unsigned int GetMinimizedX() { return minimizedX; };
			unsigned int GetMinimizedY() { return minimizedY; };
			bool IsActive() { return active; };
			bool IsMinimized() { return minimized; };
			bool IsDragged() { return dragged; };
			bool IsVisible() { return visible; };
			std::string GetName() { return name; };
			const std::string& GetConfigKey() { return configKey; };
			unsigned int GetZOrder() { return zOrder; };

			void SetX(unsigned int newX);
			void SetY(unsigned int newY);
			void SetXSize(unsigned int newXSize);
			void SetYSize(unsigned int newYSize);
			void SetMinimizedX(unsigned int newX);
			void SetMinimizedY(unsigned int newY);
			void SetActive(bool newState) { Lock(); active = newState; Unlock(); };
			void SetMinimized(bool newState);
			void SetVisible(bool newState);
			void SetName(std::string newName) { Lock(); name = newName;  Unlock(); };
			void SetDragged(bool state, bool write_file); // only write config to file if write_file is true
			void SetDragged(bool state); // never writes the config file
			void SetZOrder(unsigned int newZ) { Lock(); zOrder = newZ; Unlock(); };

			// Called when the window is collapsed, for whatever the owner wants
			// done at that point.
			void SetOnMinimized(std::function<void()> callback) { Lock(); onMinimized = callback; Unlock(); };

			UITab* GetActiveTab() { if (!currentTab) { currentTab = (*Tabs.begin()); } return currentTab; };
			void SetCurrentTab(UITab* tab) { Lock(); currentTab = tab; Unlock(); };

			// The search band. Switched on once, by whoever owns the window; the
			// box belongs to the window so every panel searches through the same
			// control rather than building one of its own.
			void EnableSearch(std::string placeholder);
			bool HasSearch() { return searchBox != NULL; };
			Inputhook* GetSearchBox() { return searchBox; };
			void SetSearchPlaceholder(std::string placeholder);
			unsigned int GetSearchBandHeight();

			// The footer band. Two lines sharing one strip: whatever the window
			// has to say about itself on the left, whatever the panel in front has
			// to say on the right.
			void EnableFooter();
			bool HasFooter() { return footerLeft != NULL; };
			void SetFooterLeft(std::string text);
			void SetFooterRight(std::string text);

			// A footer line that can be clicked, sitting after whatever the window
			// has to say about itself. Empty text takes it away, so a window can
			// offer something only while it is worth offering.
			void SetFooterAction(std::string text, std::function<void()> onClick);
			void InvokeFooterAction();

			unsigned int GetFooterBandHeight();

			// Everything the window draws above and below its panels, which is
			// what a tab has to measure itself against.
			unsigned int GetChromeAboveHeight() { return TITLE_BAR_HEIGHT + TAB_HEIGHT + GetSearchBandHeight(); };
			unsigned int GetChromeBelowHeight() { return GetFooterBandHeight(); };

			void OnDraw();
			static void Draw();

			static void Sort(UI* zero);

			bool OnLeftClick(bool up, unsigned int mouseX, unsigned int mouseY);
			static bool LeftClick(bool up, unsigned int mouseX, unsigned int mouseY);

			bool OnRightClick(bool up, unsigned int mouseX, unsigned int mouseY);
			static bool RightClick(bool up, unsigned int mouseX, unsigned int mouseY);

			// Whether the window can be resized by dragging its corner. Off by
			// default: a window whose contents are laid out to a fixed size would
			// be resized around them, so this is for windows whose tabs measure
			// themselves against the window.
			bool IsResizable() { return resizable; };
			void SetResizable(bool state) { Lock(); resizable = state; if (!state) resizing = false; Unlock(); };

			// True while the corner grip is held, in which case the window is
			// following the mouse and clicks belong to the grip.
			bool IsResizing() { return resizing; };
			void SetResizing(bool state, bool write_file);
			void SetResizing(bool state);

			// Smallest the window may be dragged to, clamped so a window can
			// always be got back to a usable size on a small screen. A window may
			// raise its own minimum, for contents needing more room than the
			// shared default leaves.
			void SetMinSize(unsigned int minX, unsigned int minY);
			unsigned int GetMinXSize();
			unsigned int GetMinYSize();

			bool InResizeGrip(unsigned int xPos, unsigned int yPos) {
				return resizable && !minimized &&
					xPos >= x + xSize - RESIZE_GRIP_SIZE && xPos <= x + xSize &&
					yPos >= y + ySize - RESIZE_GRIP_SIZE && yPos <= y + ySize;
			};

			bool InWindow(unsigned int xPos, unsigned int yPos) { return xPos >= x && xPos <= x + xSize && yPos >= y && yPos <= y + ySize; };
			bool InTitle(unsigned int xPos, unsigned int yPos) { return xPos >= x && xPos <= x + xSize && yPos >= y && yPos <= y + TITLE_BAR_HEIGHT; };
			static bool InPos(unsigned int xPos, unsigned int yPos, unsigned int x, unsigned int y, unsigned int xSize, unsigned int ySize) { return xPos >= x && xPos <= x + xSize && yPos >= y && yPos <= y + ySize; };
	};
};
