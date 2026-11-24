#include "Shop.h"
#include <algorithm>
#include <utility>
#include "ShopHighlights.h"
#include "../Settings/SettingsRegistry.h"
#include "../Item/ItemDisplay.h"
#include "../Item/ItemFactsLive.h"
#include "../../BH.h"
#include "../../Common.h"
#include "../../Constants.h"
#include "../../D2Helpers.h"
#include "../../D2Ptrs.h"
#include "../../D2Stubs.h"
#include "../../Drawing.h"
#include "../../ItemDescription.h"
#include "../../Patch.h"

// What *p_D2CLIENT_ShopKind holds for each kind of trade window.
#define SHOP_KIND_GAMBLE	1
#define SHOP_KIND_TRADE		3

#define SHOP_TABS			4

// Where D2Client 1.13c hit-tests and draws the tab buttons of a trade window, at
// 0x9C0D9 and 0x95ED1: SHOP_TAB_WIDTH apart from the panel's left edge, between
// these two heights above the bottom of the screen, both before the panel's
// vertical offset is added.
#define SHOP_TAB_WIDTH		0x50
#define SHOP_TAB_TOP		0x1DF
#define SHOP_TAB_BOTTOM		0x1C1

// The marker drawn on a tab with a match: a square in the highlight colour, in
// a black frame so it reads on the tab art whatever the colour, centred on the
// tab's height and inset from its right edge.
#define TAB_MARKER_SIZE		8
#define TAB_MARKER_FRAME	1
#define TAB_MARKER_INSET	5

// Where an Items.txt record keeps the sound an item makes landing, a WORD.
// D2Client 1.13c reads it there at 0x81E80.
#define ITEMS_TXT_DROP_SOUND	0x124

// How long a window's items have to stay as they are before it is taken to have
// finished arriving, and what is on offer is announced.
#define ANNOUNCE_SETTLE_MS	400

#define DEFAULT_HIGHLIGHT_COLOR	0x84

void ItemBackdrop_Interception();

// The immediate of the `cmp eax, 0x1F4` in D2CLIENT_ShopAction (0x47D60) that
// holds back a click until 500 ms after the last trade went through. The check
// runs only for a ShopAction called with its fifth stack argument set, which
// D2Client does from two places, both clicks on an NPC's trade panel: a right
// click on one of the NPC's items, which buys it (0x9CA37), and a left click
// with an item on the cursor, which sells it (0x9C0AB). Buying through any other
// path, and repairing, never wait on it.
static Patch* buyCooldownPatch = new Patch(Bytes, D2CLIENT, { 0x47E54, 0 },
	DEFAULT_BUY_COOLDOWN, 4);

// The call to D2GFX_DrawRectangle inside D2Client's 0x8C600, which draws the
// backdrop under an item one grid cell at a time. See ItemBackdrop_Interception.
static Patch* itemBackdropPatch = new Patch(Call, D2CLIENT, { 0x8C678, 0 },
	(int)ItemBackdrop_Interception, 5);

// Where 0x8C600 returns to in the inventory grid drawing (0x958A0) after drawing
// an item that is not the one under the cursor. The one under the cursor keeps
// the game's hover colour.
#define GRID_BACKDROP_RETURN	0x95B59
static DWORD gridBackdropReturn = 0;

// The colour each of the open window's items is drawn in, by unit id and in id
// order. Written on the game loop and read by the drawing, both on the game's
// main thread.
static std::vector<std::pair<DWORD, BYTE>> backdrops;
static DWORD backdropOwner = 0;

static int settingsGeneration = 0;

static std::string Narrow(const wchar_t* text) {
	if (!text)
		return "";
	char* narrow = UnicodeToAnsi(text);
	std::string result(narrow);
	delete[] narrow;
	return result;
}

static std::string CodeOf(UnitAny* item) {
	ItemText* txt = D2COMMON_GetItemText(item->dwTxtFileNo);
	if (!txt)
		return "";
	std::string code(txt->szCode, 3);
	code.erase(code.find_last_not_of(' ') + 1);
	return code;
}

static bool TradeWindowOpen(BYTE kind) {
	return D2CLIENT_GetUIState(UI_NPCSHOP) && *p_D2CLIENT_ShopKind == kind;
}

static bool TabExists(int tab) {
	return *(DWORD*)(p_D2CLIENT_ShopTabs + SHOP_TAB_STRIDE * tab) != 0;
}

