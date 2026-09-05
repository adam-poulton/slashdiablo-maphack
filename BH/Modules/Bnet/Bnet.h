#pragma once
#include "../Module.h"
#include <regex>

// The join wait, in milliseconds, and what one notch of its slider moves. The
// floor is the point below which the wait is shorter than loading into a game
// that is opening normally: any lower and the client gives up on every join, and
// the settings window cannot be opened from the lobby to put it back.
//
// The ceiling is the longest wait worth offering, since a game that has not
// opened by then is not going to.
#define MIN_FAIL_TO_JOIN	1000
#define MAX_FAIL_TO_JOIN	4000
#define STEP_FAIL_TO_JOIN	500

struct Control;

class Bnet : public Module {
	private:
		std::map<string, bool> bools;
		static bool* showLastGame;
		static bool* showLastPass;
		static bool* nextInstead;
		static bool* keepDesc;
		static unsigned int failToJoin;
		static std::string lastName;
		static std::string lastPass;
		static std::string lastDesc;
		static std::string defaultName;
		static std::string defaultPass;
		static std::string defaultDesc;
		static std::regex reg;

	public:

		Bnet() : Module("Bnet") {};

		void OnLoad();
		void OnUnload();
		void LoadConfig();
		void OnSettingsChanged(const vector<string>& keys);

		void OnGameJoin();
		void OnGameExit();

		void InstallPatches();
		void RemovePatches();

		std::map<string, bool>* GetBools() { return &bools; }
		static VOID __fastcall FOG10251Patch(DWORD lpCriticalSection, char nLine);
		static DWORD __stdcall BnetLobbyAdBlockPatch(DWORD a1);
		static VOID __fastcall NextGamePatch(Control* box, BOOL (__stdcall *FunCallBack)(Control*, DWORD, DWORD));
		static VOID __fastcall NextPassPatch(Control* box, BOOL(__stdcall *FunCallBack)(Control*, DWORD, DWORD));
		static VOID __fastcall GameDescPatch(Control* box, BOOL(__stdcall *FunCallBack)(Control*, DWORD, DWORD));
		static void RemovePassPatch();

		static std::string GetDefaultGameName() { return defaultName; }
		static std::string GetDefaultPassword() { return defaultPass; }
		static std::string GetDefaultDescription() { return defaultDesc; }
};

void FailToJoin_Interception();
void RemovePass_Interception();
