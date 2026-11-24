#include "doctest.h"
#include <string>
#include <vector>
#include "Modules/Shop/GambleRefresh.h"

/*
 * Rerolling a gamble window in place: the steps a refresh walks the game
 * through, when it gives up, when a refresh until marked stops, where the
 * button answers clicks, and the icon.
 */

using namespace GambleRefresh;

namespace {

View Open(unsigned int items, bool marked = false, bool answered = false) {
	View view = { true, true, answered, items, marked };
	return view;
}

View Answered() {
	return Open(0, false, true);
}

View Closed() {
	View view = { false, true, false, 0, false };
	return view;
}

// Walks one refresh from the window showing its stock to it showing the next,
// starting at `now`, and returns the time the last step was taken at.
unsigned int Cycle(Machine& machine, unsigned int now, bool marked, Step* last) {
	CHECK(machine.Tick(now, Open(20)) == StepEnd);
	now += 30;
	CHECK(machine.Tick(now, Open(0)) == StepTalk);
	now += 30;
	CHECK(machine.Tick(now, Open(0)) == StepNone);
	now += 30;
	CHECK(machine.Tick(now, Answered()) == StepGamble);
	now += 30;
	CHECK(machine.Tick(now, Open(0, false, true)) == StepNone);
	now += 30;
	CHECK(machine.Tick(now, Open(5, marked, true)) == StepNone);
	now += 100;
	CHECK(machine.Tick(now, Open(20, marked, true)) == StepNone);
	now += SETTLE_MS;
	*last = machine.Tick(now, Open(20, marked, true));
	return now;
}

}

TEST_CASE("one refresh ends the talk, talks again, asks for the stock and settles") {
	Machine machine;
	REQUIRE(machine.Start(1000, false, 1, 0));
	Step last;
	unsigned int end = Cycle(machine, 1000, false, &last);
	CHECK(last == StepRestoreTab);
	CHECK_FALSE(machine.Busy());
	CHECK(machine.Refreshes() == 1);
	CHECK(machine.TakeOutcome() == OutcomeDone);
	CHECK(machine.TakeOutcome() == OutcomeNone);
	CHECK(machine.Tick(end + 10, Open(20)) == StepNone);
}

TEST_CASE("the talk goes out on the tick after the old one ends, and only once") {
	Machine machine;
	REQUIRE(machine.Start(0, false, 1, 0));
	CHECK(machine.Tick(0, Open(20)) == StepEnd);
	CHECK(machine.Tick(0, Open(0)) == StepTalk);
	CHECK(machine.Tick(10, Open(0)) == StepNone);
	CHECK(machine.Tick(20, Open(0)) == StepNone);
	CHECK(machine.Tick(30, Answered()) == StepGamble);
}

TEST_CASE("a refresh that cannot begin leaves the window alone") {
	SUBCASE("the window was closed before the loop got to it") {
		Machine machine;
		REQUIRE(machine.Start(0, false, 1, 0));
		CHECK(machine.Tick(0, Closed()) == StepNone);
		CHECK(machine.TakeOutcome() == OutcomeStopped);
	}
	SUBCASE("the NPC is out of reach") {
		Machine machine;
		REQUIRE(machine.Start(0, false, 1, 0));
		View gone = Open(20);
		gone.npcHere = false;
		CHECK(machine.Tick(0, gone) == StepNone);
		CHECK(machine.TakeOutcome() == OutcomeLost);
	}
}

TEST_CASE("a refresh cannot start while one is going or straight after one") {
	Machine machine;
	REQUIRE(machine.Start(1000, false, 1, 0));
	CHECK_FALSE(machine.Start(1001, false, 1, 0));
	Step last;
	unsigned int end = Cycle(machine, 1000, false, &last);
	CHECK_FALSE(machine.Start(end + MIN_GAP_MS - 1, false, 1, 0));
	CHECK(machine.Start(end + MIN_GAP_MS, false, 1, 0));
}

TEST_CASE("a refresh the server stops answering closes the window") {
	SUBCASE("no answer to the talk") {
		Machine machine;
		REQUIRE(machine.Start(0, false, 1, 0));
		machine.Tick(0, Open(20));
		machine.Tick(10, Open(0));
		CHECK(machine.Tick(10 + STEP_TIMEOUT_MS - 1, Open(0)) == StepNone);
		CHECK(machine.Busy());
		CHECK(machine.Tick(10 + STEP_TIMEOUT_MS, Open(0)) == StepAbandon);
		CHECK(machine.TakeOutcome() == OutcomeTimedOut);
	}
	SUBCASE("no stock after asking for it") {
		Machine machine;
		REQUIRE(machine.Start(0, false, 1, 0));
		machine.Tick(0, Open(20));
		machine.Tick(10, Open(0));
		CHECK(machine.Tick(20, Answered()) == StepGamble);
		CHECK(machine.Tick(20 + STEP_TIMEOUT_MS - 1, Answered()) == StepNone);
		CHECK(machine.Tick(20 + STEP_TIMEOUT_MS, Answered()) == StepAbandon);
		CHECK(machine.TakeOutcome() == OutcomeTimedOut);
	}
	SUBCASE("the NPC goes while the talk is unanswered") {
		Machine machine;
		REQUIRE(machine.Start(0, false, 1, 0));
		machine.Tick(0, Open(20));
		machine.Tick(10, Open(0));
		View gone = Open(0);
		gone.npcHere = false;
		CHECK(machine.Tick(20, gone) == StepAbandon);
		CHECK(machine.TakeOutcome() == OutcomeLost);
	}
}

