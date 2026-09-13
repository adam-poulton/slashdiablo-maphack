#include "Bnet.h"
#include "../Settings/SettingsRegistry.h"
#include "../../D2Ptrs.h"
#include "../../BH.h"

// Where D2Client 1.13c keeps the countdown on the notice the lobby is showing and
// the id of that notice. Reached by offset because only joinNoticePatch below
// knows they are there, and only on the version that patch is installed on.
#define NOTICE_FRAMES_113C	0x11C3D0
#define NOTICE_ID_113C		0x119938

// The id the client sets for "failed to join game". Every other notice, the ones
// a lost connection raises among them, keeps the length the client wanted.
#define NOTICE_FAILED_TO_JOIN	6

// Where BNCLIENT 1.13c weighs how long it has waited on a battle.net reply
// against the wait it allows. Reached by offset because only enterChatWaitPatch
// below knows it is there, and only on the version it is installed on.
#define WAIT_BUDGET_113C	0x10DC1

// The battle.net message whose reply the lobby opens on: SID_ENTERCHAT. The wait
// the patch stands in is the one loop the client waits on every reply in, and it
// is told which one this run is waiting on.
#define MSG_ENTER_CHAT		0x0A

unsigned int Bnet::failToJoinChoice;
unsigned int Bnet::failToJoin;
unsigned int Bnet::joinNotice;
unsigned int Bnet::enterChatChoice;
unsigned int Bnet::enterChatWait;
bool* Bnet::showLastGame;
bool* Bnet::showLastPass;
bool* Bnet::nextInstead;
bool* Bnet::keepDesc;
bool* Bnet::overrideFailToJoin;
bool* Bnet::overrideJoinNotice;
bool* Bnet::overrideEnterChat;
std::string Bnet::lastName;
std::string Bnet::lastPass;
std::string Bnet::lastDesc;
std::string Bnet::defaultName;
std::string Bnet::defaultPass;
std::string Bnet::defaultDesc;
std::regex Bnet::reg = std::regex("^(.*?)(\\d+)$");

// Fog checks a critical section before every lock it takes, on every Windows
// since NT: the debug record non-null, aligned and naming the section back, the
// lock and recursion counts within sane bounds. A section that fails any of them
// is taken as a sign that memory is already gone, and the client is killed where
// it stands, as Unrecoverable internal error 6FF61787. That number is the return
// address inside the check and reads the same whichever test failed.
//
// Stood down for the session, so no lock is checked and none of those can be
// raised. The check is not necessarily wrong when it fires: the first thing it
// asks is whether the section still has a debug record, which is what deleting a
// section takes away, and a section that has been deleted can usually still be
// entered. So this buys a client that carries on in place of one that stops, and
// gives up the only notice that a lock was ever in that state.
Patch* fog10251Patch = new Patch(Jump, FOG, { 0x11690, 0x11690 }, (int)Bnet::FOG10251Patch, 5);

Patch* bnetLobbyPatch = new Patch(Jump, D2MULTI, { 0xBC00, 0xF9B0 }, (int)Bnet::BnetLobbyAdBlockPatch, 5);

Patch* nextGame1 = new Patch(Call, D2MULTI, { 0x14D29, 0xADAB }, (int)Bnet::NextGamePatch, 5);
Patch* nextGame2 = new Patch(Call, D2MULTI, { 0x14A0B, 0xB5E9 }, (int)Bnet::NextGamePatch, 5);
Patch* nextPass1 = new Patch(Call, D2MULTI, { 0x14D64, 0xADE6 }, (int)Bnet::NextPassPatch, 5);
Patch* nextPass2 = new Patch(Call, D2MULTI, { 0x14A46, 0xB624 }, (int)Bnet::NextPassPatch, 5);

Patch* gameDesc = new Patch(Call, D2MULTI, { 0x14D8F, 0xB64F }, (int)Bnet::GameDescPatch, 5);

Patch* ftjPatch = new Patch(Call, D2CLIENT, { 0x4363E, 0x443FE }, (int)FailToJoin_Interception, 6);

// Stands in for the store that starts the countdown on a lobby notice. Only the
// 1.13c store has been found, and a patch with no offset for the running version
// is not installed, so on 1.13d a notice keeps the length the client gives it.
Patch* joinNoticePatch = new Patch(Call, D2CLIENT, { 0x4358B, 0 }, (int)JoinNotice_Interception, 10);

