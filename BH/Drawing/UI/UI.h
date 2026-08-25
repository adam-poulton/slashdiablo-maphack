#pragma once

#include <Windows.h>
#include <string>
#include <list>
#include "../Hook.h"

namespace Drawing {
	class UI;
	class UITab;

	// Window chrome: the title bar across the top and the row of tab headings
	// under it. A tab measures itself against the window less both of these.
	#define TITLE_BAR_HEIGHT 15
	#define TAB_HEIGHT 13
	#define MINIMIZED_Y_POS 585
	#define MINIMIZED_X_POS 234

	// The grip in the bottom right corner that resizes the window, and how small
	// a window may be dragged. The minimum has to leave room below the title bar
	// and the tab row for a tab to have any height at all, since a tab measures
	// itself against the window.
	#define RESIZE_GRIP_SIZE	12
	#define UI_MIN_WIDTH		200
	#define UI_MIN_HEIGHT		(TITLE_BAR_HEIGHT + TAB_HEIGHT + 60)

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
			std::string name;//Name of the UI
			UITab* currentTab;//Current tab open at the time.
			CRITICAL_SECTION crit;//Critical section

			void EnsureInBounds();

			// Follows the mouse while the grip is held, and draws the grip.
			void DragResizeTo(unsigned int mouseX, unsigned int mouseY);
			void DrawResizeGrip();
		public:
			std::list<UITab*> Tabs;

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

			UITab* GetActiveTab() { if (!currentTab) { currentTab = (*Tabs.begin()); } return currentTab; };
			void SetCurrentTab(UITab* tab) { Lock(); currentTab = tab; Unlock(); };

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
			// always be got back to a usable size on a small screen.
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
