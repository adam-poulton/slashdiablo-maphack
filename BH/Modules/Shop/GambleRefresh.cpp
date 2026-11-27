#include "GambleRefresh.h"

namespace GambleRefresh {

Machine::Machine() : state(StateIdle), enteredAt(0), finishedAt(0), finishedOnce(false),
	talkAsked(false), lastItems(0), itemsChangedAt(0), outcome(OutcomeNone) {}

bool Machine::Start(unsigned int now) {
	if (state != StateIdle)
		return false;
	if (finishedOnce && now - finishedAt < MIN_GAP_MS)
		return false;
	outcome = OutcomeNone;
	Enter(StateStarting, now);
	return true;
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

Step Machine::Tick(unsigned int now, const View& view) {
	unsigned int waited = now - enteredAt;

	switch (state) {
	case StateIdle:
		return StepNone;

	// Nothing has been done yet, so a refresh that cannot go ahead leaves the
	// window as it is.
	case StateStarting:
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

	// The talk is asked for a tick after the window comes down, and gamble
	// picked only once the server has answered the talk, so each goes out in
	// the order and with the answers a player's own clicks would have. While
	// the window is down, the timeouts cover the server not answering.
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

	// The tab is put back once everything has arrived: the game moves off a
	// tab with nothing on it yet.
	case StateSettling:
		if (!view.gambleOpen) {
			Finish(OutcomeStopped, now);
			return StepNone;
		}
		if (view.items != lastItems) {
			lastItems = view.items;
			itemsChangedAt = now;
		}
		if (now - itemsChangedAt < SETTLE_MS)
			return StepNone;
		Finish(OutcomeDone, now);
		return StepRestoreTab;
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
};

// The same arrow as the game's own pressed icons are: the up art scaled into
// the pressed frame's face, which is a pixel in from the up frame's on every
// side but the bottom, so the icon shrinks with the button.
static const char* const GLYPH_DOWN[GLYPH_SIZE] = {
	"................................................................",
	"................................................................",
	"................................................................",
	"................................................................",
	"................................................................",
	"................................................................",
	"..........................d2....................................",
	"..........................b9b7..................................",
	"............................b5b315..............................",
	"........................c0bf13b0b113............................",
	"......................b713b8b5b0b0b113..........................",
	"..................16141211b0b0b1b01011b7........................",
	"................c7be13b1b0b0b1b0b0b013c1c1......................",
	"................c3b711b11113b9b0b0b4c1c1bb......................",
	"..............c3bcb3b1b31417c1b0b3c0c1bbb814c1..................",
	"..............1613b111bb17c1..b31517bbb812b213..................",
	"..............beb4b11317c1....b716bf....11b01114................",
	"............c615b3b11417bf..............b4b011bf16..............",
	"............c615b3b1b7c1................b4b111be19..............",
	"..............beb3b1b9c1..............b8b2b011..c3..............",
	"..............bfb5b113................1311b0b2bbc1..............",
	"................13b111bc............b7b5b1b0b6..17..............",
	"................14b3b1b313b8..b8b9b9b511b0b1..17................",
	"..................b911b0b1b3b5b5b5b3b1b0b0b517c1................",
	"....................13b1b0b0b1b0b1b1b0b1131717..................",
	"......................b7b4b01010b0b1b3..c117bf..................",
	"........................15bfb8b8b8bbc3c1c1bb....................",
	"..............................c1bbbfbf14........................",
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
