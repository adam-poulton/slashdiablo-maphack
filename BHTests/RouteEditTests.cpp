#include "doctest.h"
#include <set>
#include <string>
#include <vector>
#include "Modules/Race/RouteEdit.h"

/*
 * Changing routes: what there is to build one from, and what can be done to it.
 */

using Splits::Route;
using Splits::Split;
using Splits::Trigger;

namespace {

const Split* Named(const std::string& name) {
	for (const Split& split : RouteEdit::Catalogue()) {
		if (split.name == name)
			return &split;
	}
	return NULL;
}

std::vector<std::string> Names(const Route& route) {
	std::vector<std::string> names;
	for (const Split& split : route.splits)
		names.push_back(split.name);
	return names;
}

Split Plain(const std::string& name) {
	return { name, -1, false, {} };
}

Route Abc() {
	return { "abc", "A, B and C", { Plain("A"), Plain("B"), Plain("C") } };
}

}

TEST_CASE("the catalogue has every split in each difficulty, under a name of its own") {
	const std::vector<Split>& catalogue = RouteEdit::Catalogue();
	CHECK(catalogue.size() % 3 == 0);

	std::set<std::string> names;
	int perDifficulty[3] = { 0, 0, 0 };
	for (const Split& split : catalogue) {
		CHECK(names.insert(split.name).second);
		REQUIRE(split.difficulty >= 0);
		REQUIRE(split.difficulty < 3);
		perDifficulty[split.difficulty]++;
		CHECK_FALSE(split.when.empty());
	}
	CHECK(perDifficulty[0] == perDifficulty[1]);
	CHECK(perDifficulty[1] == perDifficulty[2]);
}

TEST_CASE("an act boss in the catalogue is the split the routes BH comes with use") {
	const Split preset = Splits::Presets()[0].splits[1];
	const Split* duriel = Named("Duriel");
	REQUIRE(duriel != NULL);
	CHECK(preset.name == "Duriel");
	CHECK(duriel->difficulty == preset.difficulty);
	REQUIRE(duriel->when.size() == preset.when.size());
	for (size_t i = 0; i < preset.when.size(); i++) {
		CHECK(duriel->when[i].kind == preset.when[i].kind);
		CHECK(duriel->when[i].value == preset.when[i].value);
		CHECK(duriel->when[i].mask == preset.when[i].mask);
	}

	const Split* hellBaal = Named("Baal (Hell)");
	REQUIRE(hellBaal != NULL);
	CHECK(hellBaal->difficulty == 2);
	CHECK(hellBaal->when[0].kind == Trigger::Boss);
	CHECK(hellBaal->when[0].value == 544);
}

TEST_CASE("every quest of a difficulty is in the catalogue once") {
	std::set<unsigned int> quests;
	for (const Split& split : RouteEdit::Catalogue()) {
		if (split.difficulty != 0)
			continue;
		for (const Trigger& trigger : split.when) {
			if (trigger.kind == Trigger::Quest && trigger.mask == Splits::kQuestDone)
				CHECK(quests.insert(trigger.value).second);
		}
	}
	// Duriel's is watched by another bit than the one that says it is done.
	quests.insert(14);
	const std::vector<unsigned int>& all = Splits::QuestIds();
	CHECK(quests == std::set<unsigned int>(all.begin(), all.end()));
}

TEST_CASE("the Cow King and the quest items are there to add") {
	const Split* cowKing = Named("Cow King (NM)");
	REQUIRE(cowKing != NULL);
	CHECK(cowKing->difficulty == 1);
	CHECK(cowKing->when[0].kind == Trigger::Quest);
	CHECK(cowKing->when[0].value == 4);
	CHECK(cowKing->when[0].mask == 0x0400);

	const Split* cube = Named("Horadric Cube");
	REQUIRE(cube != NULL);
	CHECK(cube->when[0].kind == Trigger::Item);
	CHECK(cube->when[0].code == "box");

	const Split* all = Named("All quests (Hell)");
	REQUIRE(all != NULL);
	CHECK(all->when[0].kind == Trigger::AllQuests);
}