void Shop::OnLoad() {
	gridBackdropReturn = Patch::GetDllOffset(D2CLIENT, GRID_BACKDROP_RETURN);
	LoadConfig();

	Settings::AddHeading(GetName(), Settings::Category::Input, "Shopping");
	Settings::AddSlider(GetName(), Settings::Category::Input, "Buy Cooldown", "Buy cooldown",
		&buyCooldown, MIN_BUY_COOLDOWN, MAX_BUY_COOLDOWN, STEP_BUY_COOLDOWN, " ms",
		"How long the game waits after a trade before it takes the next right click "
		"to buy or the next item dropped on the NPC to sell. The game's own wait is 500 ms.",
		"", &shortenBuyCooldown);

	Settings::AddHeading(GetName(), Settings::Category::Input, Settings::Heading::Gambling);
	Settings::AddToggle(GetName(), Settings::Category::Input, "Gamble Refresh", "Gamble refresh",
		&refresh,
		"Rerolls the gamble window: closes it and asks the NPC for it again.");
	Settings::AddBool(GetName(), Settings::Category::Input, "Gamble Refresh Button",
		"Show refresh button", &refreshButton,
		"A button in the gamble window's empty slot. Shift+click refreshes until a marked "
		"base is on offer.", "Gamble Refresh");
	Settings::AddKey(GetName(), Settings::Category::Input, "Gamble Refresh Until Marked",
		"Refresh until marked", &refreshUntilKey,
		"Refreshes until a marked base is on offer. Press again to stop.", "Gamble Refresh");
	Settings::AddSlider(GetName(), Settings::Category::Input, "Gamble Refresh Cap",
		"Most refreshes", &refreshCap, MIN_REFRESH_CAP, MAX_REFRESH_CAP, STEP_REFRESH_CAP, "",
		"How many times refreshing until marked tries before it gives up.", "Gamble Refresh");
	Settings::AddSlider(GetName(), Settings::Category::Input, "Gamble Refresh Pause",
		"Pause between refreshes", &refreshPause, MIN_REFRESH_PAUSE, MAX_REFRESH_PAUSE,
		STEP_REFRESH_PAUSE, " ms",
		"How long refreshing until marked waits before the next refresh.", "Gamble Refresh");
	Settings::AddBool(GetName(), Settings::Category::Input, "Gamble Refresh Keep Tab",
		"Keep selected tab", &keepTab,
		"After a refresh, shows the tab that was showing before it.", "Gamble Refresh");
	Settings::AddKey(GetName(), Settings::Category::Input, "Gamble Mark",
		"Mark base in gamble window", &markKey,
		"Marks the base of the item under the cursor; every item of that base is highlighted.");
	Settings::AddKey(GetName(), Settings::Category::Input, "Gamble Clear Marks",
		"Clear gamble marks", &clearMarksKey);
	Settings::AddColor(GetName(), Settings::Category::Input, "Gamble Mark Color", "Mark color",
		&markColor);
	Settings::AddBool(GetName(), Settings::Category::Input, "Gamble Mark Tiers", "Match base tiers",
		&markTiers, "Marking a base also marks its exceptional and elite versions.");
	Settings::AddBool(GetName(), Settings::Category::Input, "Gamble Remember Marks", "Remember marks",
		&rememberMarks, "Keeps your marks between games.");
	Settings::AddText(GetName(), Settings::Category::Input, "Gamble Marks", "Marked bases",
		&marksText, 0, "Base codes, comma separated.", "Gamble Remember Marks");
	Settings::AddBool(GetName(), Settings::Category::Input, "Gamble Announce", "Announce marked items",
		&announceMarks,
		"After the window opens or refreshes, says which marked items are on offer.");

	Settings::AddHeading(GetName(), Settings::Category::Filter, "Shop highlights");
	Settings::AddToggle(GetName(), Settings::Category::Filter, "Shop Highlights", "Shop highlights",
		&highlights,
		"Colours the items in an NPC's shop that your item filter would mark on the map.");
	Settings::AddEnum(GetName(), Settings::Category::Filter, "Shop Highlight Color Source",
		"Highlight color", &highlightSource, { "Rule's map color", "Fixed color" }, "",
		"Shop Highlights");
	Settings::AddColor(GetName(), Settings::Category::Filter, "Shop Highlight Color", "Fixed color",
		&highlightColor, "", "Shop Highlights");
	Settings::AddBool(GetName(), Settings::Category::Filter, "Shop Highlight Tabs",
		"Mark tabs with matches", &markTabs, "", "Shop Highlights");
	Settings::AddBool(GetName(), Settings::Category::Filter, "Shop Highlight Announce",
		"Announce matches in chat", &announceMatches, "", "Shop Highlights");
}

