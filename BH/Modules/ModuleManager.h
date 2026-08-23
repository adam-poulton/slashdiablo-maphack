#pragma once
#include <map>
#include <string>
#include <Windows.h>

class Module;
using namespace std;

class ModuleManager {
	private:
		map<string, Module*> moduleList;

		void FixName(std::string& name);

		// Prints every command BH answers, module by module. The game keeps its
		// own commands to itself and cannot be told about anyone else's, so this
		// is the only place they are all written down.
		void PrintCommands();

		// Prints where to find BH's commands, for a command BH does not answer.
		void HintCommands(const std::string& command);

	public:
		ModuleManager();
		~ModuleManager();

		// Module Management
		void Add(Module* module);
		Module* Get(string name);
		void Remove(Module* module);

		void LoadModules();
		void UnloadModules();
		void ReloadConfig();
		void MpqLoaded();

		bool UserInput(wchar_t* module, wchar_t* msg, bool fromGame);

		__event void OnLoop();

		__event void OnGameJoin();
		__event void OnGameExit();

		__event void OnDraw();
		__event void OnAutomapDraw();
		__event void OnOOGDraw();

		__event void OnLeftClick(bool up, unsigned int x, unsigned int y, bool* block);
		__event void OnRightClick(bool up, unsigned int x, unsigned int y, bool* block);
		__event void OnKey(bool up, BYTE key, LPARAM lParam, bool* block);

		__event void OnChatPacketRecv(BYTE* packet, bool* block);
		__event void OnRealmPacketRecv(BYTE* packet, bool* block);
		__event void OnGamePacketRecv(BYTE* packet, bool* block);

		__event void OnChatMsg(const char* user, const char* msg, bool fromGame, bool* block);
};
