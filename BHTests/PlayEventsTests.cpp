#include "doctest.h"
#include <string>
#include <vector>
#include "Modules/Tracking/PlayEvents.h"

/*
 * A session's events as lines of its log.
 *
 * The log is the session, so a line has to come back as the event that wrote
 * it, and a line that is not an event has to be told apart from one that is: a
 * client can die halfway through a line, and a later build can write kinds of
 * event this one has never heard of.
 */

using namespace PlayEvents;

namespace {

Started SomeSession() {
	Started fact;
	fact.startedAt = 1795774500;
	fact.account = "someaccount";
	fact.realm = "Slash";
	fact.character = "Sorc_One";
	fact.classId = 1;
	fact.hardcore = true;
	fact.expansion = true;
	fact.version = "1.10.1";
	return fact;
}

std::vector<Event> OneOfEach() {
	return {
		{ 0, SomeSession() },
		{ 1500, Joined{ "baal-12", 2, 87, 2097310703ull } },
		{ 1500, AreaEntered{ 109, true } },
		{ 1600, Players{ 8 } },
		{ 64000, Experience{ 2097999999ull } },
		{ 64100, LevelReached{ 88 } },
		{ 70000, Died() },
		{ 71000, AreaEntered{ 131, false, "Throne of Destruction" } },
		{ 72000, Killed{ 131, 571, "Minion of Destruction", TierChampion, 3 } },
		{ 73000, Notable{ 131, 571, "Lister the Tormentor", "Minion of Destruction", TierSuperUnique, 65 } },
		{ 74000, Notable{ 132, 544, "Baal", "Baal", TierBoss, -1 } },
		{ 75000, Dropped{ 1001, "Ist Rune", "r24", 2, 8, 1, 131, "Throne of Destruction" } },
		{ 76000, Dropped{ 1002, "Ring", "rin", 6, 9, 3, 131, "" } },
		{ 77000, Quest{ 6, 0x201e } },
		{ 78000, ItemHeld{ "box" } },
		{ 79000, RunStarted{ "bosses-normal", "Act bosses (Normal)", { "Andariel", "Duriel" } } },
		{ 80000, SplitReached{ 0, false } },
		{ 81000, SplitReached{ 1, true } },
		{ 82000, SplitUndone{ 1 } },
		{ 83000, SplitSkipped{ 1 } },
		{ 84000, RunEnded{ true } },
		{ 85000, RunEnded{ false } },
		{ 91000, Left() },
	};
}

}

TEST_CASE("a line is the event's time, its kind, and what it carries") {
	CHECK(ToLine({ 1500, Joined{ "baal-12", 2, 87, 2097310703ull } }) ==
		"{\"t\":1500,\"e\":\"join\",\"game\":\"baal-12\",\"diff\":2,\"level\":87,\"exp\":2097310703}");
	CHECK(ToLine({ 91000, Left() }) == "{\"t\":91000,\"e\":\"leave\"}");
	CHECK(ToLine({ 1500, AreaEntered{ 109, true } }) ==
		"{\"t\":1500,\"e\":\"area\",\"area\":109,\"town\":true}");
}

TEST_CASE("every kind of event comes back from its line as it went in") {
	for (const Event& event : OneOfEach()) {
		const std::string line = ToLine(event);
		CAPTURE(line);

		Event read;
		REQUIRE(FromLine(line, read));
		CHECK(read.at == event.at);
		CHECK(read.fact.index() == event.fact.index());
		CHECK(ToLine(read) == line);
	}
}

TEST_CASE("a session's first line says whose it is") {
	Event read;
	REQUIRE(FromLine(ToLine({ 0, SomeSession() }), read));

	const Started& fact = std::get<Started>(read.fact);
	CHECK(fact.startedAt == 1795774500);
	CHECK(fact.account == "someaccount");
	CHECK(fact.realm == "Slash");
	CHECK(fact.character == "Sorc_One");
	CHECK(fact.classId == 1);
	CHECK(fact.hardcore);
	CHECK(fact.expansion);
	CHECK(fact.version == "1.10.1");
}

TEST_CASE("an area is written with its name where it has one") {
	CHECK(ToLine({ 5, AreaEntered{ 131, false, "Throne of Destruction" } }) ==
		"{\"t\":5,\"e\":\"area\",\"area\":131,\"town\":false,\"name\":\"Throne of Destruction\"}");
}

