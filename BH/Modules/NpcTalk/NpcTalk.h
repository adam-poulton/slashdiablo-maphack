#pragma once
#include <atomic>
#include "../Module.h"
#include "TalkMenu.h"

// Picks lines of an NPC's talk menu for the player: Cain's offer to identify
// their items as soon as he makes it, and trade and gamble from a key.
class NpcTalk : public Module {
public:
	NpcTalk() : Module("Npc Talk"), autoIdentify(false), tradeKey(0), gambleKey(0),
		pickTrade(0), pickGamble(0), pickIdentify(0), tradeOffered(false),
		gambleOffered(false), pendingTrade(false), pendingGamble(false) {};

	void OnLoad();
	void LoadConfig();

	void OnLoop();
	void OnGameExit();
	void OnKey(bool up, BYTE key, LPARAM lParam, bool* block);

private:
	// Settings.
	bool autoIdentify;
	unsigned int tradeKey;
	unsigned int gambleKey;

	// The game's functions behind the lines, as the menu holds them.
	unsigned int pickTrade;
	unsigned int pickGamble;
	unsigned int pickIdentify;

	TalkMenu::OncePerTalk identifyOnce;

	// Whether the menu offered each line when the game loop last looked, which
	// is what makes a key the menu's rather than the game's.
	std::atomic<bool> tradeOffered;
	std::atomic<bool> gambleOffered;
	// Set from the input thread, carried out on the game loop.
	std::atomic<bool> pendingTrade;
	std::atomic<bool> pendingGamble;
};
