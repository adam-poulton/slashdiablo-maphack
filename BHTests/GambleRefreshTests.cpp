#include "doctest.h"
#include <string>
#include <vector>
#include "Modules/Shop/GambleRefresh.h"

/*
 * Rerolling a gamble window: the steps a refresh walks the game through, when it
 * gives up, when a refresh until marked stops, where the button answers clicks,
 * how the NPC's gamble option is found in the game's menu table, and the icon.
 */

using namespace GambleRefresh;

namespace {

View Shown(unsigned int items, bool marked = false) {
	View view = { true, false, true, items, marked };
	return view;
}

View Closed() {
	View view = { false, false, true, 0, false };
	return view;
}

View Menu() {
	View view = { false, true, true, 0, false };
	return view;
}

// Walks one refresh from the window being open to it being open again with its
// items settled, starting at `now`, and returns the time it ends at.
unsigned int Cycle(Machine& machine, unsigned int now, bool marked, Step* last) {
	CHECK(machine.Tick(now, Shown(20)) == StepClose);
	now += 30;
	CHECK(machine.Tick(now, Closed()) == StepTalk);
	now += 30;
	CHECK(machine.Tick(now, Closed()) == StepNone);
	now += 30;
	CHECK(machine.Tick(now, Menu()) == StepGamble);
	now += 30;
	CHECK(machine.Tick(now, Shown(5, marked)) == StepRestoreTab);
	now += 100;
	CHECK(machine.Tick(now, Shown(20, marked)) == StepNone);
	now += SETTLE_MS;
	*last = machine.Tick(now, Shown(20, marked));
	return now;
}

}

TEST_CASE("one refresh closes the window, talks to the NPC, picks gamble and settles") {
	Machine machine;
	REQUIRE(machine.Start(1000, false, 1, 0));
	Step last;
	unsigned int end = Cycle(machine, 1000, false, &last);
	CHECK(last == StepRestoreTab);
	CHECK_FALSE(machine.Busy());
	CHECK(machine.Refreshes() == 1);
	CHECK(machine.TakeOutcome() == OutcomeDone);
	CHECK(machine.TakeOutcome() == OutcomeNone);
	CHECK(machine.Tick(end + 10, Shown(20)) == StepNone);
}

