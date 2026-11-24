#pragma once
#include <string>
#include <vector>

// Rerolling a gamble window by closing it and asking the NPC for it again, and
// the button in the window that asks for it. Nothing here touches the game: the
// Shop module looks at the game, hands what it sees in, and carries out what it
// is told to.
namespace GambleRefresh {

// How long each step may wait on the game before the refresh gives up.
const unsigned int STEP_TIMEOUT_MS = 1500;

// How long the window's items have to stay as they are before they are taken
// to have finished arriving.
const unsigned int SETTLE_MS = 400;

// The least time between one refresh finishing and the next starting, however
// fast the key is pressed.
const unsigned int MIN_GAP_MS = 250;

// What the game is doing, as the Shop module sees it on one tick.
struct View {
	bool gambleOpen;		// the gamble window is up
	bool menuOpen;			// the NPC's talk menu is up
	bool npcHere;			// the NPC can still be talked to
	unsigned int items;		// how many items the window holds
	bool marked;			// one of them is of a marked base
};

// What the game has to be made to do on this tick.
enum Step {
	StepNone,
	StepClose,			// close the gamble window
	StepTalk,			// ask to talk to the NPC
	StepGamble,			// pick gamble in the NPC's menu
	StepRestoreTab,		// put back the tab that was selected
};

// How the last refresh or run ended. Taken once by TakeOutcome.
enum Outcome {
	OutcomeNone,
	OutcomeDone,		// one refresh, finished
	OutcomeFound,		// a run found a marked base
	OutcomeCapped,		// a run reached its most refreshes
	OutcomeStopped,		// a run was stopped, or its window closed
	OutcomeLost,		// the NPC went or the player moved away
	OutcomeTimedOut,	// the game did not answer in time
};

enum State {
	StateIdle,
	StateClosing,
	StateTalking,
	StateOpening,
	StateSettling,
	StatePausing,
};

class Machine {
public:
	Machine();

	// Starts one refresh, or a run that refreshes until a marked base is on
	// offer, at most `cap` times with `pauseMs` between them. Refused while one
	// is going or too soon after the last.
	bool Start(unsigned int now, bool untilMarked, unsigned int cap, unsigned int pauseMs);

	// Ends a run. One in the middle of reopening the window finishes reopening
	// first, so the window is never left closed.
	void Stop();

	// Forgets everything, for leaving a game.
	void Reset();

	Step Tick(unsigned int now, const View& view);
	Outcome TakeOutcome();

	State GetState() const { return state; }
	bool Busy() const { return state != StateIdle; }
	bool Running() const { return Busy() && untilMarked; }
	unsigned int Refreshes() const { return refreshes; }
	unsigned int Cap() const { return cap; }

private:
	State state;
	unsigned int enteredAt;
	unsigned int finishedAt;
	bool finishedOnce;
	bool untilMarked;
	bool stopAsked;
	bool closeAsked;
	unsigned int cap;
	unsigned int pauseMs;
	unsigned int refreshes;
	unsigned int lastItems;
	unsigned int itemsChangedAt;
	Outcome outcome;

	void Enter(State next, unsigned int now);
	void Finish(Outcome result, unsigned int now);
};

// The box a button slot of the trade window answers to, as D2Client 1.13c
// hit-tests it at 0x9C27A: x and y strictly between these bounds.
struct Box {
	int left, right, top, bottom;
	bool Contains(int x, int y) const {
		return x > left && x < right && y > top && y < bottom;
	}
};

// Where D2Client puts the third of the four buttons along the bottom of a trade
// window, the slot a gamble window leaves empty, from the panel's left edge.
const int SLOT_X = 221;

// The slot's box, for a panel offset and screen height.
Box SlotBox(int panelX, int panelY, int screenHeight);

// Where D2Client draws a button's art in that slot: the left edge and the
// bottom row it hands the cell drawing.
int ArtX(int panelX);
int ArtBottom(int panelY, int screenHeight);

// Where the game puts a button's tooltip.
int TooltipX(int panelX);
int TooltipY(int panelY, int screenHeight);

// An NPC's talk menu as D2Client's table holds it.
struct MenuRecord {
	unsigned int npcClass;
	unsigned int entries;
	unsigned short ids[4];
};

// The record and the entry in it that is the NPC's gamble option, or false
// where the table has no record for the class or the record no gamble entry.
// The first record for a class is the one the game uses.
bool FindEntry(const std::vector<MenuRecord>& menus, unsigned int npcClass,
	unsigned short entryId, unsigned int* record, unsigned int* entry);

// The refresh icon drawn over the blank button, as runs of one palette colour
// along a row, relative to the button art's top left corner. There is one for
// each of the blank button's two frames, up and down, since its edges are
// blended into the frame's own pixels.
struct Run {
	int x, y, length;
	unsigned char color;
};
const int GLYPH_SIZE = 32;
const std::vector<Run>& Glyph(bool down);

// Rows of art, two characters per pixel, turned into runs: ".." is left as the
// button has it, and anything else is a palette index in hex. A row of the
// wrong length or a pixel that is neither is an error, and gives nothing.
bool ParseArt(const std::vector<std::string>& rows, int width, std::vector<Run>* runs);

}