void Shop::LoadConfig() {
	shortenBuyCooldown = false;
	BH::config->ReadBoolean("Shorten Buy Cooldown", shortenBuyCooldown);
	BH::config->ReadInt("Buy Cooldown", buyCooldown, DEFAULT_BUY_COOLDOWN);
	// The slider cannot offer a value outside its range, but a file can name one,
	// and below the floor is what the floor is there to prevent.
	if (buyCooldown < MIN_BUY_COOLDOWN)
		buyCooldown = MIN_BUY_COOLDOWN;
	if (buyCooldown > MAX_BUY_COOLDOWN)
		buyCooldown = MAX_BUY_COOLDOWN;

	BH::config->ReadToggle("Shop Highlights", "None", true, highlights);
	BH::config->ReadInt("Shop Highlight Color Source", highlightSource,
		ShopHighlights::ColorFromRule);
	if (highlightSource > ShopHighlights::ColorFixed)
		highlightSource = ShopHighlights::ColorFromRule;
	BH::config->ReadInt("Shop Highlight Color", highlightColor, DEFAULT_HIGHLIGHT_COLOR);
	markTabs = true;
	BH::config->ReadBoolean("Shop Highlight Tabs", markTabs);
	announceMatches = false;
	BH::config->ReadBoolean("Shop Highlight Announce", announceMatches);

	BH::config->ReadKey("Gamble Mark", "VK_F2", markKey);
	BH::config->ReadKey("Gamble Clear Marks", "None", clearMarksKey);
	BH::config->ReadInt("Gamble Mark Color", markColor, DEFAULT_HIGHLIGHT_COLOR);
	markTiers = true;
	BH::config->ReadBoolean("Gamble Mark Tiers", markTiers);
	rememberMarks = false;
	BH::config->ReadBoolean("Gamble Remember Marks", rememberMarks);
	marksText = "";
	BH::config->ReadString("Gamble Marks", marksText);
	announceMarks = true;
	BH::config->ReadBoolean("Gamble Announce", announceMarks);

	BH::config->ReadToggle("Gamble Refresh", "VK_F5", true, refresh);
	refreshButton = true;
	BH::config->ReadBoolean("Gamble Refresh Button", refreshButton);
	BH::config->ReadKey("Gamble Refresh Until Marked", "None", refreshUntilKey);
	BH::config->ReadInt("Gamble Refresh Cap", refreshCap, DEFAULT_REFRESH_CAP);
	if (refreshCap < MIN_REFRESH_CAP)
		refreshCap = MIN_REFRESH_CAP;
	if (refreshCap > MAX_REFRESH_CAP)
		refreshCap = MAX_REFRESH_CAP;
	// Below the floor is what the floor is there to prevent: refreshes fast
	// enough to flood the server.
	BH::config->ReadInt("Gamble Refresh Pause", refreshPause, DEFAULT_REFRESH_PAUSE);
	if (refreshPause < MIN_REFRESH_PAUSE)
		refreshPause = MIN_REFRESH_PAUSE;
	if (refreshPause > MAX_REFRESH_PAUSE)
		refreshPause = MAX_REFRESH_PAUSE;
	keepTab = true;
	BH::config->ReadBoolean("Gamble Refresh Keep Tab", keepTab);
}

void Shop::OnSettingsChanged(const vector<string>& keys) {
	bool textChanged = std::find(keys.begin(), keys.end(), "Gamble Marks") != keys.end();
	bool rememberChanged =
		std::find(keys.begin(), keys.end(), "Gamble Remember Marks") != keys.end();

	// While marks are remembered the text is what they are, so a change to it,
	// which a reload counts as, is read back. Turning remembering on keeps what
	// is marked in this game as well as what the text already held.
	if (rememberMarks && textChanged) {
		marks = GambleMarks::Parse(marksText);
		MarksChanged(false);
	} else if (rememberMarks && rememberChanged) {
		std::vector<std::string> kept = GambleMarks::Parse(marksText);
		for (unsigned int i = 0; i < marks.size(); i++) {
			if (std::find(kept.begin(), kept.end(), marks[i]) == kept.end())
				kept.push_back(marks[i]);
		}
		marks = kept;
		MarksChanged(true);
	} else {
		MarksChanged(false);
	}

	settingsGeneration++;
	ApplyPatches();
}

// Both patches stand in code only the game's main thread runs, which is also
// the thread the settings poll and the game loop call from, so neither is ever
// written under code that is running it.
void Shop::ApplyPatches() {
	buyCooldownPatch->SetValue(buyCooldown);
	if (shortenBuyCooldown)
		buyCooldownPatch->Install();
	else
		buyCooldownPatch->Remove();

	if (highlights.state || !marks.empty())
		itemBackdropPatch->Install();
	else
		itemBackdropPatch->Remove();
}

void Shop::OnUnload() {
	buyCooldownPatch->Remove();
	itemBackdropPatch->Remove();
	backdrops.clear();
}

void Shop::OnGameExit() {
	refresher.Reset();
	refreshBusy = false;
	windowDown = false;
	keepingPanels = false;
	ShowAutomapAgain();
	awaitingTalk = 0;
	pendingRefresh = RefreshNone;
	buttonHeld = false;
	Forget();
	if (!rememberMarks) {
		marks.clear();
		MarksChanged(false);
	}
}

