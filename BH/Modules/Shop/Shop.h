#pragma once
#include <atomic>
#include <set>
#include <string>
#include <vector>
#include "../Module.h"
#include "../../Config.h"
#include "GambleMarks.h"
#include "GambleRefresh.h"

struct UnitAny;

// The buy cooldown, in milliseconds, and what one notch of its slider moves.
// The floor keeps any setting from flooding the server with buy packets; the
// ceiling is the game's own wait.
#define MIN_BUY_COOLDOWN		100
#define MAX_BUY_COOLDOWN		500
#define STEP_BUY_COOLDOWN		50
#define DEFAULT_BUY_COOLDOWN	200

// How many refreshes a refresh until marked makes at most, and how long it
// waits between them, in milliseconds.
#define MIN_REFRESH_CAP			5
#define MAX_REFRESH_CAP			200
#define STEP_REFRESH_CAP		5
#define DEFAULT_REFRESH_CAP		25
#define MIN_REFRESH_PAUSE		250
#define MAX_REFRESH_PAUSE		3000
#define STEP_REFRESH_PAUSE		250
#define DEFAULT_REFRESH_PAUSE	1000

// What an NPC's trade window is made to say: the shop's items the item filter
// would mark on the map, the gamble window's items of a base the player has
// marked, and how soon the game lets the next buy or sell through. A gamble
// window can also be rerolled, from a key or a button of its own.
class Shop : public Module {
public:
	Shop() : Module("Shop"), npcId(0), npcKind(0), contentsKey(0),
		contentsChangedAt(0), announced(false), familiesBuilt(false),
		pendingMark(false), pendingClear(false), refreshNpcId(0),
		refreshTab(0), automapHidden(false), automapWasOn(0),
		panelsOpen(0), panelsClosed(0), keepingPanels(false), pendingRefresh(RefreshNone), buttonHeld(false), refreshBusy(false),
		windowDown(false), awaitingTalk(0), talkAnswered(false) {};

	void OnLoad();
	void OnUnload();
	void LoadConfig();
	void OnSettingsChanged(const vector<string>& keys);

	void OnLoop();
	void OnDraw();
	void OnGameExit();
	void OnKey(bool up, BYTE key, LPARAM lParam, bool* block);
	void OnLeftClick(bool up, unsigned int x, unsigned int y, bool* block);
	void OnRightClick(bool up, unsigned int x, unsigned int y, bool* block);
	void OnGamePacketRecv(BYTE* packet, bool* block);

	// The palette colour to draw an item's backdrop in, or -1 to leave it as the
	// game drew it. Asked once per grid cell per frame, so it only looks up.
	static int BackdropColorFor(UnitAny* item);

private:
	// Settings.
	bool shortenBuyCooldown;
	unsigned int buyCooldown;
	Toggle highlights;
	unsigned int highlightSource;
	unsigned int highlightColor;
	bool markTabs;
	bool announceMatches;
	unsigned int markKey;
	unsigned int clearMarksKey;
	unsigned int markColor;
	bool markTiers;
	bool rememberMarks;
	std::string marksText;
	bool announceMarks;
	Toggle refresh;
	bool refreshButton;
	unsigned int refreshUntilKey;
	unsigned int refreshCap;
	unsigned int refreshPause;
	bool keepTab;

	// The window open now, and what was last worked out about it.
	DWORD npcId;
	BYTE npcKind;
	unsigned int contentsKey;
	DWORD contentsChangedAt;
	bool announced;
	int tabColor[4];		// the first match's colour on each tab, -1 for none
	std::vector<std::string> matchNames;
	unsigned int dropSound;

	std::vector<std::string> marks;
	std::set<std::string> markedCodes;
	GambleMarks::TierFamilies families;
	bool familiesBuilt;

	// The gamble window being refreshed: its NPC, and the tab it had selected.
	GambleRefresh::Machine refresher;
	DWORD refreshNpcId;
	BYTE refreshTab;
	bool automapHidden;
	DWORD automapWasOn;
	// The side panels open before a refresh took the window down, and as taking
	// it down left them; the first is kept while it is down.
	DWORD panelsOpen;
	DWORD panelsClosed;
	bool keepingPanels;

	// Set from the input thread, carried out on the game loop.
	enum RefreshRequest { RefreshNone, RefreshOnce, RefreshUntilMarked, RefreshStop };
	std::atomic<bool> pendingMark;
	std::atomic<bool> pendingClear;
	std::atomic<int> pendingRefresh;
	// The refresh button is down: pressed on it, not yet let go.
	std::atomic<bool> buttonHeld;
	// A refresh is under way, and it has the window down while the new one
	// comes.
	std::atomic<bool> refreshBusy;
	std::atomic<bool> windowDown;
	// The NPC whose answer to a refresh's talk is still to come, and whether it
	// has come: set on the game loop, answered from the packet handler.
	std::atomic<DWORD> awaitingTalk;
	std::atomic<bool> talkAnswered;

	void ApplyPatches();
	void RequestRefresh(bool untilMarked);
	void StartRefresh(bool untilMarked);
	void TickRefresh();
	void RefreshFinished(GambleRefresh::Outcome outcome);
	void TakeWindowDown();
	void DrawWindowInPlace();
	void HideAutomap();
	void ShowAutomapAgain();
	bool RefreshButtonShown();
	void DrawRefreshButton();
	void ApplyPendingKeys();
	void MarkHovered();
	void MarksChanged(bool writeText);
	void BuildFamilies();

	void FollowWindow();
	void Forget();
	unsigned int ContentsKey(UnitAny* npc);
	void Rebuild(UnitAny* npc);
	void Announce(UnitAny* npc);
	void DrawTabMarkers();
};
