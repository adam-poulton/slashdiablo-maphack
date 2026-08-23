#pragma once
#include <string>
#include <vector>
#include "ModuleManager.h"

using namespace std;

// A chat command: the name to print, the other names that reach the same one,
// what it takes after the name, and what it does. Aliases are held as aliases
// rather than as commands of their own so that saying so costs nothing.
//
// The description lands on one line of the chat log beside the command, so keep
// it to a few words; nothing wraps it and the game will cut what does not fit.
// Filled in by aggregate initialisation, a command being a plain description of
// itself:
//
//     { "cube", { "recipe", "recipes" }, "<search>", "Opens the Recipes tab" }
struct ChatCommand {
	string name;
	vector<string> aliases;
	string args;			// "<search>", and empty where it takes none
	string description;

	// Whether this command answers a name, its own or any of its aliases.
	bool Answers(const string& command) const {
		if (name.compare(command) == 0)
			return true;
		for (unsigned int i = 0; i < aliases.size(); i++) {
			if (aliases[i].compare(command) == 0)
				return true;
		}
		return false;
	};
};

class Module {
	private:
		friend class ModuleManager;

		string name;
		bool active;
		string invokedCommand;	// the command the current UserInput arrived as

		void Load();
		void Unload();

	public:
		Module(string name);
		virtual ~Module();

		string GetName() { return name; };
		bool IsActive() { return active; };

		// The chat commands this module answers, lowercased and without their
		// leading dot, its own name included where it answers to that. A module
		// owning several reads which one was typed from GetInvokedCommand() while
		// handling the input.
		//
		// Listed rather than answered one at a time, because BH has to be able to
		// say what it can be asked: the game keeps its own commands to itself and
		// cannot be told about anyone else's, so a command nothing lists is a
		// command nobody can find.
		virtual vector<ChatCommand> GetCommands() { return vector<ChatCommand>(); };

		// Whether this module answers a command, which is its own list searched.
		bool OwnsCommand(const string& command);
		const string& GetInvokedCommand() { return invokedCommand; };

		// Module Events
		virtual void OnLoad() {};
		virtual void OnUnload() {};

		virtual void LoadConfig() {};
		virtual void MpqLoaded() {};

		virtual void OnLoop() {};

		// Game Events
		virtual void OnGameJoin() {}
		virtual void OnGameExit() {};

		// Drawing Events
		virtual void OnDraw() {};
		virtual void OnAutomapDraw() {};
		virtual void OnOOGDraw() {};

		virtual void OnLeftClick(bool up, unsigned int x, unsigned int y, bool* block) {};
		virtual void OnRightClick(bool up, unsigned int x, unsigned int y, bool* block) {};
		virtual void OnKey(bool up, BYTE key, LPARAM lParam, bool* block) {};

		virtual void OnChatPacketRecv(BYTE* packet, bool* block) {};
		virtual void OnRealmPacketRecv(BYTE* packet, bool* block) {};
		virtual void OnGamePacketRecv(BYTE* packet, bool* block) {};

		__event void UserInput(const wchar_t* msg, bool fromGame, bool* block);
		virtual void OnUserInput(const wchar_t* msg, bool fromGame, bool* block) {};
		virtual void OnChatMsg(const char* user, const char* msg, bool fromGame, bool* block) {};
};