void Shop::BuildFamilies() {
	if (familiesBuilt)
		return;
	const std::vector<const ItemDescription::Base*>& bases = ItemDescription::AllBases();
	if (bases.empty())
		return;

	std::vector<GambleMarks::BaseUpgrades> upgrades;
	for (unsigned int i = 0; i < bases.size(); i++) {
		GambleMarks::BaseUpgrades base;
		base.code = bases[i]->code;
		base.exceptional = bases[i]->exceptional;
		base.elite = bases[i]->elite;
		upgrades.push_back(base);
	}
	families = GambleMarks::FamiliesOf(upgrades);
	familiesBuilt = true;
}

void Shop::MarksChanged(bool writeText) {
	BuildFamilies();
	markedCodes = GambleMarks::Expand(marks, markTiers, families);
	if (writeText && rememberMarks)
		marksText = GambleMarks::Format(marks);
	settingsGeneration++;
	ApplyPatches();
}

void Shop::OnKey(bool up, BYTE key, LPARAM lParam, bool* block) {
	if (key == 0)
		return;

	if (key == highlights.toggle) {
		*block = true;
		// Only the flip: the settings poll on the game loop notices it and
		// installs or removes the patch from there.
		if (up)
			highlights.state = !highlights.state;
		return;
	}

	if (refresh.state && (key == refresh.toggle || key == refreshUntilKey)) {
		// A refresh closes the window it is refreshing, so while one is under
		// way the key still belongs to it, to stop it.
		if (!refreshBusy && !TradeWindowOpen(SHOP_KIND_GAMBLE))
			return;
		*block = true;
		// A held key repeats, and the repeat of the press that started a run
		// would stop it.
		bool repeat = (lParam & 0x40000000) != 0;
		if (!up && !repeat)
			RequestRefresh(key == refreshUntilKey);
		return;
	}

	if (key != markKey && key != clearMarksKey)
		return;
	if (!TradeWindowOpen(SHOP_KIND_GAMBLE))
		return;

	*block = true;
	if (up)
		return;
	if (key == markKey)
		pendingMark = true;
	else
		pendingClear = true;
}

void Shop::OnLeftClick(bool up, unsigned int x, unsigned int y, bool* block) {
	// The window drawn while a refresh has it down is only a picture: a click
	// on it would reach the world behind and walk the player off.
	bool held = windowDown;
	if (!up) {
		GambleRefresh::Box box = GambleRefresh::SlotBox(*p_D2CLIENT_PanelOffsetX,
			*p_D2CLIENT_PanelOffsetY, *p_D2CLIENT_ScreenSizeY);
		if (!RefreshButtonShown() || !box.Contains((int)x, (int)y)) {
			if (held)
				*block = true;
			return;
		}
		*block = true;
		buttonHeld = true;
		return;
	}

	// The game was kept from seeing the press, so it is kept from seeing the
	// release too, wherever it lands.
	if (!buttonHeld.exchange(false)) {
		if (held)
			*block = true;
		return;
	}
	*block = true;
	if (!RefreshButtonShown())
		return;
	GambleRefresh::Box box = GambleRefresh::SlotBox(*p_D2CLIENT_PanelOffsetX,
		*p_D2CLIENT_PanelOffsetY, *p_D2CLIENT_ScreenSizeY);
	if (box.Contains((int)x, (int)y))
		RequestRefresh((GetKeyState(VK_SHIFT) & 0x8000) != 0);
}

void Shop::OnRightClick(bool up, unsigned int x, unsigned int y, bool* block) {
	if (windowDown)
		*block = true;
}

void Shop::RequestRefresh(bool untilMarked) {
	pendingRefresh = untilMarked ? RefreshUntilMarked : RefreshOnce;
}

// Shown on the gamble window, and on the one drawn in its place while a refresh
// has it down.
bool Shop::RefreshButtonShown() {
	return refresh.state && refreshButton &&
		(TradeWindowOpen(SHOP_KIND_GAMBLE) || windowDown) &&
		*p_D2CLIENT_ShopButtonCells != NULL;
}

void Shop::StartRefresh(bool untilMarked) {
	if (!TradeWindowOpen(SHOP_KIND_GAMBLE))
		return;
	if (untilMarked && markedCodes.empty()) {
		std::string key = GetKeyCode(markKey).literalName;
		PrintText(Gold, (char*)"Mark a base to refresh for first, with %s on an item.",
			key.c_str());
		return;
	}
	UnitAny* npc = D2CLIENT_GetCurrentInteractingNPC();
	if (!npc)
		return;
	if (!refresher.Start(GetTickCount(), untilMarked, refreshCap, refreshPause))
		return;
	refreshNpcId = npc->dwUnitId;
	refreshTab = *p_D2CLIENT_ShopTab;
	refreshBusy = true;
}

