#include "Bnet.h"
#include "../Settings/SettingsRegistry.h"
#include "../../D2Ptrs.h"
#include "../../BH.h"

unsigned int Bnet::failToJoin;
bool* Bnet::showLastGame;
bool* Bnet::showLastPass;
bool* Bnet::nextInstead;
bool* Bnet::keepDesc;
std::string Bnet::lastName;
std::string Bnet::lastPass;
std::string Bnet::lastDesc;
std::string Bnet::defaultName;
std::string Bnet::defaultPass;
std::string Bnet::defaultDesc;
std::regex Bnet::reg = std::regex("^(.*?)(\\d+)$");

// Fixes Unrecoverable internal error 6FF61787
Patch* fog10251Patch = new Patch(Jump, FOG, { 0x11690, 0x11690 }, (int)Bnet::FOG10251Patch, 5);

Patch* bnetLobbyPatch = new Patch(Jump, D2MULTI, { 0xBC00, 0xF9B0 }, (int)Bnet::BnetLobbyAdBlockPatch, 5);

Patch* nextGame1 = new Patch(Call, D2MULTI, { 0x14D29, 0xADAB }, (int)Bnet::NextGamePatch, 5);
Patch* nextGame2 = new Patch(Call, D2MULTI, { 0x14A0B, 0xB5E9 }, (int)Bnet::NextGamePatch, 5);
Patch* nextPass1 = new Patch(Call, D2MULTI, { 0x14D64, 0xADE6 }, (int)Bnet::NextPassPatch, 5);
Patch* nextPass2 = new Patch(Call, D2MULTI, { 0x14A46, 0xB624 }, (int)Bnet::NextPassPatch, 5);

Patch* gameDesc = new Patch(Call, D2MULTI, { 0x14D8F, 0xB64F }, (int)Bnet::GameDescPatch, 5);

Patch* ftjPatch = new Patch(Call, D2CLIENT, { 0x4363E, 0x443FE }, (int)FailToJoin_Interception, 6);
Patch* removePass = new Patch(Call, D2MULTI, { 0x1250, 0x1AD0 }, (int)RemovePass_Interception, 5);

void Bnet::OnLoad() {
	// Its own settings, said by itself. They used to be drawn by AutoTele's tab,
	// which reached them through a pointer BH published for the purpose.
	Settings::AddBool(GetName(), Settings::Category::Lobby, "Autofill Last Game", "Autofill last game",
		&bools["Autofill Last Game"],
		"Puts the last game name back in the box when you go to make a game.");
	Settings::AddBool(GetName(), Settings::Category::Lobby, "Autofill Next Game", "Autofill next game",
		&bools["Autofill Next Game"],
		"Fills in the next name in the sequence rather than the last one used.");
	Settings::AddBool(GetName(), Settings::Category::Lobby, "Autofill Last Password", "Autofill last password",
		&bools["Autofill Last Password"], "Puts the last password back in the box.");
	Settings::AddBool(GetName(), Settings::Category::Lobby, "Autofill Description", "Autofill description",
		&bools["Autofill Description"], "Keeps the game description between games.");

	// What the boxes are filled with when there is no previous game to fall back
	// on. The name and the password are capped at what the game accepts; the
	// description is left uncapped.
	Settings::AddText(GetName(), Settings::Category::Lobby, "Default Game Name", "Default game name",
		&defaultName, 15,
		"Filled into the game name box when there is no last game to put back.");
	Settings::AddText(GetName(), Settings::Category::Lobby, "Default Password", "Default password",
		&defaultPass, 15,
		"Filled into the password box when there is no last game to put back.");
	Settings::AddText(GetName(), Settings::Category::Lobby, "Default Description", "Default description",
		&defaultDesc, 0,
		"Filled into the description box when there is no last one to put back.");

	Settings::AddSlider(GetName(), Settings::Category::Lobby, "Fail To Join", "Fail to join after",
		&failToJoin, MIN_FAIL_TO_JOIN, MAX_FAIL_TO_JOIN, STEP_FAIL_TO_JOIN, " ms",
		"How long to wait for a game to open before the client says it failed to join.");

	showLastGame = &bools["Autofill Last Game"];
	*showLastGame = true;
	
	showLastPass = &bools["Autofill Last Password"];
	*showLastPass = true;

	nextInstead = &bools["Autofill Next Game"];
	*nextInstead = true;

	keepDesc = &bools["Autofill Description"];
	*keepDesc = true;

	failToJoin = MAX_FAIL_TO_JOIN;
	LoadConfig();
}

