#pragma once
#include <vector>

// An NPC's talk menu, and picking a line of it for the player.
//
// D2Client 1.13c keeps the menu as a dialog whose lines each carry the function
// that picking them calls (0x48B20 builds it from the table at 0xF3BA0). A line
// is known here by that function, whatever it is called and wherever it comes in
// the menu: "trade" and "trade/repair" are one function under two names, and a
// line the NPC is not offering now is simply not there.
//
// Nothing here touches the game: the NpcTalk module reads the menu, hands it in
// and carries out what it is told to.
namespace TalkMenu {

struct Line {
	unsigned int handler;	// what picking the line calls, 0 for a line of text
	bool selectable;
};

// The first line that can be picked and calls the handler, or -1 for none.
int Find(const std::vector<Line>& lines, unsigned int handler);

// Picks a line for the player, once in each talk with an NPC. The game builds
// the menu again when the server has answered a pick, and a pick the server
// refused leaves its line on the new menu, so picking a line whenever it is
// offered would ask again for as long as the talk lasted.
class OncePerTalk {
public:
	OncePerTalk();

	// What one game loop sees: whether an NPC is being talked to, which one, and
	// whether the line can be picked now. Returns whether to pick it.
	bool Take(bool talking, unsigned int npcId, bool offered);

	// Forgets the talk, for leaving a game.
	void Reset();

private:
	bool talking;
	unsigned int npcId;
	bool taken;
};

}