// Whether the NPC can still be talked to: the client still knows it, and the
// player is standing rather than walking off somewhere else.
static bool NpcHere(DWORD npcId) {
	UnitAny* player = D2CLIENT_GetPlayerUnit();
	if (!player || !D2CLIENT_FindServerSideUnit(npcId, UNIT_MONSTER))
		return false;
	DWORD mode = player->dwMode;
	return mode != PLAYER_MODE_WALK_OUTTOWN && mode != PLAYER_MODE_RUN &&
		mode != PLAYER_MODE_WALK_INTOWN;
}

void Shop::TickRefresh() {
	int request = pendingRefresh.exchange(RefreshNone);
	if (request != RefreshNone) {
		if (refresher.Busy())
			refresher.Stop();
		else if (request != RefreshStop)
			StartRefresh(request == RefreshUntilMarked);
	}
	if (!refresher.Busy())
		return;

	GambleRefresh::View view;
	view.gambleOpen = TradeWindowOpen(SHOP_KIND_GAMBLE);
	view.npcHere = NpcHere(refreshNpcId);
	view.talkAnswered = talkAnswered;
	UnitAny* npc = view.gambleOpen ? D2CLIENT_GetCurrentInteractingNPC() : NULL;
	bool ours = npc && npc->dwUnitId == refreshNpcId && npc->pInventory;
	view.items = ours ? npc->pInventory->dwItemCount : 0;
	// Rebuilt from the window's contents earlier in this same loop.
	view.marked = ours && npcId == refreshNpcId && !backdrops.empty();

	switch (refresher.Tick(GetTickCount(), view)) {
	case GambleRefresh::StepTakeDown:
		TakeWindowDown();
		break;
	case GambleRefresh::StepTalk: {
		talkAnswered = false;
		awaitingTalk = refreshNpcId;
		BYTE packet[9];
		packet[0] = 0x13;
		*(DWORD*)&packet[1] = UNIT_MONSTER;
		*(DWORD*)&packet[5] = refreshNpcId;
		D2NET_SendPacket(sizeof(packet), 0, packet);
		break;
	}
	case GambleRefresh::StepGamble:
		D2CLIENT_StartGamble();
		break;
	case GambleRefresh::StepRestoreTab:
		// Also recounts which tabs have anything on them, which the game
		// otherwise only does when the window opens.
		D2CLIENT_SelectShopTab(keepTab ? refreshTab : 0);
		break;
	case GambleRefresh::StepAbandon:
		awaitingTalk = 0;
		D2CLIENT_CloseNPCInteract();
		break;
	default:
		break;
	}

	GambleRefresh::Outcome outcome = refresher.TakeOutcome();
	if (outcome != GambleRefresh::OutcomeNone)
		RefreshFinished(outcome);
	refreshBusy = refresher.Busy();
	GambleRefresh::State state = refresher.GetState();
	windowDown = !TradeWindowOpen(SHOP_KIND_GAMBLE) &&
		(state == GambleRefresh::StateTalking || state == GambleRefresh::StateStocking);
	if (windowDown) {
		*p_D2CLIENT_PanelsOpen = panelsOpen;
	} else {
		// Opening the window again sets the panels itself. A refresh that
		// gave up with it down leaves them as the window's closing did.
		if (keepingPanels && !TradeWindowOpen(SHOP_KIND_GAMBLE))
			*p_D2CLIENT_PanelsOpen = panelsClosed;
		keepingPanels = false;
		ShowAutomapAgain();
	}
}

// The automap would otherwise show through for as long as the window is down.
void Shop::HideAutomap() {
	if (automapHidden)
		return;
	automapWasOn = *p_D2CLIENT_AutomapOn;
	*p_D2CLIENT_AutomapOn = 0;
	automapHidden = true;
}

void Shop::ShowAutomapAgain() {
	if (!automapHidden)
		return;
	*p_D2CLIENT_AutomapOn = automapWasOn;
	automapHidden = false;
}

// Takes the gamble window down the way the game's own close starts out
// (D2Client 0x492F0), without going on to end the talk: the next talk replaces
// it, and its menu is where gamble is picked again.
void Shop::TakeWindowDown() {
	HideAutomap();
	*p_D2CLIENT_SelectedInvItem = NULL;
	panelsOpen = *p_D2CLIENT_PanelsOpen;
	D2CLIENT_SetUIVar(UI_NPCSHOP, 1, 0);
	panelsClosed = *p_D2CLIENT_PanelsOpen;
	keepingPanels = true;
	// Closing clears the trade window's cursor mode, which the trade screen
	// drawn in the window's place goes by, and the panels it had open, which
	// the borders around them are drawn by.
	*p_D2CLIENT_ShopCursorType = 1;
	*p_D2CLIENT_PanelsOpen = panelsOpen;
	Forget();
}

