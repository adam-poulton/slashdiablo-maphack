#pragma once
#include <map>
#include <string>
#include <vector>
#include "../Module.h"
#include "../../Config.h"
#include "../../Drawing.h"
#include "BaseTab.h"
#include "InfoTab.h"
#include "RecipeTab.h"
#include "RunewordTab.h"
#include "SetTab.h"
#include "UniqueTab.h"

// The module owns the window, its hotkey and tab switching; each panel is an
// InfoTab and deals only with its own contents.
class InfoWindow : public Module {
	private:
		CRITICAL_SECTION crit;

		Drawing::UI* infoUI;
		std::vector<InfoTab*> tabs;

		std::map<std::string, Toggle> Toggles;
		bool wasOpen;			// so closing the window can be noticed

		InfoTab* GetActiveTab();
		InfoTab* GetCurrentTab();
		InfoTab* GetTabForCommand(const std::string& command);
		void CheckOpenState();

		// Moves the tab in front by delta places, wrapping at either end.
		void CycleTab(int delta);

	public:
		InfoWindow() : Module("Info"),
			infoUI(NULL),
			wasOpen(false) {
			InitializeCriticalSection(&crit);
		};

		~InfoWindow() {
			for (unsigned int i = 0; i < tabs.size(); i++)
				delete tabs[i];
			DeleteCriticalSection(&crit);
		};

		void Lock() { EnterCriticalSection(&crit); };
		void Unlock() { LeaveCriticalSection(&crit); };

		void OnLoad();
		void LoadConfig();
		void MpqLoaded();
		std::vector<ChatCommand> GetCommands();

		void OnLoop();
		void OnGameExit();
		void OnDraw();
		void OnKey(bool up, BYTE key, LPARAM lParam, bool* block);
		void OnUserInput(const wchar_t* msg, bool fromGame, bool* block);

		// Collapses to the title bar rather than hiding.
		void ShowWindow(bool show);
		void ShowTab(InfoTab* tab);
};
