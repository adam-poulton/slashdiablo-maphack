#pragma once
#include <string>
#include <vector>
#include "ModuleManager.h"

using namespace std;

// A chat command and the other names that reach it. The first name is the one to
// print and the rest are the same command by another name, which is worth saying
// rather than listing each as a command of its own.
struct ChatCommand {
	string name;
	vector<string> aliases;

	ChatCommand() {};
	ChatCommand(const string& name) : name(name) {};
	ChatCommand(const string& name, const vector<string>& aliases) :
		name(name), aliases(aliases) {};

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
