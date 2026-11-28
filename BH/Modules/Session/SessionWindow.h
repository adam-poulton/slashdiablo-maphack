#pragma once
#include "../Window/WindowModule.h"

// The session window: the session in hand, a tab to each way of looking at it.
// It owns the window, its hotkey and the chat commands. What a session is and
// what it has come to belong to the Session module, which the tabs read.
class SessionWindow : public WindowModule {
	protected:
		std::vector<ChatCommand> GetOwnCommands();
		void OnOpenStateChanged(bool open);

	public:
		SessionWindow() : WindowModule("Session", "Show Session", "None") {};

		void OnLoad();
		void OnUserInput(const wchar_t* msg, bool fromGame, bool* block);
};
