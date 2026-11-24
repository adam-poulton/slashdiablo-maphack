#include "GambleRefresh.h"

namespace GambleRefresh {

Machine::Machine() : state(StateIdle), enteredAt(0), finishedAt(0), finishedOnce(false),
	untilMarked(false), stopAsked(false), closeAsked(false), cap(1), pauseMs(0), refreshes(0), lastItems(0),
	itemsChangedAt(0), outcome(OutcomeNone) {}

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
	Enter(StateClosing, now);
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
	closeAsked = false;
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

Step Machine::Tick(unsigned int now, const View& view) {
	unsigned int waited = now - enteredAt;
	switch (state) {
	case StateIdle:
		return StepNone;

	case StateClosing:
		if (!closeAsked && view.gambleOpen) {
			closeAsked = true;
			return StepClose;
		}
		if (!view.gambleOpen && !view.menuOpen) {
			if (!view.npcHere) {
				Finish(OutcomeLost, now);
				return StepNone;
			}
			Enter(StateTalking, now);
			return StepTalk;
		}
		if (waited >= STEP_TIMEOUT_MS)
			Finish(OutcomeTimedOut, now);
		return StepNone;

	case StateTalking:
		if (view.menuOpen) {
			Enter(StateOpening, now);
			return StepGamble;
		}
		if (!view.npcHere)
			Finish(OutcomeLost, now);
		else if (waited >= STEP_TIMEOUT_MS)
			Finish(OutcomeTimedOut, now);
		return StepNone;

	case StateOpening:
		if (view.gambleOpen) {
			Enter(StateSettling, now);
			lastItems = view.items;
			itemsChangedAt = now;
			return StepRestoreTab;
		}
		if (waited >= STEP_TIMEOUT_MS)
			Finish(OutcomeTimedOut, now);
		return StepNone;

	case StateSettling:
		if (!view.gambleOpen) {
			Finish(OutcomeStopped, now);
			return StepNone;
		}
		if (view.items != lastItems) {
			lastItems = view.items;
			itemsChangedAt = now;
		}
		if (view.items == 0) {
			if (waited >= STEP_TIMEOUT_MS)
				Finish(OutcomeTimedOut, now);
			return StepNone;
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
		// The tab is put back again once everything has arrived: on the first
		// try the tab may have had nothing in it yet, and the game moves off a
		// tab with nothing in it.
		return StepRestoreTab;

	case StatePausing:
		if (!view.gambleOpen || stopAsked) {
			Finish(OutcomeStopped, now);
			return StepNone;
		}
		if (waited < pauseMs)
			return StepNone;
		Enter(StateClosing, now);
		closeAsked = true;
		return StepClose;
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

bool FindEntry(const std::vector<MenuRecord>& menus, unsigned int npcClass,
		unsigned short entryId, unsigned int* record, unsigned int* entry) {
	for (unsigned int i = 0; i < menus.size(); i++) {
		if (menus[i].npcClass != npcClass)
			continue;
		unsigned int entries = menus[i].entries < 4 ? menus[i].entries : 4;
		for (unsigned int e = 0; e < entries; e++) {
			if (menus[i].ids[e] == entryId) {
				*record = i;
				*entry = e;
				return true;
			}
		}
		return false;
	}
	return false;
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
	"..........................c7....................................",
	"..........................c7c6..................................",
	"..........................b5afb114..............................",
	"..........................13b0adaf13............................",
	"....................bbb9b7b5b1adadad13..........................",
	"................c71512b3b1aeadadadadad11bf......................",
	"..............c1b813b0adadadadadadadadad14......................",
	"............c715b7afadadadadadadadadadb8c917....................",
	"............1515afadadadadaeb0adadadb5c917......................",
	"..........c31311adadad0fb7c917adad11c917..bbb8be................",
	"..........1413adadad0fbfc91914afafc917bb1212b4afb6..............",
	"..........13b3adadad14c917....b1c3c9bf..1210adadaf17............",
	"........ceb7aeadadaec9c9........17c1....b6b0adadadbf............",
	"........ceb9adadad11c9c1........16......1411adadad14c1..........",
	"........c9b7adadadb317..................b6b3adadadbf17..........",
	"..........13adadadb5c9..................1211adadad14c1..........",
	"..........bcadadadafca................bc12b1adadad..c1..........",
	"..........bfb0adadadbf................b5b4adadadaec9c1..........",
	"............b2adadadafbf..........bcb5b210adadadb5c9c1..........",
	"............b8afadadadaeb6b714b7b8121210adadadad17c9............",
	"..............b90fadadadadb01111b2b1adadadadad14c9c3............",
	"................b7adadadadadadadadadadadadadb4c9c9..............",
	"..................14aeadadadadadadadadadadb7c9c9................",
	"......................b5afadadadadadaeb3c1c917..................",
	"......................16c7c3bb14..bf171717c1....................",
	"............................c1c7c8c917c0........................",
	"................................................................",
	"................................................................",
	"................................................................",
};

static const char* const GLYPH_DOWN[GLYPH_SIZE] = {
	"................................................................",
	"................................................................",
	"................................................................",
	"..........................be....................................",
	"..........................b9b9c8................................",
	"..........................c6b113d1..............................",
	"..........................13b0adaf13............................",
	"....................b813b4b2b1adadad11..........................",
	"................16b812b3b1aeadadadadadb1bc......................",
	"..............c113b6b0adadadadadadadadad14......................",
	"............c1b8b6aeadadadadadadadadadb8c917....................",
	"............1413afadadadadaeb0adadad12c917......................",
	"..........19b6b1adadadadb7c917adadb1c917..14b614................",
	"..........beb5adadad0fbfc917b8aeaf1717141212b4afb3..............",
	"..........b911adadad14c9c1....b11619bf..1210adadafc1............",
	"..........b9aeadadaec9c9........17c1....b6b0adadadbf............",
	"........b9b9adadad11c9c1................b811adadad..bb..........",
	"........13b7adadadb3c9..................b6b3adadad14bf..........",
	"..........13adadadb4c9..................12b1adadad..c1..........",
	"..........b9adadadafc9................b8b2b0adadadbfbf..........",
	"..........c3b0adadadbb................12b2adadadaec9bb..........",
	"............b4adadadafbb..........b812b2afadadadb4c914..........",
	"............c1afadadadae12b6b8b61312b4afadadadad1717............",
	"..............b7adadadadad10b12811b1adadadadad14c9c1............",
	"................b8adadadadadadadadadadadadadb3c917..............",
	"..................b9aeadadadadadadadadadadb8c917................",
	"......................b5afadadadadad0fb2c11717..................",
	"......................bbc316..1414..171717c1....................",
	"............................c7c7c8c8c816........................",
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