TEST_CASE("kills are written by area, kind of monster and tier") {
	CHECK(ToLine({ 5, Killed{ 131, 571, "Minion of Destruction", TierChampion, 3 } }) ==
		"{\"t\":5,\"e\":\"kills\",\"area\":131,\"class\":571,\"name\":\"Minion of Destruction\","
		"\"tier\":\"champion\",\"n\":3}");

	Event read;
	REQUIRE(FromLine(ToLine({ 5, Killed{ 131, 571, "Minion of Destruction", TierChampion, 3 } }), read));
	const Killed& fact = std::get<Killed>(read.fact);
	CHECK(fact.area == 131);
	CHECK(fact.classId == 571);
	CHECK(fact.name == "Minion of Destruction");
	CHECK(fact.tier == TierChampion);
	CHECK(fact.count == 3);
}

TEST_CASE("every tier comes back from its name") {
	for (int tier = 0; tier < TierCount; tier++) {
		Event read;
		REQUIRE(FromLine(ToLine({ 5, Killed{ 1, 2, "x", (Tier)tier, 1 } }), read));
		CHECK(std::get<Killed>(read.fact).tier == (Tier)tier);
	}
}

TEST_CASE("a tier this build has no name for is still a kill") {
	Event read;
	REQUIRE(FromLine("{\"t\":5,\"e\":\"kills\",\"area\":1,\"class\":2,\"name\":\"x\",\"tier\":\"mythic\",\"n\":4}", read));
	CHECK(std::get<Killed>(read.fact).tier == TierNormal);
	CHECK(std::get<Killed>(read.fact).count == 4);
}

TEST_CASE("a unique monster is written with its own name and its kind's") {
	Event read;
	REQUIRE(FromLine(ToLine({ 5, Notable{ 131, 571, "Lister the Tormentor", "Minion of Destruction",
		TierSuperUnique, 65 } }), read));
	const Notable& fact = std::get<Notable>(read.fact);
	CHECK(fact.name == "Lister the Tormentor");
	CHECK(fact.kind == "Minion of Destruction");
	CHECK(fact.tier == TierSuperUnique);
	CHECK(fact.superUnique == 65);
}

TEST_CASE("a unique monster that is no super unique says nothing of which one") {
	const std::string line = ToLine({ 5, Notable{ 132, 544, "Baal", "Baal", TierBoss, -1 } });
	CHECK(line.find("\"su\"") == std::string::npos);

	Event read;
	REQUIRE(FromLine(line, read));
	CHECK(std::get<Notable>(read.fact).superUnique == -1);
}

TEST_CASE("the first super unique of the table is told from none at all") {
	Event read;
	REQUIRE(FromLine(ToLine({ 5, Notable{ 3, 19, "Bishibosh", "Fallen Shaman", TierSuperUnique, 0 } }), read));
	CHECK(std::get<Notable>(read.fact).superUnique == 0);
}

TEST_CASE("a drop is written with what it was and where") {
	Event read;
	REQUIRE(FromLine(ToLine({ 5, Dropped{ 1001, "Ist Rune", "r24", 2, 8, 1, 131, "Throne of Destruction" } }), read));
	const Dropped& fact = std::get<Dropped>(read.fact);
	CHECK(fact.id == 1001);
	CHECK(fact.name == "Ist Rune");
	CHECK(fact.code == "r24");
	CHECK(fact.quality == 2);
	CHECK(fact.color == 8);
	CHECK(fact.ping == 1);
	CHECK(fact.area == 131);
	CHECK(fact.areaName == "Throne of Destruction");
}

TEST_CASE("a run is written with its route and the names its splits had") {
	const std::string line = ToLine({ 5, RunStarted{ "bosses-normal", "Act bosses (Normal)",
		{ "Andariel", "Duriel" } } });
	CHECK(line == "{\"t\":5,\"e\":\"run\",\"route\":\"bosses-normal\",\"name\":\"Act bosses (Normal)\","
		"\"splits\":[\"Andariel\",\"Duriel\"]}");

	Event read;
	REQUIRE(FromLine(line, read));
	const RunStarted& fact = std::get<RunStarted>(read.fact);
	CHECK(fact.route == "bosses-normal");
	CHECK(fact.name == "Act bosses (Normal)");
	CHECK(fact.splits == std::vector<std::string>{ "Andariel", "Duriel" });
}