// The server's answer to the talk a refresh asks for (D2Client 0xAFA70, then
// 0x4B8A0), after which gamble can be picked. Read on whichever thread the
// game hands packets over on, so it only trades atomics with the game loop.
void Shop::OnGamePacketRecv(BYTE* packet, bool* block) {
	if (packet[0] != 0x28 || packet[1] != UNIT_MONSTER)
		return;
	DWORD waiting = awaitingTalk;
	if (waiting == 0 || *(DWORD*)&packet[2] != waiting)
		return;
	awaitingTalk = 0;
	talkAnswered = true;
}

void Shop::RefreshFinished(GambleRefresh::Outcome outcome) {
	unsigned int count = refresher.Refreshes();
	const char* plural = count == 1 ? "" : "es";
	switch (outcome) {
	case GambleRefresh::OutcomeFound:
		PrintText(Gold, (char*)"Found a marked base after %u refresh%s.", count, plural);
		break;
	case GambleRefresh::OutcomeCapped:
		PrintText(Gold, (char*)"No marked base after %u refresh%s.", count, plural);
		break;
	case GambleRefresh::OutcomeStopped:
		if (refresher.Cap() > 1 || count > 1)
			PrintText(Gold, (char*)"Stopped refreshing after %u refresh%s.", count, plural);
		break;
	case GambleRefresh::OutcomeLost:
		PrintText(Red, (char*)"Refresh stopped: the NPC is out of reach. Talk to them again.");
		break;
	case GambleRefresh::OutcomeTimedOut:
		PrintText(Red, (char*)"Refresh stopped: the NPC did not answer. Talk to them again.");
		break;
	default:
		break;
	}
}

void Shop::ApplyPendingKeys() {
	if (pendingClear.exchange(false) && !marks.empty()) {
		marks.clear();
		MarksChanged(true);
	}
	if (pendingMark.exchange(false))
		MarkHovered();
}

void Shop::MarkHovered() {
	if (!TradeWindowOpen(SHOP_KIND_GAMBLE))
		return;
	UnitAny* item = *p_D2CLIENT_SelectedInvItem;
	if (!item || item->dwType != UNIT_ITEM)
		return;
	std::string code = CodeOf(item);
	if (code.empty())
		return;

	BuildFamilies();
	GambleMarks::Toggle(marks, code, markTiers, families);
	MarksChanged(true);
}

void Shop::Forget() {
	npcId = 0;
	npcKind = 0;
	contentsKey = 0;
	announced = false;
	matchNames.clear();
	dropSound = 0;
	for (int i = 0; i < SHOP_TABS; i++)
		tabColor[i] = -1;
	backdrops.clear();
	backdropOwner = 0;
}

// Everything that decides what Rebuild would make of the window, folded into
// one number: the items, the rules they were judged by and the settings that
// colour them.
unsigned int Shop::ContentsKey(UnitAny* npc) {
	unsigned int key = 2166136261u;
	auto fold = [&key](DWORD value) {
		key = (key ^ value) * 16777619u;
	};
	fold(npc->dwUnitId);
	fold(*p_D2CLIENT_ShopKind);
	fold(ItemVerdictsGeneration());
	fold(settingsGeneration);
	fold(npc->pInventory->dwItemCount);
	for (UnitAny* item = npc->pInventory->pFirstItem; item && item->pItemData;
			item = item->pItemData->pNextInvItem) {
		fold(item->dwUnitId);
		fold(item->pItemData->dwFlags);
	}
	return key;
}

void Shop::OnLoop() {
	ApplyPendingKeys();

	// The tables are read off the main thread after BH loads, so marks read
	// before then are matched without their tiers until the tables are there.
	if (!familiesBuilt && !marks.empty()) {
		BuildFamilies();
		if (familiesBuilt)
			MarksChanged(false);
	}

	FollowWindow();
	TickRefresh();
}

void Shop::FollowWindow() {
	UnitAny* npc = D2CLIENT_GetUIState(UI_NPCSHOP) ? D2CLIENT_GetCurrentInteractingNPC() : NULL;
	if (!npc || !npc->pInventory) {
		if (npcId)
			Forget();
		return;
	}

	DWORD now = GetTickCount();
	BYTE kind = *p_D2CLIENT_ShopKind;
	if (npc->dwUnitId != npcId || kind != npcKind) {
		Forget();
		npcId = npc->dwUnitId;
		npcKind = kind;
		contentsChangedAt = now;
	}

	unsigned int key = ContentsKey(npc);
	if (key != contentsKey) {
		contentsKey = key;
		contentsChangedAt = now;
		Rebuild(npc);
	}

	if (!announced && npc->pInventory->dwItemCount > 0 &&
			now - contentsChangedAt >= ANNOUNCE_SETTLE_MS) {
		announced = true;
		Announce(npc);
	}
}

