#include "doctest.h"
#include <string>
#include <vector>
#include "Modules/Session/SplitRoute.h"

/*
 * The routes a run is timed against: the ones BH comes with, and the file they
 * are kept in for a player to change.
 */

using namespace Splits;

namespace {

const Route& Preset(const std::vector<Route>& routes, const char* id) {
	const Route* found = Find(routes, id);
	REQUIRE(found != NULL);
	return *found;
}

std::vector<std::string> NamesOf(const Route& route) {
	std::vector<std::string> names;
	for (const Split& split : route.splits)
		names.push_back(split.name);
	return names;
}

std::vector<Route> ReadOrFail(const std::string& text) {
	std::vector<Route> routes;
	REQUIRE(Read(text, routes, NULL));
	return routes;
}

}

TEST_CASE("the route BH starts with is the five act bosses in Normal") {
	const std::vector<Route> presets = Presets();
	const Route& route = Preset(presets, "bosses-normal");

	CHECK(route.name == "Act bosses (Normal)");
	CHECK(NamesOf(route) == std::vector<std::string>{ "Andariel", "Duriel", "Mephisto", "Diablo", "Baal" });
	for (const Split& split : route.splits) {
		CHECK(split.difficulty == 0);
		CHECK_FALSE(split.all);
	}
}

TEST_CASE("an act boss is marked by its death, or by the quest for it where that went unseen") {
	const std::vector<Route> presets = Presets();
	const Split& andariel = Preset(presets, "bosses-normal").splits[0];

	REQUIRE(andariel.when.size() == 2);
	CHECK(andariel.when[0].kind == Trigger::Boss);
	CHECK(andariel.when[0].value == 156);
	CHECK(andariel.when[1].kind == Trigger::Quest);
	CHECK(andariel.when[1].value == 6);
	CHECK(andariel.when[1].mask == kQuestDone);
}

TEST_CASE("Duriel's quest is watched for the bit his death sets, not for the quest being done") {
	const std::vector<Route> presets = Presets();
	const Split& duriel = Preset(presets, "bosses-normal").splits[1];

	CHECK(duriel.when[0].value == 211);
	CHECK(duriel.when[1].value == 14);
	CHECK(duriel.when[1].mask == 0x0020);
}

TEST_CASE("the route through Hell is the act bosses three times over, each in its difficulty") {
	const std::vector<Route> presets = Presets();
	const Route& route = Preset(presets, "bosses-hell");

	REQUIRE(route.splits.size() == 15);
	CHECK(route.splits[0].name == "Andariel");
	CHECK(route.splits[0].difficulty == 0);
	CHECK(route.splits[5].name == "Andariel (NM)");
	CHECK(route.splits[5].difficulty == 1);
	CHECK(route.splits[14].name == "Baal (Hell)");
	CHECK(route.splits[14].difficulty == 2);
}

TEST_CASE("there is a route to each act's boss alone") {
	const std::vector<Route> presets = Presets();
	const char* const bosses[] = { "Andariel", "Duriel", "Mephisto", "Diablo", "Baal" };
	for (int act = 1; act <= 5; act++) {
		const Route& route = Preset(presets, ("act" + std::to_string(act)).c_str());
		REQUIRE(route.splits.size() == 1);
		CHECK(route.splits[0].name == bosses[act - 1]);
	}
}

TEST_CASE("the milestones are the bosses and the steps a Normal run passes on the way") {
	const std::vector<Route> presets = Presets();
	const Route& route = Preset(presets, "milestones-normal");

	CHECK(route.splits.front().name == "Andariel");
	CHECK(route.splits.back().name == "Baal");
	CHECK(route.splits.size() == 17);

	const Split& cube = route.splits[2];
	CHECK(cube.name == "Horadric Cube");
	REQUIRE(cube.when.size() == 1);
	CHECK(cube.when[0].kind == Trigger::Item);
	CHECK(cube.when[0].code == "box");
}

TEST_CASE("the completionist route ends on a split that wants every goal at once") {
	const std::vector<Route> presets = Presets();
	const Route& route = Preset(presets, "completionist-normal");

	CHECK(NamesOf(route) == std::vector<std::string>{ "All quests", "Cow King", "Level 40", "Complete" });
	CHECK_FALSE(route.splits[0].all);
	CHECK(route.splits[3].all);
	CHECK(route.splits[3].when.size() == 3);
	CHECK(route.splits[2].when[0].kind == Trigger::Level);
	CHECK(route.splits[2].when[0].value == 40);
}

TEST_CASE("a difficulty has twenty seven quests") {
	CHECK(QuestIds().size() == 27);
	CHECK(QuestIds().front() == 1);
	CHECK(QuestIds().back() == 40);
}

