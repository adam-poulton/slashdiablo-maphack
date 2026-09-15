#pragma once
#include <map>
#include <string>
#include <vector>
#include "../Window/WindowModule.h"
#include "BaseTab.h"
#include "QueryBuilder.h"
#include "RecipeTab.h"
#include "RunewordTab.h"
#include "SetTab.h"
#include "UniqueTab.h"

#define CODEX_TOGGLE_NAME	"Codex"

// Which panels the codex has, and the commands that reach them.
// Everything about owning a window belongs to WindowModule
class CodexWindow : public WindowModule {
	private:
		// The stat conditions, and the kind of source each panel that offers
		// them is a view onto. One builder for the window rather than one per
		// panel: the question is the same on each of them, and the search box
		// beside it is shared for the same reason.
		//
		// A panel with no entry here does not offer conditions, which is what
		// takes the button off the recipes and the bases: a recipe is a
		// transaction rather than a thing with stats, and a base is what an
		// item is made of before anything has granted it any.
		QueryBuilder* conditions;
		std::map<UIPanel*, std::string> conditionKinds;

		// Whether the escape that is being held shut the picker, so that its
		// release does not go on to close the window as well.
		bool escapeShutPicker;

		// Adds a panel that offers conditions, under the kind its stats come
		// from.
		void AddConditionPanel(UIPanel* panel, const std::string& kind);

		// Brings the button, the room the rows take and the kind the picker
		// offers up to date with whichever panel is in front.
		void UpdateConditions();

	protected:
		std::vector<ChatCommand> GetOwnCommands();
		void OnOpenStateChanged(bool open);

	public:
		CodexWindow() : WindowModule("Codex", CODEX_TOGGLE_NAME, "VK_NUMPAD9"),
			conditions(NULL), escapeShutPicker(false) {};

		void OnLoad();
		void OnDraw();
		void OnKey(bool up, BYTE key, LPARAM lParam, bool* block);
		void OnUserInput(const wchar_t* msg, bool fromGame, bool* block);
};