TEST_CASE("a split says which it is, and that the player marked it where they did") {
	CHECK(ToLine({ 5, SplitReached{ 2, false } }) == "{\"t\":5,\"e\":\"split\",\"index\":2}");
	CHECK(ToLine({ 5, SplitReached{ 2, true } }) ==
		"{\"t\":5,\"e\":\"split\",\"index\":2,\"manual\":true}");
	CHECK(ToLine({ 5, SplitUndone{ 2 } }) == "{\"t\":5,\"e\":\"unsplit\",\"index\":2}");
	CHECK(ToLine({ 5, SplitSkipped{ 2 } }) == "{\"t\":5,\"e\":\"skip\",\"index\":2}");

	Event read;
	REQUIRE(FromLine("{\"t\":5,\"e\":\"split\",\"index\":2,\"manual\":true}", read));
	CHECK(std::get<SplitReached>(read.fact).index == 2);
	CHECK(std::get<SplitReached>(read.fact).manual);
}

TEST_CASE("the end of a run says whether it was finished") {
	CHECK(ToLine({ 5, RunEnded{ true } }) == "{\"t\":5,\"e\":\"end\",\"finished\":true}");

	Event read;
	REQUIRE(FromLine("{\"t\":5,\"e\":\"end\",\"finished\":false}", read));
	CHECK_FALSE(std::get<RunEnded>(read.fact).finished);
}

TEST_CASE("a quest's state and an item held are written as they were read") {
	CHECK(ToLine({ 5, Quest{ 14, 0x002c } }) == "{\"t\":5,\"e\":\"quest\",\"quest\":14,\"bits\":44}");
	CHECK(ToLine({ 5, ItemHeld{ "box" } }) == "{\"t\":5,\"e\":\"item\",\"code\":\"box\"}");

	Event read;
	REQUIRE(FromLine("{\"t\":5,\"e\":\"quest\",\"quest\":14,\"bits\":44}", read));
	CHECK(std::get<Quest>(read.fact).quest == 14);
	CHECK(std::get<Quest>(read.fact).bits == 0x002c);
}

TEST_CASE("experience past what a signed 32-bit number holds is kept whole") {
	// Level 99 is 3,520,485,254.
	Event read;
	REQUIRE(FromLine(ToLine({ 5, Experience{ 3520485254ull } }), read));
	CHECK(std::get<Experience>(read.fact).total == 3520485254ull);
}

TEST_CASE("a line cut short is not an event") {
	const std::string line = ToLine({ 1500, Joined{ "baal-12", 2, 87, 2097310703ull } });
	Event read;
	for (size_t length = 0; length < line.size(); length++) {
		CAPTURE(length);
		CHECK_FALSE(FromLine(line.substr(0, length), read));
	}
}

TEST_CASE("a kind of event this build does not know is not an event") {
	Event read;
	CHECK_FALSE(FromLine("{\"t\":10,\"e\":\"something-new\",\"n\":1}", read));
}

TEST_CASE("a line without a time or a kind is not an event") {
	Event read;
	CHECK_FALSE(FromLine("{\"e\":\"leave\"}", read));
	CHECK_FALSE(FromLine("{\"t\":10}", read));
	CHECK_FALSE(FromLine("{\"t\":-10,\"e\":\"leave\"}", read));
	CHECK_FALSE(FromLine("[10,\"leave\"]", read));
	CHECK_FALSE(FromLine("", read));
}

TEST_CASE("a field that is missing reads as nothing rather than refusing the line") {
	Event read;
	REQUIRE(FromLine("{\"t\":10,\"e\":\"join\"}", read));
	const Joined& fact = std::get<Joined>(read.fact);
	CHECK(fact.game.empty());
	CHECK(fact.difficulty == 0);
	CHECK(fact.level == 0);
	CHECK(fact.experience == 0);
}

TEST_CASE("a name that is not UTF-8 is written rather than losing the event") {
	// A game named in the system's code page: 0xE9 alone is not UTF-8.
	const std::string line = ToLine({ 10, Joined{ "caf\xE9", 0, 1, 0 } });

	Event read;
	REQUIRE(FromLine(line, read));
	CHECK(std::get<Joined>(read.fact).game.substr(0, 3) == "caf");
}