TEST_CASE("the routes are written to a file and read back as they were") {
	const std::string written = Write(Presets());
	CHECK(Write(ReadOrFail(written)) == written);
	CHECK(ReadOrFail(written).size() == Presets().size());
}

TEST_CASE("a route is found by its id or its name, whatever the case") {
	const std::vector<Route> presets = Presets();
	CHECK(Find(presets, "BOSSES-NORMAL")->id == "bosses-normal");
	CHECK(Find(presets, "act bosses (through hell)")->id == "bosses-hell");
	CHECK(Find(presets, "nothing") == NULL);
	CHECK(Find(presets, "") == NULL);
}

TEST_CASE("the items a route is marked by are what there is to watch for") {
	const std::set<std::string> codes = ItemCodes(Presets());
	CHECK(codes == std::set<std::string>{ "box", "msf", "vip", "qf2" });
}

TEST_CASE("a file that is not JSON is refused, with where it stopped being") {
	std::vector<Route> routes;
	std::string where;
	CHECK_FALSE(Read("{\n  \"routes\": [\n    {,\n", routes, &where));
	CHECK(where == "line 3, column 6");
}

TEST_CASE("a file with no routes in it is refused") {
	std::vector<Route> routes;
	CHECK_FALSE(Read("{}", routes, NULL));
	CHECK_FALSE(Read("[]", routes, NULL));
	CHECK_FALSE(Read("{\"routes\": 4}", routes, NULL));
}

TEST_CASE("every kind of trigger is read") {
	const std::vector<Route> routes = ReadOrFail(R"({"routes": [{"id": "mine", "name": "Mine", "splits": [
		{"name": "A", "difficulty": "Nightmare", "when": [
			{"boss": 156}, {"superunique": 42}, {"quest": 9, "mask": 32}, {"area": 131},
			{"level": 24}, {"item": "box"}, {"allQuests": true}]},
		{"name": "B", "all": true, "when": [{"quest": 6}]}
	]}]})");

	REQUIRE(routes.size() == 1);
	const Split& a = routes[0].splits[0];
	CHECK(a.difficulty == 1);
	REQUIRE(a.when.size() == 7);
	CHECK(a.when[0].kind == Trigger::Boss);
	CHECK(a.when[1].kind == Trigger::SuperUnique);
	CHECK(a.when[1].value == 42);
	CHECK(a.when[2].kind == Trigger::Quest);
	CHECK(a.when[2].mask == 32);
	CHECK(a.when[3].kind == Trigger::Area);
	CHECK(a.when[3].value == 131);
	CHECK(a.when[4].kind == Trigger::Level);
	CHECK(a.when[5].kind == Trigger::Item);
	CHECK(a.when[5].code == "box");
	CHECK(a.when[6].kind == Trigger::AllQuests);

	const Split& b = routes[0].splits[1];
	CHECK(b.difficulty == -1);
	CHECK(b.all);
	// A quest named without its bits means the quest done.
	CHECK(b.when[0].mask == kQuestDone);
}

TEST_CASE("what cannot be made sense of is left out, and the rest is read") {
	const std::vector<Route> routes = ReadOrFail(R"({"routes": [
		{"name": "No id", "splits": [{"name": "A", "when": [{"boss": 1}]}]},
		{"id": "kept", "splits": [
			{"when": [{"boss": 1}]},
			{"name": "A", "when": [{"dragon": 7}, {"boss": 2}, 12]},
			{"name": "By hand"}
		]}
	]})");

	REQUIRE(routes.size() == 1);
	CHECK(routes[0].id == "kept");
	// A route with no name is called by its id.
	CHECK(routes[0].name == "kept");
	REQUIRE(routes[0].splits.size() == 2);
	CHECK(routes[0].splits[0].name == "A");
	REQUIRE(routes[0].splits[0].when.size() == 1);
	CHECK(routes[0].splits[0].when[0].value == 2);
	// A split nothing marks is one to split by hand.
	CHECK(routes[0].splits[1].name == "By hand");
	CHECK(routes[0].splits[1].when.empty());
}

TEST_CASE("a route with no splits yet is read as one still being put together") {
	const std::vector<Route> routes = ReadOrFail(R"({"routes": [
		{"id": "empty", "name": "Mine", "splits": []},
		{"id": "none"}
	]})");
	REQUIRE(routes.size() == 2);
	CHECK(routes[0].name == "Mine");
	CHECK(routes[0].splits.empty());
	CHECK(routes[1].splits.empty());

	std::vector<Route> again;
	REQUIRE(Splits::Read(Splits::Write(routes), again, NULL));
	CHECK(again.size() == 2);
}