TEST_CASE("a level to reach is asked for in words") {
	Split split;
	REQUIRE(RouteEdit::LevelSplit("level 24", split));
	CHECK(split.name == "Level 24");
	CHECK(split.difficulty == -1);
	REQUIRE(split.when.size() == 1);
	CHECK(split.when[0].kind == Trigger::Level);
	CHECK(split.when[0].value == 24);

	CHECK(RouteEdit::LevelSplit("Level 99", split));
	CHECK(split.when[0].value == 99);
}

TEST_CASE("what is not a level a character can reach is not one to add") {
	Split split;
	CHECK_FALSE(RouteEdit::LevelSplit("", split));
	CHECK_FALSE(RouteEdit::LevelSplit("level", split));
	CHECK_FALSE(RouteEdit::LevelSplit("level ", split));
	CHECK_FALSE(RouteEdit::LevelSplit("level 1", split));
	CHECK_FALSE(RouteEdit::LevelSplit("level 100", split));
	CHECK_FALSE(RouteEdit::LevelSplit("level 2x", split));
	CHECK_FALSE(RouteEdit::LevelSplit("level -5", split));
	CHECK_FALSE(RouteEdit::LevelSplit("levels 24", split));
}

TEST_CASE("a split says in a few words what marks it") {
	CHECK(RouteEdit::Describe(*Named("Andariel")) == "Normal: kill or quest");
	CHECK(RouteEdit::Describe(*Named("Horadric Cube (Hell)")) == "Hell: item box");
	CHECK(RouteEdit::Describe(*Named("All quests (NM)")) == "Nightmare: all quests");

	Split level;
	REQUIRE(RouteEdit::LevelSplit("level 40", level));
	CHECK(RouteEdit::Describe(level) == "level 40");

	const Split complete = Splits::Presets().back().splits.back();
	CHECK(RouteEdit::Describe(complete) == "Normal: all quests and quest and level 40");

	CHECK(RouteEdit::Describe(Plain("By hand")) == "marked by hand");
	CHECK(RouteEdit::Describe({ "Catacombs", -1, false, { { Trigger::Area, 37, 0, "" } } }) ==
		"area 37");
}

TEST_CASE("a split's difficulty and what marks it are each said on their own") {
	CHECK(RouteEdit::DifficultyOf(*Named("Andariel")) == "Normal");
	CHECK(RouteEdit::MarkedBy(*Named("Andariel")) == "kill or quest");
	CHECK(RouteEdit::DifficultyOf(*Named("Horadric Cube (Hell)")) == "Hell");
	CHECK(RouteEdit::MarkedBy(*Named("Horadric Cube (Hell)")) == "item box");

	Split level;
	REQUIRE(RouteEdit::LevelSplit("level 40", level));
	CHECK(RouteEdit::DifficultyOf(level) == "Any difficulty");
	CHECK(RouteEdit::MarkedBy(level) == "level 40");

	CHECK(RouteEdit::MarkedBy(Plain("By hand")) == "hand");
}

TEST_CASE("a new route has no splits, and a name and an id no other route has") {
	std::vector<Route> routes = Splits::Presets();
	const size_t before = routes.size();

	const std::string first = RouteEdit::Add(routes, NULL);
	CHECK(first == "route-1");
	REQUIRE(routes.size() == before + 1);
	CHECK(routes.back().name == "My route 1");
	CHECK(routes.back().splits.empty());

	CHECK(RouteEdit::Add(routes, NULL) == "route-2");
	CHECK(routes.back().name == "My route 2");
}

TEST_CASE("a number a route has been named with is not given to another") {
	std::vector<Route> routes = { { "abc", "my ROUTE 1", {} }, { "route-2", "Something", {} } };
	CHECK(RouteEdit::Add(routes, NULL) == "route-3");
	CHECK(routes.back().name == "My route 3");
}

TEST_CASE("a copy of a route has its splits and is a route of its own") {
	std::vector<Route> routes = { Abc() };
	const Route original = routes[0];
	const std::string id = RouteEdit::Add(routes, &original);
	REQUIRE(routes.size() == 2);
	CHECK(id == "route-1");
	CHECK(routes[1].id == "route-1");
	CHECK(Names(routes[1]) == std::vector<std::string>{ "A", "B", "C" });
	CHECK(routes[0].id == "abc");
}