void Bnet::LoadConfig() {
	BH::config->ReadBoolean("Autofill Last Game", *showLastGame);
	BH::config->ReadBoolean("Autofill Last Password", *showLastPass);
	BH::config->ReadBoolean("Autofill Next Game", *nextInstead);
	BH::config->ReadBoolean("Autofill Description", *keepDesc);
	BH::config->ReadInt("Fail To Join", failToJoin);

	// Config::ReadInt yields zero for a key the file does not have, and the wait
	// used to be a box in which zero meant leave the client's own wait alone. Both
	// read as no wait having been chosen, and the longest one is what to fall back
	// on, being the closest to the wait the client would have used.
	if (failToJoin == 0)
		failToJoin = MAX_FAIL_TO_JOIN;

	// Held to the range here and not only by the slider: an old file can name a
	// wait shorter than loading into a game that is opening normally, which gives
	// up on every join, and the settings window opens only in game - so the value
	// has to be made usable whether or not that window is ever reached.
	if (failToJoin < MIN_FAIL_TO_JOIN)
		failToJoin = MIN_FAIL_TO_JOIN;
	if (failToJoin > MAX_FAIL_TO_JOIN)
		failToJoin = MAX_FAIL_TO_JOIN;

	// Used to prefill the create/join boxes when there is no previous game to fall back on
	BH::config->ReadString("Default Game Name", defaultName);
	BH::config->ReadString("Default Password", defaultPass);
	BH::config->ReadString("Default Description", defaultDesc);
	defaultName = Trim(defaultName);
	defaultPass = Trim(defaultPass);
	defaultDesc = Trim(defaultDesc);

	InstallPatches();
}

// Which patches are installed depends on the settings, including on whether the
// defaults are blank, so they are worked out again when a setting changes rather
// than only when a game is left. Never while in a game: the lobby patches are
// removed on joining one, and OnGameExit puts them back.
void Bnet::OnSettingsChanged(const vector<string>& keys) {
	defaultName = Trim(defaultName);
	defaultPass = Trim(defaultPass);
	defaultDesc = Trim(defaultDesc);

	if (D2CLIENT_GetPlayerUnit())
		return;
	RemovePatches();
	InstallPatches();
}

void Bnet::InstallPatches() {
	fog10251Patch->Install();
	bnetLobbyPatch->Install();
	// The defaults are filled in by the same patches, so they need to be installed
	// even when the corresponding autofill option is off.
	if (*showLastGame || *nextInstead || defaultName.size() > 0) {
		nextGame1->Install();
		nextGame2->Install();
	}

	if (*showLastPass || defaultPass.size() > 0) {
		nextPass1->Install();
		nextPass2->Install();
		removePass->Install();
	}

	if (*keepDesc || defaultDesc.size() > 0) {
		gameDesc->Install();
	}

	if (!D2CLIENT_GetPlayerUnit())
		ftjPatch->Install();
}

void Bnet::RemovePatches() {
	fog10251Patch->Remove();
	bnetLobbyPatch->Remove();
	nextGame1->Remove();
	nextGame2->Remove();

	nextPass1->Remove();
	nextPass2->Remove();

	gameDesc->Remove();

	ftjPatch->Remove();
	removePass->Remove();
}

void Bnet::OnUnload() {
	RemovePatches();
}

void Bnet::OnGameJoin() {
	if ( strlen((*p_D2LAUNCH_BnData)->szGameName) > 0)
		lastName = (*p_D2LAUNCH_BnData)->szGameName;

	if ( strlen((*p_D2LAUNCH_BnData)->szGamePass) > 0)
		lastPass = (*p_D2LAUNCH_BnData)->szGamePass;
	else
		lastPass = "";
	
	if ( strlen((*p_D2LAUNCH_BnData)->szGameDesc) > 0)
		lastDesc = (*p_D2LAUNCH_BnData)->szGameDesc;
	else
		lastDesc = "";

	RemovePatches();
}

