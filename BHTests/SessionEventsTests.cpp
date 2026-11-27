#include "doctest.h"
#include <string>
#include <vector>
#include "Modules/Session/SessionEvents.h"

/*
 * A session's events as lines of its log.
 *
 * The log is the session, so a line has to come back as the event that wrote
 * it, and a line that is not an event has to be told apart from one that is: a
 * client can die halfway through a line, and a later build can write kinds of
 * event this one has never heard of.
 */

using namespace SessionEvents;

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