TEST_CASE("a route is removed and renamed by its id") {
	std::vector<Route> routes = { Abc(), { "other", "Other", {} } };
	CHECK(RouteEdit::Rename(routes, "other", "Mine"));
	CHECK(routes[1].name == "Mine");
	// A route is called something.
	CHECK_FALSE(RouteEdit::Rename(routes, "other", ""));
	CHECK(routes[1].name == "Mine");
	CHECK_FALSE(RouteEdit::Rename(routes, "none", "Nobody's"));

	RouteEdit::Remove(routes, "abc");
	REQUIRE(routes.size() == 1);
	CHECK(routes[0].id == "other");
	RouteEdit::Remove(routes, "none");
	CHECK(routes.size() == 1);
}

TEST_CASE("a route cannot take the name another has") {
	std::vector<Route> routes = { Abc(), { "other", "Other", {} } };
	const std::string before = routes[0].name;
	CHECK_FALSE(RouteEdit::Rename(routes, "abc", "OTHER"));
	CHECK(routes[0].name == before);
	// Its own, written another way, is no other's.
	CHECK(RouteEdit::Rename(routes, "other", "OTHER"));
	CHECK(routes[1].name == "OTHER");
}

TEST_CASE("the routes BH comes with are put back where they are missing, and no other is touched") {
	std::vector<Route> routes = Splits::Presets();
	const size_t all = routes.size();
	CHECK_FALSE(RouteEdit::PresetsMissing(routes));
	RouteEdit::Remove(routes, "act3");
	RouteEdit::Remove(routes, "bosses-hell");
	CHECK(RouteEdit::PresetsMissing(routes));
	routes[0].name = "Renamed";
	routes.push_back(Abc());

	RouteEdit::Restore(routes);
	CHECK_FALSE(RouteEdit::PresetsMissing(routes));
	CHECK(routes.size() == all + 1);
	CHECK(Splits::Find(routes, "act3") != NULL);
	CHECK(Splits::Find(routes, "bosses-hell") != NULL);
	CHECK(routes[0].name == "Renamed");
	CHECK(Splits::Find(routes, "abc") != NULL);

	RouteEdit::Restore(routes);
	CHECK(routes.size() == all + 1);
}

TEST_CASE("a split is added after the one picked out, or at the end") {
	Route route = Abc();
	CHECK(RouteEdit::AddSplit(route, Plain("X"), 0) == 1);
	CHECK(Names(route) == std::vector<std::string>{ "A", "X", "B", "C" });
	CHECK(RouteEdit::AddSplit(route, Plain("Y"), -1) == 4);
	CHECK(RouteEdit::AddSplit(route, Plain("Z"), 99) == 5);
	CHECK(Names(route) == std::vector<std::string>{ "A", "X", "B", "C", "Y", "Z" });

	Route none = { "none", "None", {} };
	CHECK(RouteEdit::AddSplit(none, Plain("First"), -1) == 0);
	CHECK(Names(none) == std::vector<std::string>{ "First" });
}

TEST_CASE("a split is removed by its place") {
	Route route = Abc();
	RouteEdit::RemoveSplit(route, 1);
	CHECK(Names(route) == std::vector<std::string>{ "A", "C" });
	RouteEdit::RemoveSplit(route, 5);
	CHECK(route.splits.size() == 2);
}

TEST_CASE("a split moves a place at a time, and not past either end") {
	Route route = Abc();
	CHECK(RouteEdit::MoveSplit(route, 0, 1));
	CHECK(Names(route) == std::vector<std::string>{ "B", "A", "C" });
	CHECK(RouteEdit::MoveSplit(route, 2, -1));
	CHECK(Names(route) == std::vector<std::string>{ "B", "C", "A" });

	CHECK_FALSE(RouteEdit::MoveSplit(route, 0, -1));
	CHECK_FALSE(RouteEdit::MoveSplit(route, 2, 1));
	CHECK_FALSE(RouteEdit::MoveSplit(route, 3, -1));
	CHECK_FALSE(RouteEdit::MoveSplit(route, 1, 2));
	CHECK(Names(route) == std::vector<std::string>{ "B", "C", "A" });
}
