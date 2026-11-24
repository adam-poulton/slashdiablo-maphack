#include "doctest.h"
#include <string>
#include <vector>
#include "Modules/Shop/GambleRefresh.h"

/*
 * Rerolling a gamble window: the steps a refresh walks the game through, when
 * it gives up, where the button answers clicks, and the icon.
 */

using namespace GambleRefresh;

namespace {

View Open(unsigned int items) {
	View view = { true, true, true, items };
	return view;
}

// The window taken down for the refresh, the talk answered or not.
View Down(bool answered = false) {
	View view = { false, true, answered, 0 };
	return view;
}

View Closed() {
	View view = { false, true, false, 0 };
	return view;
}

// Walks one refresh from the window showing its stock to it showing the next,
// starting at `now`, and returns the time the last step was taken at.
unsigned int Cycle(Machine& machine, unsigned int now, Step* last) {
	CHECK(machine.Tick(now, Open(20)) == StepTakeDown);
	now += 30;
	CHECK(machine.Tick(now, Down()) == StepTalk);
	now += 30;
	CHECK(machine.Tick(now, Down()) == StepNone);
	now += 30;
	CHECK(machine.Tick(now, Down(true)) == StepGamble);
	now += 30;
	CHECK(machine.Tick(now, Down(true)) == StepNone);
	now += 30;
	CHECK(machine.Tick(now, Open(0)) == StepNone);
	now += 30;
	CHECK(machine.Tick(now, Open(5)) == StepNone);
	now += 100;
	CHECK(machine.Tick(now, Open(20)) == StepNone);
	now += SETTLE_MS;
	*last = machine.Tick(now, Open(20));
	return now;
}

}

TEST_CASE("a refresh takes the window down, talks again, picks gamble and settles") {
	Machine machine;
	REQUIRE(machine.Start(1000));
	Step last;
	unsigned int end = Cycle(machine, 1000, &last);
	CHECK(last == StepRestoreTab);
	CHECK_FALSE(machine.Busy());
	CHECK(machine.TakeOutcome() == OutcomeDone);
	CHECK(machine.TakeOutcome() == OutcomeNone);
	CHECK(machine.Tick(end + 10, Open(20)) == StepNone);
}

TEST_CASE("the talk goes out on the tick after the window comes down, and only once") {
	Machine machine;
	REQUIRE(machine.Start(0));
	CHECK(machine.Tick(0, Open(20)) == StepTakeDown);
	CHECK(machine.Tick(0, Down()) == StepTalk);
	CHECK(machine.Tick(10, Down()) == StepNone);
	CHECK(machine.Tick(20, Down()) == StepNone);
	CHECK(machine.Tick(30, Down(true)) == StepGamble);
}

TEST_CASE("a refresh that cannot begin leaves the window alone") {
	SUBCASE("the window was closed before the loop got to it") {
		Machine machine;
		REQUIRE(machine.Start(0));
		CHECK(machine.Tick(0, Closed()) == StepNone);
		CHECK(machine.TakeOutcome() == OutcomeStopped);
	}
	SUBCASE("the NPC is out of reach") {
		Machine machine;
		REQUIRE(machine.Start(0));
		View gone = Open(20);
		gone.npcHere = false;
		CHECK(machine.Tick(0, gone) == StepNone);
		CHECK(machine.TakeOutcome() == OutcomeLost);
	}
}

TEST_CASE("a refresh cannot start while one is going or straight after one") {
	Machine machine;
	REQUIRE(machine.Start(1000));
	CHECK_FALSE(machine.Start(1001));
	Step last;
	unsigned int end = Cycle(machine, 1000, &last);
	CHECK_FALSE(machine.Start(end + MIN_GAP_MS - 1));
	CHECK(machine.Start(end + MIN_GAP_MS));
}

TEST_CASE("the window being down is the refresh's own doing, not a stop") {
	Machine machine;
	REQUIRE(machine.Start(0));
	machine.Tick(0, Open(20));
	machine.Tick(10, Down());
	CHECK(machine.Tick(20, Down()) == StepNone);
	CHECK(machine.Busy());
	CHECK(machine.Tick(30, Down(true)) == StepGamble);
	CHECK(machine.Tick(40, Down(true)) == StepNone);
	CHECK(machine.Busy());
}

TEST_CASE("a refresh the server stops answering ends the talk") {
	SUBCASE("no answer to the talk") {
		Machine machine;
		REQUIRE(machine.Start(0));
		machine.Tick(0, Open(20));
		machine.Tick(10, Down());
		CHECK(machine.Tick(10 + STEP_TIMEOUT_MS - 1, Down()) == StepNone);
		CHECK(machine.Busy());
		CHECK(machine.Tick(10 + STEP_TIMEOUT_MS, Down()) == StepAbandon);
		CHECK(machine.TakeOutcome() == OutcomeTimedOut);
	}
	SUBCASE("no window after picking gamble") {
		Machine machine;
		REQUIRE(machine.Start(0));
		machine.Tick(0, Open(20));
		machine.Tick(10, Down());
		CHECK(machine.Tick(20, Down(true)) == StepGamble);
		CHECK(machine.Tick(20 + STEP_TIMEOUT_MS - 1, Down(true)) == StepNone);
		CHECK(machine.Tick(20 + STEP_TIMEOUT_MS, Down(true)) == StepAbandon);
		CHECK(machine.TakeOutcome() == OutcomeTimedOut);
	}
	SUBCASE("a window with no stock in it") {
		Machine machine;
		REQUIRE(machine.Start(0));
		machine.Tick(0, Open(20));
		machine.Tick(10, Down());
		machine.Tick(20, Down(true));
		CHECK(machine.Tick(20 + STEP_TIMEOUT_MS - 1, Open(0)) == StepNone);
		CHECK(machine.Tick(20 + STEP_TIMEOUT_MS, Open(0)) == StepAbandon);
		CHECK(machine.TakeOutcome() == OutcomeTimedOut);
	}
	SUBCASE("the NPC goes while the talk is unanswered") {
		Machine machine;
		REQUIRE(machine.Start(0));
		machine.Tick(0, Open(20));
		machine.Tick(10, Down());
		View gone = Down();
		gone.npcHere = false;
		CHECK(machine.Tick(20, gone) == StepAbandon);
		CHECK(machine.TakeOutcome() == OutcomeLost);
	}
}