TEST_CASE("closing the window mid refresh ends it without touching the window") {
	Machine machine;
	REQUIRE(machine.Start(0, false, 1, 0));
	machine.Tick(0, Open(20));
	machine.Tick(10, Open(0));
	CHECK(machine.Tick(20, Closed()) == StepNone);
	CHECK_FALSE(machine.Busy());
	CHECK(machine.TakeOutcome() == OutcomeStopped);
}

TEST_CASE("items still arriving hold the refresh back until they stop") {
	Machine machine;
	REQUIRE(machine.Start(0, false, 1, 0));
	machine.Tick(0, Open(20));
	machine.Tick(10, Open(0));
	machine.Tick(20, Answered());
	machine.Tick(30, Open(4));
	CHECK(machine.Tick(30 + SETTLE_MS - 1, Open(4)) == StepNone);
	machine.Tick(30 + SETTLE_MS - 1, Open(9));
	CHECK(machine.Tick(30 + SETTLE_MS, Open(9)) == StepNone);
	CHECK(machine.Busy());
	CHECK(machine.Tick(30 + 2 * SETTLE_MS - 1, Open(9)) == StepRestoreTab);
	CHECK(machine.TakeOutcome() == OutcomeDone);
}

TEST_CASE("refreshing until marked") {
	Machine machine;
	Step last;

	SUBCASE("stops when a marked base is on offer") {
		REQUIRE(machine.Start(0, true, 10, 500));
		CHECK(machine.Running());
		unsigned int now = Cycle(machine, 0, false, &last);
		CHECK(last == StepRestoreTab);
		CHECK(machine.GetState() == StatePausing);
		CHECK(machine.Tick(now + 499, Open(20)) == StepNone);
		now = Cycle(machine, now + 500, true, &last);
		CHECK_FALSE(machine.Busy());
		CHECK(machine.Refreshes() == 2);
		CHECK(machine.TakeOutcome() == OutcomeFound);
	}
	SUBCASE("stops at its most refreshes") {
		REQUIRE(machine.Start(0, true, 3, 0));
		unsigned int now = 0;
		for (int i = 0; i < 3; i++)
			now = Cycle(machine, now, false, &last) + 1;
		CHECK_FALSE(machine.Busy());
		CHECK(machine.Refreshes() == 3);
		CHECK(machine.TakeOutcome() == OutcomeCapped);
	}
	SUBCASE("a stop during the pause ends it at once") {
		REQUIRE(machine.Start(0, true, 10, 1000));
		unsigned int now = Cycle(machine, 0, false, &last);
		machine.Stop();
		CHECK(machine.Tick(now + 10, Open(20)) == StepNone);
		CHECK(machine.TakeOutcome() == OutcomeStopped);
		CHECK(machine.Refreshes() == 1);
	}
	SUBCASE("a stop mid refresh lets the stock arrive first") {
		REQUIRE(machine.Start(0, true, 10, 0));
		CHECK(machine.Tick(0, Open(20)) == StepEnd);
		machine.Stop();
		CHECK(machine.Tick(10, Open(0)) == StepTalk);
		CHECK(machine.Tick(20, Answered()) == StepGamble);
		CHECK(machine.Tick(30, Open(20)) == StepNone);
		CHECK(machine.Tick(30 + SETTLE_MS, Open(20)) == StepRestoreTab);
		CHECK(machine.TakeOutcome() == OutcomeStopped);
	}
	SUBCASE("closing the window during the pause ends it") {
		REQUIRE(machine.Start(0, true, 10, 1000));
		unsigned int now = Cycle(machine, 0, false, &last);
		CHECK(machine.Tick(now + 10, Closed()) == StepNone);
		CHECK(machine.TakeOutcome() == OutcomeStopped);
	}
}

TEST_CASE("a single refresh finds nothing to stop") {
	Machine machine;
	REQUIRE(machine.Start(0, false, 1, 0));
	CHECK_FALSE(machine.Running());
	machine.Tick(0, Open(20));
	machine.Stop();
	machine.Tick(10, Open(0));
	machine.Tick(20, Answered());
	machine.Tick(30, Open(20, true));
	CHECK(machine.Tick(30 + SETTLE_MS, Open(20, true)) == StepRestoreTab);
	CHECK(machine.TakeOutcome() == OutcomeDone);
}

TEST_CASE("reset forgets a refresh under way") {
	Machine machine;
	REQUIRE(machine.Start(0, true, 10, 0));
	machine.Tick(0, Open(20));
	machine.Reset();
	CHECK_FALSE(machine.Busy());
	CHECK(machine.TakeOutcome() == OutcomeNone);
	CHECK(machine.Start(1, false, 1, 0));
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
