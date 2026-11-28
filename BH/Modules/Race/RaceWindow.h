#pragma once
#include "../Window/WindowModule.h"

// The race window: the routes there are to time a run on, and the run being
// timed. It owns the window, its hotkey and the chat commands. Timing a run
// belongs to the Race module, which the window reads.
class RaceWindow : public WindowModule {
	protected:
		std::vector<ChatCommand> GetOwnCommands();
		void OnOpenStateChanged(bool open);

	public:
		RaceWindow() : WindowModule("Race", "Show Race", "None") {};

		void OnLoad();
		void OnUserInput(const wchar_t* msg, bool fromGame, bool* block);
};
