#include "GambleRefresh.h"

namespace GambleRefresh {

Machine::Machine() : state(StateIdle), enteredAt(0), finishedAt(0), finishedOnce(false),
	untilMarked(false), stopAsked(false), talkAsked(false), cap(1), pauseMs(0), refreshes(0),
	lastItems(0), itemsChangedAt(0), outcome(OutcomeNone) {}

bool Machine::Start(unsigned int now, bool runUntilMarked, unsigned int most,
		unsigned int pause) {
	if (state != StateIdle)
		return false;
	if (finishedOnce && now - finishedAt < MIN_GAP_MS)
		return false;
	untilMarked = runUntilMarked;
	cap = most > 0 ? most : 1;
	pauseMs = pause;
	refreshes = 0;
	stopAsked = false;
	outcome = OutcomeNone;
	Enter(StateEnding, now);
	return true;
}

void Machine::Stop() {
	if (state != StateIdle)
		stopAsked = true;
}

void Machine::Reset() {
	*this = Machine();
}

void Machine::Enter(State next, unsigned int now) {
	state = next;
	enteredAt = now;
	talkAsked = false;
}

void Machine::Finish(Outcome result, unsigned int now) {
	outcome = result;
	state = StateIdle;
	finishedAt = now;
	finishedOnce = true;
}

Outcome Machine::TakeOutcome() {
	Outcome taken = outcome;
	outcome = OutcomeNone;
	return taken;
}

// Nothing has been done yet, so a refresh that cannot go ahead leaves the
// window as it is.
Step Machine::BeginCycle(unsigned int now, const View& view) {
	if (!view.gambleOpen) {
		Finish(OutcomeStopped, now);
		return StepNone;
	}
	if (!view.npcHere) {
		Finish(OutcomeLost, now);
		return StepNone;
	}
	Enter(StateTalking, now);
	return StepTakeDown;
}

Step Machine::Tick(unsigned int now, const View& view) {
	unsigned int waited = now - enteredAt;

	// The window is only the player's to close while it is up. While it is
	// down for the refresh, the timeouts cover the server not answering.
	if ((state == StateSettling || state == StatePausing) && !view.gambleOpen) {
		Finish(OutcomeStopped, now);
		return StepNone;
	}

	switch (state) {
	case StateIdle:
		return StepNone;

	case StateEnding:
		return BeginCycle(now, view);

	// The talk is asked for a tick after the window comes down, and gamble
	// picked only once the server has answered the talk, so each goes out in
	// the order and with the answers a player's own clicks would have.
	case StateTalking:
		if (!talkAsked) {
			talkAsked = true;
			enteredAt = now;
			return StepTalk;
		}
		if (view.talkAnswered) {
			Enter(StateStocking, now);
			return StepGamble;
		}
		if (!view.npcHere) {
			Finish(OutcomeLost, now);
			return StepAbandon;
		}
		if (waited >= STEP_TIMEOUT_MS) {
			Finish(OutcomeTimedOut, now);
			return StepAbandon;
		}
		return StepNone;

	case StateStocking:
		if (view.gambleOpen && view.items > 0) {
			Enter(StateSettling, now);
			lastItems = view.items;
			itemsChangedAt = now;
			return StepNone;
		}
		if (waited >= STEP_TIMEOUT_MS) {
			Finish(OutcomeTimedOut, now);
			return StepAbandon;
		}
		return StepNone;

	case StateSettling:
		if (view.items != lastItems) {
			lastItems = view.items;
			itemsChangedAt = now;
		}
		if (now - itemsChangedAt < SETTLE_MS)
			return StepNone;
		refreshes++;
		if (!untilMarked)
			Finish(OutcomeDone, now);
		else if (view.marked)
			Finish(OutcomeFound, now);
		else if (stopAsked)
			Finish(OutcomeStopped, now);
		else if (refreshes >= cap)
			Finish(OutcomeCapped, now);
		else
			Enter(StatePausing, now);
		return StepRestoreTab;

	case StatePausing:
		if (stopAsked) {
			Finish(OutcomeStopped, now);
			return StepNone;
		}
		if (waited < pauseMs)
			return StepNone;
		return BeginCycle(now, view);
	}
	return StepNone;
}

Box SlotBox(int panelX, int panelY, int screenHeight) {
	int bottom = screenHeight + panelY;
	Box box;
	box.left = panelX + SLOT_X;
	box.right = box.left + 0x2D;
	box.top = bottom - 0x6D;
	box.bottom = bottom - 0x41;
	return box;
}

int ArtX(int panelX) {
	return panelX + SLOT_X - 1;
}

int ArtBottom(int panelY, int screenHeight) {
	return screenHeight + panelY - 0x3F;
}

int TooltipX(int panelX) {
	return panelX + SLOT_X;
}

int TooltipY(int panelY, int screenHeight) {
	return screenHeight + panelY - 0x64;
}

static int HexDigit(char c) {
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

bool ParseArt(const std::vector<std::string>& rows, int width, std::vector<Run>* runs) {
	runs->clear();
	for (unsigned int y = 0; y < rows.size(); y++) {
		const std::string& row = rows[y];
		if ((int)row.size() != width * 2) {
			runs->clear();
			return false;
		}
		Run run = { 0, (int)y, 0, 0 };
		for (int x = 0; x <= width; x++) {
			int color = -1;
			if (x < width && row.compare(x * 2, 2, "..") != 0) {
				int high = HexDigit(row[x * 2]);
				int low = HexDigit(row[x * 2 + 1]);
				if (high < 0 || low < 0) {
					runs->clear();
					return false;
				}
				color = high * 16 + low;
			}
			if (run.length > 0 && color == run.color)
				run.length++;
			else {
				if (run.length > 0)
					runs->push_back(run);
				run.length = 0;
				if (color >= 0) {
					run.x = x;
					run.length = 1;
					run.color = (unsigned char)color;
				}
			}
		}
	}
	return true;
}

// A clockwise arrow cut into the blank button the way the game's own icons are:
// a groove in near black, the button's face in shadow along its upper left edge
// and lit along its lower right, as if the light came from the top left, with
// every edge blended into the frame's own pixels and kept to the colours the
// game's button art uses. Made by drawing the arrow at six times the size over
// each frame, averaging down and picking the nearest of those colours.
static const char* const GLYPH_UP[GLYPH_SIZE] = {
	"................................................................",
	"................................................................",
	"................................................................",
	"................................................................",
	"..........................1d....................................",
	"..........................b9b7..................................",
	"..........................14b4b4be..............................",
	"..........................beb5b0b1b9............................",
	"......................b9b81413b0b0b0b7..........................",
	"..................16b81211b0b0b1b010b0b4........................",
	"................c6beb6b0b0b0b0b0b010b1bf17......................",
	"..............ca16b7b1b0b0b1b4b0b010..17c1......................",
	"..............c315b1b1b1b7171710b0b917c1b814....................",
	"............c7beb3b1b1bb1717..11b5c9c1b8b6b611c1................",
	"............c715b1b1b5c917....13c317....b3b0b0b5................",
	"............c313b1b1c1c9........c1......b6b0b11217..............",
	"............bfb4b1b1..17................b7b1b0b4ca..............",
	"............c6b5b1b0bfc1................b6b1b0b31b..............",
	"............c8131111c117..............b812b0b012c9..............",
	"..............14b1b11317..............13b4b0b0b617..............",
	"..............c111b1b1bf..........bfb713b0b0b0..c8..............",
	"................b8b1b1b0b4b814b8b713b5b0b0b013c9................",
	"..................b9b0b0b0b1b3b412b1b0b0b0111717................",
	"....................b510b0b0b010b0b1b0b0b417c9..................",
	"......................bcb3101010b0b1b3bb1717c0..................",
	"........................bf17..bb..c11917c1......................",
	"..............................c1bfbbbf..........................",
	"................................................................",
	"................................................................",
	"................................................................",
	"................................................................",
	"................................................................",
};

static const char* const GLYPH_DOWN[GLYPH_SIZE] = {
	"................................................................",
	"................................................................",
	"................................................................",
	"................................................................",
	"..........................c7....................................",
	"..........................cdc6..................................",
	"..........................14b3b315..............................",
	"..........................b8b4b0b1b6............................",
	"......................b9b9b8b5b0b0b013..........................",
	"..................bfb81211b0b0b01010b0b4........................",
	"................bf14b610101010b01010b0bfc1......................",
	"..............c3be13b1b0b0b112101010bb17bf......................",
	"..............beb9b1b1b0b717c11010b817c1b8b7....................",
	"............c115b3b1b1bb1717..b11217c1b6b6b611bf................",
	"............c015b1b1b5c9c1....b51617....b3b0b0b5c0..............",
	"............c013b1b0c1c9........16bb....b6b0b0b3c1..............",
	"............c0b4b0b0..17................b6b1b0b317..............",
	"............16b5b1b0c1bf................b6b1b0b3c9..............",
	"............c313b1b1c1c1..............b8121010b317..............",
	"..............14b1b1b517..............b6b3b010b617..............",
	"..............14b1b0b114..........14b61210b0b01717..............",
	"................13b0b1b0b4b8b8b8b6b612b01010b517c1..............",
	"..................b8b01010b011b3b3b1b01010111717................",
	"....................b510101010afb0b0b010b31717..................",
	"........................b310101010b0b3..1717bf..................",
	"........................bec1bf..bbc11717c1......................",
	"..............................bfbbbb14..........................",
	"................................................................",
	"................................................................",
	"................................................................",
	"................................................................",
	"................................................................",
};

const std::vector<Run>& Glyph(bool down) {
	static std::vector<Run> runs[2];
	static bool parsed = false;
	if (!parsed) {
		ParseArt(std::vector<std::string>(GLYPH_UP, GLYPH_UP + GLYPH_SIZE), GLYPH_SIZE,
			&runs[0]);
		ParseArt(std::vector<std::string>(GLYPH_DOWN, GLYPH_DOWN + GLYPH_SIZE), GLYPH_SIZE,
			&runs[1]);
		parsed = true;
	}
	return runs[down ? 1 : 0];
}

}