TEST_CASE("closing the new window before it settles ends the refresh") {
	Machine machine;
	REQUIRE(machine.Start(0));
	machine.Tick(0, Open(20));
	machine.Tick(10, Down());
	machine.Tick(20, Down(true));
	machine.Tick(30, Open(20));
	CHECK(machine.GetState() == StateSettling);
	CHECK(machine.Tick(40, Closed()) == StepNone);
	CHECK_FALSE(machine.Busy());
	CHECK(machine.TakeOutcome() == OutcomeStopped);
}

TEST_CASE("items still arriving hold the refresh back until they stop") {
	Machine machine;
	REQUIRE(machine.Start(0));
	machine.Tick(0, Open(20));
	machine.Tick(10, Down());
	machine.Tick(20, Down(true));
	machine.Tick(30, Open(4));
	CHECK(machine.Tick(30 + SETTLE_MS - 1, Open(4)) == StepNone);
	machine.Tick(30 + SETTLE_MS - 1, Open(9));
	CHECK(machine.Tick(30 + SETTLE_MS, Open(9)) == StepNone);
	CHECK(machine.Busy());
	CHECK(machine.Tick(30 + 2 * SETTLE_MS - 1, Open(9)) == StepRestoreTab);
	CHECK(machine.TakeOutcome() == OutcomeDone);
}

TEST_CASE("reset forgets a refresh under way") {
	Machine machine;
	REQUIRE(machine.Start(0));
	machine.Tick(0, Open(20));
	machine.Reset();
	CHECK_FALSE(machine.Busy());
	CHECK(machine.TakeOutcome() == OutcomeNone);
	CHECK(machine.Start(1));
}

TEST_CASE("the button answers where the game's buttons do") {
	// 800 x 600, the panel where the game puts it with nothing else open.
	Box box = SlotBox(0, 0, 600);
	CHECK(box.left == 221);
	CHECK(box.right == 266);
	CHECK(box.top == 600 - 0x6D);
	CHECK(box.bottom == 600 - 0x41);

	// The game's bounds are strict on every side.
	CHECK_FALSE(box.Contains(221, 520));
	CHECK(box.Contains(222, 520));
	CHECK(box.Contains(265, 520));
	CHECK_FALSE(box.Contains(266, 520));
	CHECK_FALSE(box.Contains(240, 600 - 0x6D));
	CHECK(box.Contains(240, 600 - 0x6D + 1));
	CHECK(box.Contains(240, 600 - 0x41 - 1));
	CHECK_FALSE(box.Contains(240, 600 - 0x41));

	// Moved with the panel.
	Box moved = SlotBox(80, -20, 768);
	CHECK(moved.left == 301);
	CHECK(moved.bottom == 768 - 20 - 0x41);
}

TEST_CASE("the button is drawn where the game draws its buttons") {
	CHECK(ArtX(0) == 220);
	CHECK(ArtX(80) == 300);
	CHECK(ArtBottom(0, 600) == 600 - 0x3F);
	CHECK(ArtBottom(-20, 768) == 768 - 20 - 0x3F);
	CHECK(TooltipX(80) == 301);
	CHECK(TooltipY(0, 600) == 600 - 0x64);
}

TEST_CASE("art rows become runs of one colour") {
	std::vector<Run> runs;
	REQUIRE(ParseArt({ "..acacad..", "0f0f0f0f0f" }, 5, &runs));
	REQUIRE(runs.size() == 3);
	CHECK(runs[0].x == 1);
	CHECK(runs[0].y == 0);
	CHECK(runs[0].length == 2);
	CHECK(runs[0].color == 0xAC);
	CHECK(runs[1].x == 3);
	CHECK(runs[1].length == 1);
	CHECK(runs[1].color == 0xAD);
	CHECK(runs[2].x == 0);
	CHECK(runs[2].y == 1);
	CHECK(runs[2].length == 5);
	CHECK(runs[2].color == 0x0F);

	CHECK_FALSE(ParseArt({ "acac" }, 3, &runs));
	CHECK(runs.empty());
	CHECK_FALSE(ParseArt({ "ac.x" }, 2, &runs));
	CHECK(runs.empty());
}

TEST_CASE("the refresh icon fits its button and keeps off the bevel") {
	for (int down = 0; down < 2; down++) {
		const std::vector<Run>& glyph = Glyph(down != 0);
		REQUIRE_FALSE(glyph.empty());
		for (unsigned int i = 0; i < glyph.size(); i++) {
			CHECK(glyph[i].length > 0);
			CHECK(glyph[i].x >= 3);
			CHECK(glyph[i].x + glyph[i].length <= GLYPH_SIZE - 3);
			CHECK(glyph[i].y >= 3);
			CHECK(glyph[i].y < GLYPH_SIZE - 3);
		}
	}
}