Patch* removePass = new Patch(Call, D2MULTI, { 0x1250, 0x1AD0 }, (int)RemovePass_Interception, 5);

// Where BNCLIENT's wait carries on once the comparison below has been made: the
// instruction after the one that patch stands in for. Held in a variable because
// the stub jumps back through it with every register live and no room to work one
// out, and resolved when the patch goes in, since a module's address is not known
// before it is loaded.
static DWORD waitResume;

// The lobby sends SID_ENTERCHAT on its way in and then waits on the reply,
// sleeping in ten millisecond steps for up to 45 seconds, on the thread that
// draws it. pvpgn answers that message at login but not when the lobby is handed
// back by a game it never opened, so the window is frozen for the whole wait
// before the lobby appears.
//
// Nothing is given up by giving up sooner. The reply carries only the account's
// chat name, which the client already holds from the reply it did get at login,
// and the lobby is opened whether the wait ended in a reply or in the time
// running out.
//
// Stands in for the comparison the client makes, which is reached with the time
// waited so far in eax and the message being waited on in esi. Only SID_ENTERCHAT
// is answered for: the same loop carries the waits on logon, on auth and on the
// realm and game lists, which are answered and can fairly take a while.
Patch* enterChatWaitPatch = new Patch(Jump, BNCLIENT, { WAIT_BUDGET_113C, 0 },
	(int)EnterChatWait_Interception, 5);

// The only patch here with an address of its own to find first.
static void InstallEnterChatWaitPatch() {
	waitResume = Patch::GetDllOffset(BNCLIENT, WAIT_BUDGET_113C + 5);
	enterChatWaitPatch->Install();
}

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

	// Held before the sliders are registered, since a slider is given the bool
	// itself rather than the key of one.
	overrideFailToJoin = &bools["Override Fail To Join"];
	*overrideFailToJoin = true;

	overrideJoinNotice = &bools["Override Join Notice"];
	*overrideJoinNotice = true;

	// The switch on each of these chooses between the wait below it and the client's
	// own. It does not decide whether the patch behind the wait is installed: every
	// patch this module has goes in once, at load, and a patch left in place saying
	// what the client would have said is what off means.
	Settings::AddSlider(GetName(), Settings::Category::Lobby, "Fail To Join", "Fail to join after",
		&failToJoinChoice, MIN_FAIL_TO_JOIN, MAX_FAIL_TO_JOIN, STEP_FAIL_TO_JOIN, " ms",
		"How long to wait for a game to open before the client says it failed to join. "
		"Off leaves the client to decide.",
		"", overrideFailToJoin);
	Settings::AddSlider(GetName(), Settings::Category::Lobby, "Join Notice", "Hold failed to join for",
		&joinNotice, MIN_JOIN_NOTICE, MAX_JOIN_NOTICE, STEP_JOIN_NOTICE, " frames",
		"How long the failed to join notice is displayed, in frames. "
		"Off holds it for the length the client gives it.",
		"", overrideJoinNotice);

	overrideEnterChat = &bools["Override Enter Chat Wait"];
	*overrideEnterChat = true;

	Settings::AddSlider(GetName(), Settings::Category::Lobby, "Enter Chat Wait", "Wait on battle.net for",
		&enterChatChoice, MIN_ENTER_CHAT, MAX_ENTER_CHAT, STEP_ENTER_CHAT, " ms",
		"How long the lobby waits on battle.net's reply before it opens anyway. "
		"The client draws nothing while it waits. Off leaves the client to decide.",
		"", overrideEnterChat);

	showLastGame = &bools["Autofill Last Game"];
	*showLastGame = true;
	
	showLastPass = &bools["Autofill Last Password"];
	*showLastPass = true;

	nextInstead = &bools["Autofill Next Game"];
	*nextInstead = true;

	keepDesc = &bools["Autofill Description"];
	*keepDesc = true;

	failToJoinChoice = MAX_FAIL_TO_JOIN;
	joinNotice = DEFAULT_JOIN_NOTICE;
	enterChatChoice = DEFAULT_ENTER_CHAT;
	LoadConfig();
	InstallPatches();
}

