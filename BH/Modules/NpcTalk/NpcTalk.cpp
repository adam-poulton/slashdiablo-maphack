#include "NpcTalk.h"
#include <cstddef>
#include "../Settings/SettingsRegistry.h"
#include "../../BH.h"
#include "../../D2Ptrs.h"
#include "../../Patch.h"

// What picking a line of the talk menu calls in D2Client 1.13c, from the menu
// table at 0xF3BA0: trade, which is also trade/repair; gamble; and Cain's
// identify items. Each takes no arguments and acts on the NPC being talked to.
#define PICK_TRADE		0x4AEE0
#define PICK_GAMBLE		0x4ABE0
#define PICK_IDENTIFY	0x46AF0

static_assert(offsetof(DialogLine, fnPick) == 0x108, "DialogLine layout");
static_assert(sizeof(DialogLine) == 0x110, "DialogLine layout");
static_assert(offsetof(Dialog, dwLines) == 0x50, "Dialog layout");
static_assert(offsetof(Dialog, Lines) == 0x68, "Dialog layout");
static_assert(sizeof(Dialog) == 0xB10, "Dialog layout");

// The talk menu while a line of it can be picked, which is while it is the
// dialog on top: picking a line takes down whichever dialog that is.
static Dialog* OpenMenu() {
	Dialog* menu = *p_D2CLIENT_TalkMenu;
	return menu && menu == *p_D2CLIENT_TopDialog ? menu : NULL;
}

static std::vector<TalkMenu::Line> LinesOf(Dialog* menu) {
	std::vector<TalkMenu::Line> lines;
	if (!menu)
		return lines;
	unsigned int count = menu->dwLines < DIALOG_MAX_LINES ? menu->dwLines : DIALOG_MAX_LINES;
	for (unsigned int i = 0; i < count; i++) {
		TalkMenu::Line line;
		line.handler = (unsigned int)menu->Lines[i].fnPick;
		line.selectable = menu->Lines[i].bSelectable != 0;
		lines.push_back(line);
	}
	return lines;
}

void NpcTalk::OnLoad() {
	pickTrade = Patch::GetDllOffset(D2CLIENT, PICK_TRADE);
	pickGamble = Patch::GetDllOffset(D2CLIENT, PICK_GAMBLE);
	pickIdentify = Patch::GetDllOffset(D2CLIENT, PICK_IDENTIFY);
	LoadConfig();

	Settings::AddHeading(GetName(), Settings::Category::Input, "NPC menu");
	Settings::AddBool(GetName(), Settings::Category::Input, "Cain Auto Identify",
		"Identify items at Cain automatically", &autoIdentify,
		"Picks Cain's identify items for you as soon as you talk to him, whenever he "
		"offers it. Where he charges for it, his fee is paid as if you had picked it.");
	Settings::AddKey(GetName(), Settings::Category::Input, "Npc Trade", "Trade with NPC",
		&tradeKey, "With an NPC's menu up, picks trade or trade/repair.");
	Settings::AddKey(GetName(), Settings::Category::Input, "Npc Gamble", "Gamble with NPC",
		&gambleKey, "With an NPC's menu up, picks gamble.");
}

void NpcTalk::LoadConfig() {
	autoIdentify = false;
	BH::config->ReadBoolean("Cain Auto Identify", autoIdentify);
	BH::config->ReadKey("Npc Trade", "None", tradeKey);
	BH::config->ReadKey("Npc Gamble", "None", gambleKey);
}

void NpcTalk::OnGameExit() {
	identifyOnce.Reset();
	tradeOffered = false;
	gambleOffered = false;
	pendingTrade = false;
	pendingGamble = false;
}

void NpcTalk::OnKey(bool up, BYTE key, LPARAM lParam, bool* block) {
	if (key == 0)
		return;

	std::atomic<bool>* pending = NULL;
	if (key == tradeKey && tradeOffered)
		pending = &pendingTrade;
	else if (key == gambleKey && gambleOffered)
		pending = &pendingGamble;
	if (!pending)
		return;

	*block = true;
	// A held key repeats, and only the first press picks.
	bool repeat = (lParam & 0x40000000) != 0;
	if (!up && !repeat)
		*pending = true;
}

void NpcTalk::OnLoop() {
	bool wantTrade = pendingTrade.exchange(false);
	bool wantGamble = pendingGamble.exchange(false);

	Dialog* menu = OpenMenu();
	std::vector<TalkMenu::Line> lines = LinesOf(menu);
	int trade = TalkMenu::Find(lines, pickTrade);
	int gamble = TalkMenu::Find(lines, pickGamble);
	int identify = TalkMenu::Find(lines, pickIdentify);
	tradeOffered = trade >= 0;
	gambleOffered = gamble >= 0;

	// Identifying puts up the box that waits on the server's answer, and the
	// game stops on finding one already up (0x46B0A).
	bool canIdentify = autoIdentify && identify >= 0 && !*p_D2CLIENT_TradeWaitDialog;
	UnitAny* npc = D2CLIENT_GetCurrentInteractingNPC();
	bool identifyNow = identifyOnce.Take(npc != NULL, npc ? npc->dwUnitId : 0, canIdentify);

	int pick = identifyNow ? identify : wantTrade ? trade : wantGamble ? gamble : -1;
	if (pick >= 0)
		menu->Lines[pick].fnPick();
}