void Bnet::OnGameExit() {
	if (*nextInstead) {
		std::smatch match;
		if (std::regex_search(Bnet::lastName, match, Bnet::reg) && match.size() == 3) {
			std::string name = match.format("$1");
			if (name.length() != 0) {
				int count = atoi(match.format("$2").c_str());

				//Restart at 1 if the next number would exceed the max game name length of 15
				if (lastName.length() == 15) {
					int maxCountLength = 15 - name.length();
					int countLength = 1;
					int tempCount = count + 1;
					while (tempCount > 9) {
						countLength++;
						tempCount /= 10;
					}
					if (countLength > maxCountLength) {
						count = 1;
					} else {
						count++;
					}
				} else {
					count++;
				}
				char buffer[16];
				sprintf_s(buffer, sizeof(buffer), "%s%d", name.c_str(), count);
				lastName = std::string(buffer);
			}
		}
	}

	InstallPatches();
}

VOID __fastcall Bnet::FOG10251Patch(DWORD lpCriticalSection, char nLine) {
	return;
}

DWORD __stdcall Bnet::BnetLobbyAdBlockPatch(DWORD a1) {
	return 1;
}

VOID __fastcall Bnet::NextGamePatch(Control* box, BOOL (__stdcall *FunCallBack)(Control*, DWORD, DWORD)) {
	// Fall back to the configured default when there is no previous game name
	const bool useLast = (*Bnet::showLastGame || *Bnet::nextInstead) && Bnet::lastName.size() > 0;
	const std::string& name = useLast ? Bnet::lastName : Bnet::defaultName;
	if (name.size() == 0)
		return;

	wchar_t *wszLastGameName = AnsiToUnicode(name.c_str());

	D2WIN_SetControlText(box, wszLastGameName);
	D2WIN_SelectEditBoxText(box);

	// original code
	D2WIN_SetEditBoxProc(box, FunCallBack);
	delete [] wszLastGameName;
}

VOID __fastcall Bnet::NextPassPatch(Control* box, BOOL(__stdcall *FunCallBack)(Control*, DWORD, DWORD)) {
	// Only fall back to the default password when there is no previous game at all;
	// a remembered game name with no password means that game genuinely had none.
	const bool useLast = *Bnet::showLastPass && Bnet::lastPass.size() > 0;
	if (!useLast && Bnet::lastName.size() > 0)
		return;

	const std::string& pass = useLast ? Bnet::lastPass : Bnet::defaultPass;
	if (pass.size() == 0)
		return;
	wchar_t *wszLastPass = AnsiToUnicode(pass.c_str());

	D2WIN_SetControlText(box, wszLastPass);
	
	// original code
	D2WIN_SetEditBoxProc(box, FunCallBack);
	delete[] wszLastPass;
}

VOID __fastcall Bnet::GameDescPatch(Control* box, BOOL(__stdcall *FunCallBack)(Control*, DWORD, DWORD)) {
	// Fall back to the configured default when there is no previous description
	const bool useLast = *Bnet::keepDesc && Bnet::lastDesc.size() > 0;
	const std::string& desc = useLast ? Bnet::lastDesc : Bnet::defaultDesc;
	if (desc.size() == 0)
		return;
	wchar_t *wszLastDesc = AnsiToUnicode(desc.c_str());

	D2WIN_SetControlText(box, wszLastDesc);
	
	// original code
	D2WIN_SetEditBoxProc(box, FunCallBack);
	delete[] wszLastDesc;
}

void __declspec(naked) RemovePass_Interception() {
	__asm {
		PUSHAD
		CALL [Bnet::RemovePassPatch]
		POPAD

		; Original code
		XOR EAX, EAX
		SUB ECX, 01
		RET
	}
}

void Bnet::RemovePassPatch() {
	Control* box = *p_D2MULTI_PassBox;

	if (Bnet::lastPass.size() == 0 || box == nullptr) {
		return;
	}

	wchar_t *wszLastPass = AnsiToUnicode("");
	D2WIN_SetControlText(box, wszLastPass);
	delete[] wszLastPass;
}

void __declspec(naked) FailToJoin_Interception()
{
	/*
	Changes the amount of time, in milliseconds, that we wait for the loading
	door to open before the client confirms that it failed to join the game.
	*/
	__asm
	{
		cmp esi, Bnet::failToJoin;
		ret;
	}
}