void Shop::Rebuild(UnitAny* npc) {
	backdrops.clear();
	backdropOwner = npc->dwUnitId;
	matchNames.clear();
	dropSound = 0;
	for (int i = 0; i < SHOP_TABS; i++)
		tabColor[i] = -1;

	bool trade = npcKind == SHOP_KIND_TRADE && highlights.state;
	bool gamble = npcKind == SHOP_KIND_GAMBLE && !markedCodes.empty();
	if (!trade && !gamble)
		return;

	for (UnitAny* item = npc->pInventory->pFirstItem; item && item->pItemData;
			item = item->pItemData->pNextInvItem) {
		unsigned int color;
		if (trade) {
			LiveItem live(item);
			if (!live.Known())
				continue;
			std::vector<const Action*> actions = GetItemMapActions(&live.Unit());
			if (actions.empty())
				continue;
			color = ShopHighlights::BackdropColor(actions,
				(ShopHighlights::ColorSource)highlightSource, highlightColor);
		} else {
			if (markedCodes.find(CodeOf(item)) == markedCodes.end())
				continue;
			color = markColor;
			ItemText* txt = D2COMMON_GetItemText(item->dwTxtFileNo);
			if (txt && dropSound == 0)
				dropSound = *(WORD*)((BYTE*)txt + ITEMS_TXT_DROP_SOUND);
		}

		backdrops.push_back(std::make_pair(item->dwUnitId, (BYTE)color));
		BYTE tab = item->pItemData->ItemLocation;
		if (tab < SHOP_TABS && tabColor[tab] < 0)
			tabColor[tab] = color;
		matchNames.push_back(ItemDescription::NameLine(GetItemName(item)) + "\377c0");
	}
	std::sort(backdrops.begin(), backdrops.end());
}

void Shop::Announce(UnitAny* npc) {
	if (matchNames.empty())
		return;

	std::string name = Narrow(D2CLIENT_GetUnitName(npc));
	if (npcKind == SHOP_KIND_TRADE && highlights.state && announceMatches) {
		Print((char*)"%s", ShopHighlights::AnnounceLine(name, "item for you",
			"items for you", matchNames).c_str());
	} else if (npcKind == SHOP_KIND_GAMBLE && announceMarks) {
		Print((char*)"%s", ShopHighlights::AnnounceLine(name, "marked item",
			"marked items", matchNames).c_str());
		if (dropSound > 0)
			D2CLIENT_PlaySound(dropSound);
	}
}

int Shop::BackdropColorFor(UnitAny* item) {
	if (backdrops.empty() || !item || item->dwType != UNIT_ITEM || !item->pItemData)
		return -1;
	Inventory* inventory = item->pItemData->pOwnerInventory;
	if (!inventory || !inventory->pOwner || inventory->pOwner->dwType != UNIT_MONSTER ||
			inventory->pOwner->dwUnitId != backdropOwner)
		return -1;

	std::vector<std::pair<DWORD, BYTE>>::const_iterator found = std::lower_bound(
		backdrops.begin(), backdrops.end(), std::make_pair(item->dwUnitId, (BYTE)0));
	if (found == backdrops.end() || found->first != item->dwUnitId)
		return -1;
	return found->second;
}

void Shop::OnDraw() {
	DrawWindowInPlace();
	DrawTabMarkers();
	DrawRefreshButton();
}

// While a refresh has the gamble window down, the game draws the world where it
// was. Drawing what the game draws for an open trade window in its place keeps
// the window on screen until the new one is up. The borders around it the game
// still draws itself, from the panels kept open meanwhile.
void Shop::DrawWindowInPlace() {
	if (!windowDown || TradeWindowOpen(SHOP_KIND_GAMBLE))
		return;
	D2CLIENT_DrawTradeScreen();
	D2CLIENT_DrawLifeGlobe();
	D2CLIENT_DrawManaGlobe();
}