void Bnet::LoadConfig() {
	BH::config->ReadBoolean("Autofill Last Game", *showLastGame);
	BH::config->ReadBoolean("Autofill Last Password", *showLastPass);
	BH::config->ReadBoolean("Autofill Next Game", *nextInstead);
	BH::config->ReadBoolean("Autofill Description", *keepDesc);
	BH::config->ReadBoolean("Override Fail To Join", *overrideFailToJoin);
	BH::config->ReadBoolean("Override Join Notice", *overrideJoinNotice);
	BH::config->ReadBoolean("Override Enter Chat Wait", *overrideEnterChat);
	BH::config->ReadInt("Fail To Join", failToJoinChoice, MAX_FAIL_TO_JOIN);

	// Config::ReadInt yields zero for a key the file does not have, and the wait
	// used to be a box in which zero meant leave the client's own wait alone. Both
	// read as no wait having been chosen, and the longest one is what to fall back
	// on, being the closest to the wait the client would have used.
	if (failToJoinChoice == 0)
		failToJoinChoice = MAX_FAIL_TO_JOIN;

	// Held to the range here and not only by the slider: an old file can name a
	// wait shorter than loading into a game that is opening normally, which gives
	// up on every join, and the settings window opens only in game - so the value
	// has to be made usable whether or not that window is ever reached.
	if (failToJoinChoice < MIN_FAIL_TO_JOIN)
		failToJoinChoice = MIN_FAIL_TO_JOIN;
	if (failToJoinChoice > MAX_FAIL_TO_JOIN)
		failToJoinChoice = MAX_FAIL_TO_JOIN;

	// Held to the range for the same reason as the wait above: the slider cannot
	// offer a value outside it, but a file can name one.
	BH::config->ReadInt("Join Notice", joinNotice, DEFAULT_JOIN_NOTICE);
	if (joinNotice < MIN_JOIN_NOTICE)
		joinNotice = MIN_JOIN_NOTICE;
	if (joinNotice > MAX_JOIN_NOTICE)
		joinNotice = MAX_JOIN_NOTICE;

	// Held to the range for the same reason as the two above.
	BH::config->ReadInt("Enter Chat Wait", enterChatChoice, DEFAULT_ENTER_CHAT);
	if (enterChatChoice < MIN_ENTER_CHAT)
		enterChatChoice = MIN_ENTER_CHAT;
	if (enterChatChoice > MAX_ENTER_CHAT)
		enterChatChoice = MAX_ENTER_CHAT;
	SetWaits();

	// Used to prefill the create/join boxes when there is no previous game to fall back on
	BH::config->ReadString("Default Game Name", defaultName);
	BH::config->ReadString("Default Password", defaultPass);
	BH::config->ReadString("Default Description", defaultDesc);
	defaultName = Trim(defaultName);
	defaultPass = Trim(defaultPass);
	defaultDesc = Trim(defaultDesc);
}

void Bnet::OnSettingsChanged(const vector<string>& keys) {
	defaultName = Trim(defaultName);
	defaultPass = Trim(defaultPass);
	defaultDesc = Trim(defaultDesc);
	SetWaits();
}

// Every patch here goes in once, at load, and stays in for the session. They are
// written by BH's own thread while the game runs on its own, and a patch is five
// to ten bytes of code rewritten in place: a thread reading those bytes as they
// are written reads half of each instruction. Some of these stand in code the
// game is running constantly - Fog's 10251 alone is called from a hundred and
// fifty places and imported by three more libraries - so the bytes are written
// once, before any of that is under way, and left alone.
//
// What each patch does is decided when it runs instead, from the settings it
// reads there. A setting that is off leaves the client's own behaviour in place,
// which is what the patch would have left had it never been installed.
void Bnet::InstallPatches() {
	fog10251Patch->Install();
	bnetLobbyPatch->Install();

	nextGame1->Install();
	nextGame2->Install();

	nextPass1->Install();
	nextPass2->Install();
	removePass->Install();

	gameDesc->Install();

	ftjPatch->Install();
	joinNoticePatch->Install();

	InstallEnterChatWaitPatch();
}

// Only on the way out, when BH is going and a patch left in place would be a jump
// into code that is no longer there.
void Bnet::RemovePatches() {
	fog10251Patch->Remove();
	bnetLobbyPatch->Remove();
	nextGame1->Remove();
	nextGame2->Remove();

	nextPass1->Remove();
	nextPass2->Remove();

	gameDesc->Remove();

	ftjPatch->Remove();
	joinNoticePatch->Remove();
	removePass->Remove();
	enterChatWaitPatch->Remove();
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
}

VOID __fastcall Bnet::FOG10251Patch(DWORD lpCriticalSection, char nLine) {
	return;
}

