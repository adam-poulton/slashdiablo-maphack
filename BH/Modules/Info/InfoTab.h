#pragma once
#include <string>
#include <vector>
#include <Windows.h>
#include "../../Drawing.h"
#include "../Module.h"

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

		// The chat commands that open this tab, lowercased and without their
		// leading dot. Answering one is what makes the Info window switch to
		// this tab and hand it whatever was typed after the command.
		//
		// Listed rather than answered one at a time, so the window can say what
		// its tabs can be asked as well as answer it.
		virtual std::vector<ChatCommand> GetCommands() {
			return std::vector<ChatCommand>();
		};

		// Whether this tab answers a command, which is its own list searched.
		bool HandlesCommand(const std::string& command) {
			std::vector<ChatCommand> commands = GetCommands();
			for (unsigned int i = 0; i < commands.size(); i++) {
				if (commands[i].Answers(command))
					return true;
			}
			return false;
		};

		// Filter the tab's contents from outside the window, for the chat
		// command. An empty search clears the filter.
		virtual void Search(const std::string& text) {};

		// Called every frame before the window is drawn, whether or not this tab
		// is the active one, so a tab can load data lazily.
		virtual void OnDraw() {};

		// Called for keys the Info window did not consume itself, and only while
		// this tab is active. Return true to swallow the key.
		virtual bool OnKey(bool up, BYTE key) { return false; };

		// Called for every tab when the window opens, however it was opened.
		// Somewhere to put whatever should be ready the moment it appears, such
		// as focus on the control the user is most likely to reach for.
		virtual void OnOpen() {};

		// Called for every tab when the window closes, however it was closed.
		// Somewhere to drop whatever shouldn't still be there next time it opens,
		// such as a search someone typed.
		virtual void OnClose() {};
};
