#pragma once
#include <string>
#include <vector>
#include "../Window/WindowModule.h"
#include "BaseTab.h"
#include "RecipeTab.h"
#include "RunewordTab.h"
#include "SetTab.h"
#include "UniqueTab.h"

#define INFO_TOGGLE_NAME	"Info Window"

// Which panels the Info window has, and the commands that reach them.
// Everything about owning a window belongs to WindowModule
class InfoWindow : public WindowModule {
	protected:
		std::vector<ChatCommand> GetOwnCommands();

	public:
		InfoWindow() : WindowModule("Info", INFO_TOGGLE_NAME, "VK_NUMPAD9") {};

		void OnLoad();
		void OnUserInput(const wchar_t* msg, bool fromGame, bool* block);
};
