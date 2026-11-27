#include "doctest.h"
#include "Modules/NpcTalk/TalkMenu.h"

/*
 * Picking a line of an NPC's talk menu.
 *
 * A line is found by the function picking it calls, so it is found wherever it
 * comes in the menu and only while the NPC offers it. A line picked for the
 * player is picked once in a talk: the game builds the menu again after the
 * server answers, and a refused pick leaves its line there to be picked again.
 */

using TalkMenu::Find;
using TalkMenu::Line;
using TalkMenu::OncePerTalk;

static const unsigned int TALK = 0x6FAFA2F0;
static const unsigned int TRADE = 0x6FAFAEE0;
static const unsigned int GAMBLE = 0x6FAFABE0;
static const unsigned int IDENTIFY = 0x6FAF6AF0;
static const unsigned int LEAVE = 0x6FAF8380;

static std::vector<Line> Menu(std::initializer_list<unsigned int> handlers) {
	std::vector<Line> lines;
	// The NPC's name heads every menu, and cannot be picked.
	lines.push_back({ 0, false });
	for (unsigned int handler : handlers)
		lines.push_back({ handler, true });
	return lines;
}

TEST_CASE("a line is found by its handler, wherever it comes in the menu") {
	CHECK(Find(Menu({ TALK, TRADE, GAMBLE, LEAVE }), TRADE) == 2);
	CHECK(Find(Menu({ TALK, TRADE, GAMBLE, LEAVE }), GAMBLE) == 3);
	CHECK(Find(Menu({ TRADE, GAMBLE, LEAVE }), GAMBLE) == 2);
}

TEST_CASE("a line the NPC does not offer is not found") {
	CHECK(Find(Menu({ TALK, TRADE, LEAVE }), GAMBLE) == -1);
	CHECK(Find(Menu({ TALK, LEAVE }), IDENTIFY) == -1);
	CHECK(Find(std::vector<Line>(), TRADE) == -1);
}

TEST_CASE("a line that cannot be picked is not found") {
	std::vector<Line> lines = Menu({ TALK, IDENTIFY, LEAVE });
	lines[2].selectable = false;
	CHECK(Find(lines, IDENTIFY) == -1);
}

TEST_CASE("no handler finds nothing, though lines of text have none") {
	CHECK(Find(Menu({ TALK, LEAVE }), 0) == -1);
}

TEST_CASE("an offered line is picked once in a talk") {
	OncePerTalk once;
	CHECK(once.Take(true, 7, true));
	CHECK_FALSE(once.Take(true, 7, true));
	CHECK_FALSE(once.Take(true, 7, true));
}

TEST_CASE("a line still offered after the menu is built again is not picked again") {
	OncePerTalk once;
	CHECK(once.Take(true, 7, true));
	// The menu is down while the server answers, then up again with the line.
	CHECK_FALSE(once.Take(true, 7, false));
	CHECK_FALSE(once.Take(true, 7, true));
}

TEST_CASE("a line is picked when it comes to be offered later in the talk") {
	OncePerTalk once;
	CHECK_FALSE(once.Take(true, 7, false));
	CHECK_FALSE(once.Take(true, 7, false));
	CHECK(once.Take(true, 7, true));
}

TEST_CASE("the next talk with the same NPC picks again") {
	OncePerTalk once;
	CHECK(once.Take(true, 7, true));
	CHECK_FALSE(once.Take(false, 0, false));
	CHECK(once.Take(true, 7, true));
}

TEST_CASE("a talk with another NPC picks again") {
	OncePerTalk once;
	CHECK(once.Take(true, 7, true));
	CHECK(once.Take(true, 8, true));
	CHECK_FALSE(once.Take(true, 8, true));
}

TEST_CASE("nothing is picked while no NPC is being talked to") {
	OncePerTalk once;
	CHECK_FALSE(once.Take(false, 0, true));
	CHECK_FALSE(once.Take(false, 7, true));
}

TEST_CASE("a reset forgets the talk") {
	OncePerTalk once;
	CHECK(once.Take(true, 7, true));
	once.Reset();
	CHECK(once.Take(true, 7, true));
}