// Drawn over the empty slot the game leaves in a gamble window, with the game's
// own blank button and the same call and position the game draws its buttons
// with, so only the icon on it is ours.
void Shop::DrawRefreshButton() {
	if (!RefreshButtonShown())
		return;

	int panelX = *p_D2CLIENT_PanelOffsetX;
	int panelY = *p_D2CLIENT_PanelOffsetY;
	int screenHeight = *p_D2CLIENT_ScreenSizeY;
	GambleRefresh::Box box = GambleRefresh::SlotBox(panelX, panelY, screenHeight);
	int mouseX = *p_D2CLIENT_MouseX;
	int mouseY = *p_D2CLIENT_MouseY;
	bool hovered = box.Contains(mouseX, mouseY);
	bool down = (buttonHeld && hovered) || refresher.Busy();

	// The game's cell context is 0x48 bytes, zeroed, with the frame first and
	// the cell file at 0x34.
	struct {
		CellContext context;
		DWORD rest[4];
	} cell = {};
	cell.context.nCellNo = down ? 1 : 0;
	cell.context.pCellFile = *p_D2CLIENT_ShopButtonCells;
	int x = GambleRefresh::ArtX(panelX);
	int bottom = GambleRefresh::ArtBottom(panelY, screenHeight);
	D2GFX_DrawAutomapCell2(&cell.context, x, bottom, (DWORD)-1, DRAW_MODE_NORMAL, NULL);

	int top = bottom - GambleRefresh::GLYPH_SIZE;
	const std::vector<GambleRefresh::Run>& glyph = GambleRefresh::Glyph(down);
	for (unsigned int i = 0; i < glyph.size(); i++) {
		const GambleRefresh::Run& run = glyph[i];
		D2GFX_DrawRectangle(x + run.x, top + run.y, x + run.x + run.length, top + run.y + 1,
			run.color, Drawing::BTNormal);
	}

	if (!hovered)
		return;
	std::string name = "Refresh";
	if (refresh.toggle != 0)
		name += " (" + GetKeyCode(refresh.toggle).literalName + ")";
	char text[128];
	if (refresher.Running()) {
		sprintf_s(text, sizeof(text), "Refreshing: %u of %u. Click to stop",
			refresher.Refreshes() + 1, refresher.Cap());
	} else if (refresher.Busy()) {
		sprintf_s(text, sizeof(text), "Refreshing");
	} else if (!markedCodes.empty()) {
		sprintf_s(text, sizeof(text), "%s. Shift+click: until marked", name.c_str());
	} else {
		sprintf_s(text, sizeof(text), "%s", name.c_str());
	}
	wchar_t* wide = AnsiToUnicode(text);
	// The font the trade window draws its buttons' tooltips in.
	DWORD font = D2WIN_SetTextSize(1);
	D2WIN_DrawFramedText(wide, GambleRefresh::TooltipX(panelX),
		GambleRefresh::TooltipY(panelY, screenHeight), 0, 0);
	D2WIN_SetTextSize(font);
	delete[] wide;
}

void Shop::DrawTabMarkers() {
	if (!markTabs || !highlights.state || !TradeWindowOpen(SHOP_KIND_TRADE) || !npcId)
		return;

	int tabs = 0;
	for (int i = 0; i < SHOP_TABS; i++)
		tabs += TabExists(i) ? 1 : 0;
	if (tabs < 2)
		return;

	int top = *p_D2CLIENT_PanelOffsetY + *p_D2CLIENT_ScreenSizeY - SHOP_TAB_TOP;
	int height = SHOP_TAB_TOP - SHOP_TAB_BOTTOM;
	int y = top + (height - TAB_MARKER_SIZE) / 2;
	for (int i = 0; i < SHOP_TABS; i++) {
		if (!TabExists(i) || tabColor[i] < 0)
			continue;
		int x = *p_D2CLIENT_PanelOffsetX + SHOP_TAB_WIDTH * (i + 1) - TAB_MARKER_INSET -
			TAB_MARKER_SIZE;
		Drawing::Boxhook::Draw(x, y, TAB_MARKER_SIZE, TAB_MARKER_SIZE, 0, Drawing::BTNormal);
		Drawing::Boxhook::Draw(x + TAB_MARKER_FRAME, y + TAB_MARKER_FRAME,
			TAB_MARKER_SIZE - 2 * TAB_MARKER_FRAME, TAB_MARKER_SIZE - 2 * TAB_MARKER_FRAME,
			tabColor[i], Drawing::BTNormal);
	}
}

static void __stdcall DrawItemBackdrop(DWORD gridReturn, UnitAny* item,
		int x1, int y1, int x2, int y2, DWORD color, DWORD trans) {
	if (gridReturn == gridBackdropReturn) {
		int custom = Shop::BackdropColorFor(item);
		if (custom >= 0)
			color = custom;
	}
	D2GFX_DrawRectangle(x1, y1, x2, y2, color, trans);
}

// D2Client's 0x8C600 is called by the inventory grid drawing with the item in
// ebx, and saves ebx before anything else, so at this call the item sits at
// [esp+0x24] and 0x8C600's own return address at [esp+0x34]. Other callers reach
// 0x8C600 with something else in ebx (the cursor's placement backdrop, 0x8E440,
// passes a byte there), which is why the return address is checked before the
// saved ebx is read as an item.
void __declspec(naked) ItemBackdrop_Interception() {
	__asm {
		push [esp + 0x18]
		push [esp + 0x18]
		push [esp + 0x18]
		push [esp + 0x18]
		push [esp + 0x18]
		push [esp + 0x18]
		push [esp + 0x3C]
		push [esp + 0x50]
		call DrawItemBackdrop
		ret 0x18
	}
}
