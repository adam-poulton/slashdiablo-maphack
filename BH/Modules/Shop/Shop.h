#pragma once
#include <atomic>
#include <set>
#include <string>
#include <vector>
#include "../Module.h"
#include "../../Config.h"
#include "GambleMarks.h"

struct UnitAny;

// The buy cooldown, in milliseconds, and what one notch of its slider moves.
// The floor keeps any setting from flooding the server with buy packets; the
// ceiling is the game's own wait.
#define MIN_BUY_COOLDOWN		100
#define MAX_BUY_COOLDOWN		500
#define STEP_BUY_COOLDOWN		50
#define DEFAULT_BUY_COOLDOWN	200

// What an NPC's trade window is made to say: the shop's items the item filter
// would mark on the map, the gamble window's items of a base the player has
// marked, and how soon the game lets the next buy or sell through.
class Shop : public Module {
public:
	Shop() : Module("Shop"), npcId(0), npcKind(0), contentsKey(0),
		contentsChangedAt(0), announced(false), familiesBuilt(false),
		pendingMark(false), pendingClear(false) {};

	void OnLoad();
	void OnUnload();
	void LoadConfig();
	void OnSettingsChanged(const vector<string>& keys);

	void OnLoop();
	void OnDraw();
	void OnGameExit();
	void OnKey(bool up, BYTE key, LPARAM lParam, bool* block);

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

	// Set from the input thread, carried out on the game loop.
	std::atomic<bool> pendingMark;
	std::atomic<bool> pendingClear;

	void ApplyPatches();
	void ApplyPendingKeys();
	void MarkHovered();
	void MarksChanged(bool writeText);
	void BuildFamilies();

	void Forget();
	unsigned int ContentsKey(UnitAny* npc);
	void Rebuild(UnitAny* npc);
	void Announce(UnitAny* npc);
	void DrawTabMarkers();
};
