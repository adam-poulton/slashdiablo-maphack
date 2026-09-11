#pragma once
#include <string>
#include <vector>
#include "../Window/WindowModule.h"
#include "BaseTab.h"
#include "RecipeTab.h"
#include "RunewordTab.h"
#include "SetTab.h"
#include "UniqueTab.h"

#define CODEX_TOGGLE_NAME	"Codex"

// Which panels the codex has, and the commands that reach them.
// Everything about owning a window belongs to WindowModule
class CodexWindow : public WindowModule {
	protected:
		std::vector<ChatCommand> GetOwnCommands();

	public:
		CodexWindow() : WindowModule("Codex", CODEX_TOGGLE_NAME, "VK_NUMPAD9") {};

		void OnLoad();
		void OnUserInput(const wchar_t* msg, bool fromGame, bool* block);
};