DWORD __stdcall Bnet::BnetLobbyAdBlockPatch(DWORD a1) {
	return 1;
}

// The box is given its proc whether or not there is anything to put in it: the
// call that does so is the code this patch stands in for, and a box that never
// gets one is a box that cannot be typed in.
VOID __fastcall Bnet::NextGamePatch(Control* box, BOOL (__stdcall *FunCallBack)(Control*, DWORD, DWORD)) {
	// Fall back to the configured default when there is no previous game name
	const bool useLast = (*Bnet::showLastGame || *Bnet::nextInstead) && Bnet::lastName.size() > 0;
	const std::string& name = useLast ? Bnet::lastName : Bnet::defaultName;

	if (name.size() > 0) {
		wchar_t *wszLastGameName = AnsiToUnicode(name.c_str());

		D2WIN_SetControlText(box, wszLastGameName);
		D2WIN_SelectEditBoxText(box);
		delete [] wszLastGameName;
	}

	// original code
	D2WIN_SetEditBoxProc(box, FunCallBack);
}

VOID __fastcall Bnet::NextPassPatch(Control* box, BOOL(__stdcall *FunCallBack)(Control*, DWORD, DWORD)) {
	// Only fall back to the default password when there is no previous game at all;
	// a remembered game name with no password means that game genuinely had none.
	const bool useLast = *Bnet::showLastPass && Bnet::lastPass.size() > 0;
	const std::string& pass = useLast ? Bnet::lastPass : Bnet::defaultPass;

	if ((useLast || Bnet::lastName.size() == 0) && pass.size() > 0) {
		wchar_t *wszLastPass = AnsiToUnicode(pass.c_str());

		D2WIN_SetControlText(box, wszLastPass);
		delete[] wszLastPass;
	}

	// original code
	D2WIN_SetEditBoxProc(box, FunCallBack);
}

VOID __fastcall Bnet::GameDescPatch(Control* box, BOOL(__stdcall *FunCallBack)(Control*, DWORD, DWORD)) {
	// Fall back to the configured default when there is no previous description
	const bool useLast = *Bnet::keepDesc && Bnet::lastDesc.size() > 0;
	const std::string& desc = useLast ? Bnet::lastDesc : Bnet::defaultDesc;

	if (desc.size() > 0) {
		wchar_t *wszLastDesc = AnsiToUnicode(desc.c_str());

		D2WIN_SetControlText(box, wszLastDesc);
		delete[] wszLastDesc;
	}

	// original code
	D2WIN_SetEditBoxProc(box, FunCallBack);
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

void __declspec(naked) JoinNotice_Interception()
{
	/*
	Starts the countdown on the notice the lobby is about to show, in place of the
	store the client would have made.
	*/
	__asm
	{
		PUSHAD
		CALL [Bnet::SetJoinNotice]
		POPAD
		RET
	}
}

void __declspec(naked) EnterChatWait_Interception()
{
	/*
	Leaves the comparison the client would have made, against the wait that applies
	to the message this run is waiting on. Every register the run holds is live
	here, so nothing is touched, and the comparison is made last so that the jump
	back carries the flags the client's own branch reads.
	*/
	__asm
	{
		CMP ESI, MSG_ENTER_CHAT
		JNE stock

		CMP EAX, Bnet::enterChatWait
		JMP resume

	stock:
		CMP EAX, STOCK_ENTER_CHAT

	resume:
		JMP DWORD PTR [waitResume]
	}
}

// What the two waits above read. Each is reached where no setting can be read -
// one from a stub with every register live, one from a loop inside BNCLIENT - so
// the switch on each is answered here: off is the client's own wait, which the
// patch says as readily as it says a chosen one.
void Bnet::SetWaits() {
	failToJoin = *overrideFailToJoin ? failToJoinChoice : STOCK_FAIL_TO_JOIN;
	enterChatWait = *overrideEnterChat ? enterChatChoice : STOCK_ENTER_CHAT;
}

void Bnet::SetJoinNotice() {
	DWORD* frames = (DWORD*)Patch::GetDllOffset(D2CLIENT, NOTICE_FRAMES_113C);
	DWORD* notice = (DWORD*)Patch::GetDllOffset(D2CLIENT, NOTICE_ID_113C);

	const bool shorten = *overrideJoinNotice && *notice == NOTICE_FAILED_TO_JOIN;
	*frames = shorten ? joinNotice : STOCK_JOIN_NOTICE;
}
