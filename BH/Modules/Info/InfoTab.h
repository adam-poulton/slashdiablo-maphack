#pragma once
#include <string>
#include <Windows.h>
#include "../../Drawing.h"

// One panel of the Info window. Subclasses build their own controls into the
// tab in their constructor and refresh them from OnDraw().
//
// The Info module owns the window, the hotkey and tab switching; a tab only
// needs to care about its own contents.
class InfoTab {
	protected:
		Drawing::UITab* tab;

	public:
		InfoTab(std::string name, Drawing::UI* ui) : tab(new Drawing::UITab(name, ui)) {};
		virtual ~InfoTab() {};

		Drawing::UITab* GetTab() { return tab; };
		bool IsActive() { return tab->IsActive(); };

		// Called once the MPQ data tables are available.
		virtual void MpqLoaded() {};

		// Called every frame before the window is drawn, whether or not this tab
		// is the active one, so a tab can load data lazily.
		virtual void OnDraw() {};

		// Called for keys the Info window did not consume itself, and only while
		// this tab is active. Return true to swallow the key.
		virtual bool OnKey(bool up, BYTE key) { return false; };
};