TEST_CASE("the close is asked for once, however long the window takes to go") {
	Machine machine;
	REQUIRE(machine.Start(0, false, 1, 0));
	CHECK(machine.Tick(0, Shown(20)) == StepClose);
	CHECK(machine.Tick(10, Shown(20)) == StepNone);
	CHECK(machine.Tick(20, Shown(20)) == StepNone);
	CHECK(machine.Tick(30, Closed()) == StepTalk);
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

TEST_CASE("each step gives up when the game does not answer") {
	SUBCASE("the window never closes") {
		Machine machine;
		REQUIRE(machine.Start(0, false, 1, 0));
		machine.Tick(0, Shown(20));
		CHECK(machine.Tick(STEP_TIMEOUT_MS - 1, Shown(20)) == StepNone);
		CHECK(machine.Busy());
		machine.Tick(STEP_TIMEOUT_MS, Shown(20));
		CHECK(machine.TakeOutcome() == OutcomeTimedOut);
	}
	SUBCASE("the NPC never answers the talk") {
		Machine machine;
		REQUIRE(machine.Start(0, false, 1, 0));
		machine.Tick(0, Shown(20));
		machine.Tick(10, Closed());
		machine.Tick(10 + STEP_TIMEOUT_MS, Closed());
		CHECK(machine.TakeOutcome() == OutcomeTimedOut);
	}
	SUBCASE("the gamble window never opens") {
		Machine machine;
		REQUIRE(machine.Start(0, false, 1, 0));
		machine.Tick(0, Shown(20));
		machine.Tick(10, Closed());
		machine.Tick(20, Menu());
		machine.Tick(20 + STEP_TIMEOUT_MS, Menu());
		CHECK(machine.TakeOutcome() == OutcomeTimedOut);
	}
	SUBCASE("no items ever arrive") {
		Machine machine;
		REQUIRE(machine.Start(0, false, 1, 0));
		machine.Tick(0, Shown(20));
		machine.Tick(10, Closed());
		machine.Tick(20, Menu());
		machine.Tick(30, Shown(0));
		CHECK(machine.Tick(30 + STEP_TIMEOUT_MS - 1, Shown(0)) == StepNone);
		CHECK(machine.Busy());
		machine.Tick(30 + STEP_TIMEOUT_MS, Shown(0));
		CHECK(machine.TakeOutcome() == OutcomeTimedOut);
	}
}

TEST_CASE("a refresh gives up when the NPC can no longer be talked to") {
	View gone = Closed();
	gone.npcHere = false;

	SUBCASE("before talking") {
		Machine machine;
		REQUIRE(machine.Start(0, false, 1, 0));
		machine.Tick(0, Shown(20));
		CHECK(machine.Tick(10, gone) == StepNone);
		CHECK(machine.TakeOutcome() == OutcomeLost);
	}
	SUBCASE("while waiting on the menu") {
		Machine machine;
		REQUIRE(machine.Start(0, false, 1, 0));
		machine.Tick(0, Shown(20));
		machine.Tick(10, Closed());
		CHECK(machine.Tick(20, gone) == StepNone);
		CHECK(machine.TakeOutcome() == OutcomeLost);
	}
}

TEST_CASE("items still arriving hold the refresh back until they stop") {
	Machine machine;
	REQUIRE(machine.Start(0, false, 1, 0));
	machine.Tick(0, Shown(20));
	machine.Tick(10, Closed());
	machine.Tick(20, Menu());
	machine.Tick(30, Shown(4));
	CHECK(machine.Tick(30 + SETTLE_MS - 1, Shown(4)) == StepNone);
	machine.Tick(30 + SETTLE_MS - 1, Shown(9));
	CHECK(machine.Tick(30 + SETTLE_MS, Shown(9)) == StepNone);
	CHECK(machine.Busy());
	CHECK(machine.Tick(30 + 2 * SETTLE_MS - 1, Shown(9)) == StepRestoreTab);
	CHECK(machine.TakeOutcome() == OutcomeDone);
}

TEST_CASE("refreshing until marked") {
	Machine machine;
	Step last;

	SUBCASE("stops when a marked base is on offer") {
		REQUIRE(machine.Start(0, true, 10, 500));
		CHECK(machine.Running());
		unsigned int now = Cycle(machine, 0, false, &last);
		CHECK(machine.GetState() == StatePausing);
		CHECK(machine.Tick(now + 499, Shown(20)) == StepNone);
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
		CHECK(machine.Tick(now + 10, Shown(20)) == StepNone);
		CHECK(machine.TakeOutcome() == OutcomeStopped);
		CHECK(machine.Refreshes() == 1);
	}
	SUBCASE("a stop with the window closed reopens it first") {
		REQUIRE(machine.Start(0, true, 10, 0));
		CHECK(machine.Tick(0, Shown(20)) == StepClose);
		machine.Stop();
		CHECK(machine.Tick(10, Closed()) == StepTalk);
		CHECK(machine.Tick(20, Menu()) == StepGamble);
		CHECK(machine.Tick(30, Shown(20)) == StepRestoreTab);
		CHECK(machine.Tick(30 + SETTLE_MS, Shown(20)) == StepRestoreTab);
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
	machine.Tick(0, Shown(20));
	machine.Stop();
	Step last;
	machine.Tick(10, Closed());
	machine.Tick(20, Menu());
	machine.Tick(30, Shown(20, true));
	last = machine.Tick(30 + SETTLE_MS, Shown(20, true));
	CHECK(last == StepRestoreTab);
	CHECK(machine.TakeOutcome() == OutcomeDone);
}

TEST_CASE("reset forgets a refresh under way") {
	Machine machine;
	REQUIRE(machine.Start(0, true, 10, 0));
	machine.Tick(0, Shown(20));
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

TEST_CASE("the gamble option is found by its id in the NPC's own record") {
	const unsigned short TALK = 0x0D35, TRADE = 0x0D44, GAMBLE = 0x0D46, HIRE = 0x0D47;
	// A few records as D2Client 1.13c's table at 0xF3BA0 has them.
	std::vector<MenuRecord> menus = {
		{ 148, 3, { TALK, TRADE, 0, 0 } },			// Akara
		{ 147, 4, { TALK, TRADE, GAMBLE, 0 } },		// Gheed
		{ 405, 3, { TRADE, GAMBLE, 0, 0 } },		// Jamella
		{ 252, 4, { TALK, HIRE, TRADE, 0 } },		// Asheara
		{ 512, 4, { TALK, TRADE, GAMBLE, 0 } },		// Anya
		{ 514, 3, { TALK, GAMBLE, 0, 0 } },			// Nihlathak
		{ 257, 2, { TRADE, 0, 0, 0 } },				// Halbu, the game's first record
		{ 257, 2, { TALK, GAMBLE, 0, 0 } },			// a second record for one class
		{ 600, 9, { TALK, TRADE, TALK, TRADE } },	// an entry count past the four
	};
	unsigned int record = 99, entry = 99;

	CHECK(FindEntry(menus, 147, GAMBLE, &record, &entry));
	CHECK(record == 1);
	CHECK(entry == 2);
	CHECK(FindEntry(menus, 405, GAMBLE, &record, &entry));
	CHECK(record == 2);
	CHECK(entry == 1);
	CHECK(FindEntry(menus, 512, GAMBLE, &record, &entry));
	CHECK(record == 4);
	CHECK(entry == 2);
	CHECK(FindEntry(menus, 514, GAMBLE, &record, &entry));
	CHECK(record == 5);
	CHECK(entry == 1);

	CHECK_FALSE(FindEntry(menus, 148, GAMBLE, &record, &entry));
	CHECK_FALSE(FindEntry(menus, 252, GAMBLE, &record, &entry));
	CHECK_FALSE(FindEntry(menus, 999, GAMBLE, &record, &entry));
	CHECK_FALSE(FindEntry(menus, 257, GAMBLE, &record, &entry));
	CHECK_FALSE(FindEntry(menus, 600, GAMBLE, &record, &entry));
	CHECK_FALSE(FindEntry(std::vector<MenuRecord>(), 147, GAMBLE, &record, &entry));
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
