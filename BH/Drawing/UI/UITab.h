#pragma once
#include "../Hook.h"
#include "UI.h"

namespace Drawing {
	class UITab : public HookGroup {
		private:
			std::string name;
			UI* ui;
		public:
			UITab(std::string name, UI* nui) : name(name), ui(nui) {ui->Tabs.push_back(this); if (ui->Tabs.size() == 1) { ui->SetCurrentTab(this); }};
			~UITab();

			const std::string& GetName() { return name; };

			// The tab, and every hook built into it, is on whichever screen its
			// window is.
			HookVisibility GetVisibility() { return ui->GetVisibility(); };

			unsigned int GetX() { return ui->GetX(); };
			unsigned int GetY() { return ui->GetY() + ui->GetChromeAboveHeight(); };
			unsigned int GetXSize() { return ui->GetXSize(); };

			// Whatever the window has not spent on its own chrome. Clamped rather
			// than allowed to wrap: the window can be dragged smaller than its
			// chrome only in the degenerate case where the screen itself is,
			// and an unsigned subtraction there would come out enormous.
			unsigned int GetYSize() {
				unsigned int spent = ui->GetChromeAboveHeight() + ui->GetChromeBelowHeight();
				return (ui->GetYSize() > spent) ? (ui->GetYSize() - spent) : 0;
			};

			unsigned int GetTabPos();
			unsigned int GetTabSize() { return (ui->GetXSize() / ui->Tabs.size()); };
			unsigned int GetTabX() { return ui->GetX() + GetTabPos() * GetTabSize(); };
			unsigned int GetTabY() { return ui->GetY() + TITLE_BAR_HEIGHT; };


			// Also gates whether this tab's hooks handle clicks and keys, so a
			// window that isn't on screen must not report an active tab.
			bool IsActive() { return ui->IsVisible() && ui->GetActiveTab() == this && !ui->IsMinimized(); };

			// A tab with no row to be drawn in is nowhere, and must not claim the
			// strip of panel that would otherwise be under it.
			bool IsHovering(unsigned int x, unsigned int y) { return ui->GetTabBandHeight() > 0 && x >= GetTabX() && y >= GetTabY() && x <= (GetTabX() + GetTabSize()) && y <= (GetTabY() + TAB_HEIGHT); };

			void OnDraw();
	};
};