#include "doctest.h"
#include <string>
#include <vector>
#include "Modules/Race/RaceRows.h"

/*
 * What the race window lists, worked out from runs and routes without the game.
 */

using namespace PlayEvents;
using namespace RaceRows;

namespace {

const unsigned long long kSecond = 1000;
const unsigned long long kMinute = 60 * kSecond;

RunModel After(const std::vector<Event>& events) {
	RunModel model;
	for (const Event& event : events)
		model.Apply(event);
	return model;
}

std::vector<std::string> Headings(const std::vector<Row>& rows) {
	std::vector<std::string> headings;
	for (const Row& row : rows) {
		if (row.heading)
			headings.push_back(row.cells[0]);
	}
	return headings;
}

// The route's own rows: the ones over the row that names the runs that are over.
std::vector<Row> Top(const std::vector<Row>& rows) {
	std::vector<Row> top;
	for (const Row& row : rows) {
		if (row.cells[SplitName] == kEarlierRuns)
			break;
		top.push_back(row);
	}
	return top;
}

// The rows of the runs that are over: the ones after the row that names them.
std::vector<Row> Earlier(const std::vector<Row>& rows) {
	std::vector<Row> earlier;
	bool inside = false;
	for (const Row& row : rows) {
		if (inside)
			earlier.push_back(row);
		else if (row.cells[SplitName] == kEarlierRuns)
			inside = true;
	}
	return earlier;
}

// What a row named in its first cell says in its second.
std::string Figure(const std::vector<Row>& rows, const std::string& name) {
	for (const Row& row : rows) {
		if (row.cells[SplitName] == name)
			return row.cells[SplitTime];
	}
	return "<none>";
}

const Row* Keyed(const std::vector<Row>& rows, const std::string& key) {
	for (const Row& row : rows) {
		if (row.key == key)
			return &row;
	}
	return NULL;
}

const Splits::Route& Bosses() {
	static const std::vector<Splits::Route> presets = Splits::Presets();
	return *Splits::Find(presets, "bosses-normal");
}

// A sorceress in softcore, with the act bosses picked unless told otherwise.
RunsShown Shown(const RunStore* store, unsigned long long now,
		RunStore::Clock clock = RunStore::RealTime, const Splits::Route* route = &Bosses()) {
	return { route, store, clock, now, 1, false };
}

RunStarted BossRun() {
	return RunStarted{ "bosses-normal", "Act bosses (Normal)",
		{ "Andariel", "Duriel", "Mephisto", "Diablo", "Baal" } };
}

// A finished run of the five bosses at these times, and a second in hand with
// the first two reached.
RunModel TwoRuns() {
	return After({
		{ 0, Joined{ "a", 0, 1, 0 } },
		{ 0, BossRun() },
		{ 100 * kSecond, SplitReached{ 0, false } },
		{ 250 * kSecond, SplitReached{ 1, false } },
		{ 400 * kSecond, SplitReached{ 2, false } },
		{ 600 * kSecond, SplitReached{ 3, false } },
		{ 900 * kSecond, SplitReached{ 4, false } },
		{ 900 * kSecond, RunEnded{ true } },
		{ 1000 * kSecond, BossRun() },
		{ 1090 * kSecond, LevelReached{ 9 } },
		{ 1090 * kSecond, SplitReached{ 0, false } },
		{ 1270 * kSecond, SplitReached{ 1, false } },
	});
}

RunStore StoreOf(const RunModel& model) {
	RunStore store;
	for (const RunModel::Run& run : model.Runs()) {
		if (run.ended)
			store.Record(RunStore::KeyFor(run.route, 1, false), run);
	}
	return store;
}

std::vector<Splits::Route> TwoRoutes() {
	return {
		{ "one", "First", { { "Andariel", 0, false, { { Splits::Trigger::Boss, 156, 0, "" } } },
			{ "By hand", -1, false, {} } } },
		{ "two", "Second", {} },
	};
}

// The names of the splits there are to add, in the order they are listed.
std::vector<std::string> Choices(const std::vector<Row>& rows) {
	std::vector<std::string> names;
	for (const Row& row : rows) {
		std::string rest;
		if (ListRows::KeyRest(row.key, RouteKeys::kAdd, rest))
			names.push_back(row.cells[ChoiceName]);
	}
	return names;
}

}

TEST_CASE("with no route picked nothing is listed") {
	const RunModel model = After({ { 0, Joined{ "a", 0, 1, 0 } } });
	CHECK(Runs(model, Shown(NULL, kMinute, RunStore::RealTime, NULL)).empty());
	CHECK(Runs(RunModel(), Shown(NULL, 0, RunStore::RealTime, NULL)).empty());
}

TEST_CASE("with no run in hand the route picked is listed as it stands before a run") {
	const RunModel model = After({ { 0, Joined{ "a", 0, 1, 0 } } });
	const std::vector<Row> rows = Runs(model, Shown(NULL, kMinute));

	CHECK(Headings(rows).empty());
	const std::vector<Row> splits = Top(rows);
	REQUIRE(splits.size() == 5);
	// No split is the one a run is on, and none has a time.
	CHECK(splits[0].cells[SplitName] == "Andariel");
	CHECK(splits[0].cells[SplitTime] == "");
	CHECK(splits[0].cells[SplitBest] == "");
	CHECK(splits[4].cells[SplitName] == "Baal");
}

TEST_CASE("a route that has been run is listed with its times to beat") {
	RunModel model = TwoRuns();
	model.Apply({ 1300 * kSecond, RunEnded{ false } });
	const RunStore store = StoreOf(model);
	const std::vector<Row> splits = Top(Runs(model, Shown(&store, 1400 * kSecond)));

	CHECK(splits[0].cells[SplitName] == "Andariel");
	CHECK(splits[0].cells[SplitBest] == "1:40");
	// The second run did its first stretch in 90.
	CHECK(splits[0].cells[SplitSegment] == "1:30");
	CHECK(Figure(splits, "Best run") == "15:00");
	CHECK(Figure(splits, "Finished") == "1 of 2");
	// A run that is not going has no time of its own, and none it could still do.
	CHECK(Figure(splits, "Run time") == "<none>");
	CHECK(Figure(splits, "Best possible") == "<none>");
}

TEST_CASE("a route with no splits has nothing to list") {
	const std::vector<Splits::Route> routes = TwoRoutes();
	CHECK(Runs(RunModel(), Shown(NULL, 0, RunStore::RealTime, &routes[1])).empty());
}

TEST_CASE("the run in hand is listed split by split against the best run") {
	const RunModel model = TwoRuns();
	const RunStore store = StoreOf(model);
	const std::vector<Row> rows = Runs(model, Shown(&store, 1300 * kSecond));

	// The run that is over is named under the one in hand, and no row is a
	// heading: a heading would set every split in from the left.
	CHECK(Headings(rows).empty());
	CHECK(Earlier(rows).size() == 1);
	const std::vector<Row> splits = Top(rows);

	// Ten seconds ahead, and the fastest that stretch has been done.
	CHECK(splits[0].cells[SplitName] == "Andariel");
	CHECK(splits[0].cells[SplitTime] == "1:30");
	CHECK(splits[0].cells[SplitDelta] == "-10.0");
	CHECK(splits[0].colors[SplitDelta] == kColorBest);
	CHECK(splits[0].cells[SplitBest] == "1:40");
	CHECK(splits[0].cells[SplitSegment] == "1:40");
	CHECK(splits[0].cells[SplitLevel] == "9");

	// Twenty behind.
	CHECK(splits[1].cells[SplitTime] == "4:30");
	CHECK(splits[1].cells[SplitDelta] == "+20.0");
	CHECK(splits[1].colors[SplitDelta] == kColorBehind);
	CHECK(splits[1].cells[SplitSegment] == "2:30");

	// The split the run is on, with the time to beat and nothing of its own.
	CHECK(splits[2].cells[SplitName] == "> Mephisto");
	CHECK(splits[2].cells[SplitTime] == "");
	CHECK(splits[2].cells[SplitDelta] == "");
	CHECK(splits[2].cells[SplitBest] == "6:40");
	CHECK(splits[2].cells[SplitLevel] == "");
	CHECK(splits[2].colors[SplitDelta] == -1);
}

TEST_CASE("under the splits are the run's time and what it is up against") {
	const RunModel model = TwoRuns();
	const RunStore store = StoreOf(model);
	const std::vector<Row> splits = Top(Runs(model, Shown(&store, 1300 * kSecond)));

	CHECK(Figure(splits, "Run time") == "5:00");
	CHECK(Figure(splits, "Best run") == "15:00");
	CHECK(Figure(splits, "Sum of best") == "15:00");
	// 4:30 done, and the three stretches left at 2:30, 3:20 and 5:00.
	CHECK(Figure(splits, "Best possible") == "15:20");
	CHECK(Figure(splits, "Finished") == "1 of 1");
	for (const Row& row : splits) {
		if (row.cells[SplitName] == "Run time")
			CHECK(row.summary);
	}
}

TEST_CASE("a route's bests are those of the class playing it") {
	const RunModel model = TwoRuns();
	const RunStore store = StoreOf(model);
	RunsShown shown = Shown(&store, 1300 * kSecond);
	shown.classId = 4;
	const std::vector<Row> splits = Top(Runs(model, shown));
	CHECK(splits[0].cells[SplitDelta] == "");
	CHECK(Figure(splits, "Best run") == "<none>");
}

TEST_CASE("what was run on a route before its splits changed is nothing to it now") {
	const RunModel model = TwoRuns();
	const RunStore store = StoreOf(model);
	Splits::Route longer = Bosses();
	longer.splits.push_back({ "Cow King", 0, false, {} });

	const std::vector<Row> splits = Top(
		Runs(RunModel(), Shown(&store, 0, RunStore::RealTime, &longer)));
	REQUIRE(splits.size() == 6);
	CHECK(splits[0].cells[SplitBest] == "");
	CHECK(Figure(splits, "Best run") == "<none>");
	CHECK(Figure(splits, "Finished") == "<none>");
}

TEST_CASE("a run with nothing to be compared with has only its own times") {
	const RunModel model = After({
		{ 0, Joined{ "a", 0, 1, 0 } },
		{ 0, BossRun() },
		{ 90 * kSecond, SplitReached{ 0, false } },
	});
	const std::vector<Row> splits = Top(Runs(model, Shown(NULL, 100 * kSecond)));

	CHECK(splits[0].cells[SplitTime] == "1:30");
	CHECK(splits[0].cells[SplitDelta] == "");
	CHECK(splits[0].cells[SplitBest] == "");
	CHECK(Figure(splits, "Run time") == "1:40");
	CHECK(Figure(splits, "Best run") == "<none>");
	CHECK(Figure(splits, "Sum of best") == "<none>");
}

TEST_CASE("a split passed over is listed as that") {
	const RunModel model = After({
		{ 0, Joined{ "a", 0, 1, 0 } },
		{ 0, BossRun() },
		{ 10 * kSecond, SplitSkipped{ 0 } },
	});
	const std::vector<Row> splits = Top(Runs(model, Shown(NULL, 100 * kSecond)));
	CHECK(splits[0].cells[SplitTime] == "-");
	CHECK(splits[1].cells[SplitName] == "> Duriel");
}

TEST_CASE("the runs that are over are listed latest first, with how each ended") {
	RunModel model = TwoRuns();
	model.Apply({ 1300 * kSecond, RunEnded{ false } });
	const std::vector<Row> over = Earlier(Runs(model, Shown(NULL, 1400 * kSecond)));

	REQUIRE(over.size() == 2);
	CHECK(over[0].cells[SplitName] == "Stopped after 2 of 5");
	CHECK(over[0].cells[SplitTime] == "5:00");
	CHECK(over[1].cells[SplitName] == "Finished");
	CHECK(over[1].cells[SplitTime] == "15:00");
}

TEST_CASE("a run going on another route leaves the route picked as it stands before a run") {
	const RunModel model = TwoRuns();
	const std::vector<Splits::Route> routes = TwoRoutes();
	const std::vector<Row> rows = Runs(model, Shown(NULL, 1300 * kSecond, RunStore::RealTime, &routes[0]));

	// Its own splits, none of them the one a run is on, and none of the other
	// route's runs under them.
	CHECK(Headings(rows).empty());
	const std::vector<Row> splits = Top(rows);
	REQUIRE(splits.size() == 2);
	CHECK(splits[0].cells[SplitName] == "Andariel");
	CHECK(Figure(splits, "Run time") == "<none>");
}

TEST_CASE("the times are on the clock asked for") {
	const RunModel model = After({
		{ 0, Joined{ "a", 0, 1, 0 } },
		{ 0, BossRun() },
		{ 60 * kSecond, Left() },
		{ 120 * kSecond, Joined{ "b", 0, 5, 0 } },
		{ 150 * kSecond, SplitReached{ 0, false } },
	});

	CHECK(Top(Runs(model, Shown(NULL, 160 * kSecond, RunStore::RealTime)))[0].cells[SplitTime] == "2:30");
	CHECK(Top(Runs(model, Shown(NULL, 160 * kSecond, RunStore::GameTime)))[0].cells[SplitTime] == "1:30");
}

TEST_CASE("a key is told by how it starts") {
	std::string rest;
	CHECK(ListRows::KeyRest("edit:one", RouteKeys::kEdit, rest));
	CHECK(rest == "one");
	CHECK(ListRows::KeyRest("add:", RouteKeys::kAdd, rest));
	CHECK(rest.empty());
	CHECK_FALSE(ListRows::KeyRest("edit:one", RouteKeys::kAdd, rest));
	CHECK_FALSE(ListRows::KeyRest("", RouteKeys::kAdd, rest));
}

TEST_CASE("the routes are listed by name, the one a run is going on said to be") {
	const std::vector<Row> rows = RouteList(TwoRoutes(), "two");
	REQUIRE(rows.size() == 2);
	CHECK(rows[0].cells[RouteName] == "First");
	CHECK(rows[0].cells[RouteNote] == "");
	CHECK(rows[0].key == "edit:one");
	CHECK(rows[1].cells[RouteName] == "Second");
	CHECK(rows[1].cells[RouteNote] == "live");
	CHECK(rows[1].key == "edit:two");
	CHECK_FALSE(rows[0].heading);

	CHECK(RouteList({}, "two").empty());
}

TEST_CASE("with no run going no route is said to have one") {
	for (const Row& row : RouteList(TwoRoutes(), ""))
		CHECK(row.cells[RouteNote] == "");
}

TEST_CASE("a route's splits are listed in order, each with its place") {
	const std::vector<Splits::Route> routes = TwoRoutes();
	const std::vector<Row> rows = RouteSplits(routes[0]);
	REQUIRE(rows.size() == 2);
	CHECK(rows[0].cells[RouteSplitPlace] == "1");
	CHECK(rows[0].cells[RouteSplitName] == "Andariel");
	CHECK(rows[1].cells[RouteSplitPlace] == "2");
	CHECK(rows[1].cells[RouteSplitName] == "By hand");

	CHECK(RouteSplits(routes[1]).empty());
}

TEST_CASE("every split of the catalogue is there to add, under its difficulty") {
	const std::vector<Row> rows = SplitChoices("");
	CHECK(Choices(rows).size() == RouteEdit::Catalogue().size());
	CHECK(Headings(rows) == std::vector<std::string>{ "Normal", "Nightmare", "Hell" });

	const Row* first = Keyed(rows, "add:0");
	REQUIRE(first != NULL);
	CHECK(first->cells[ChoiceName] == "Andariel");
	CHECK(first->cells[ChoiceMarkedBy] == "kill or quest");
}

TEST_CASE("the text narrows what there is to add") {
	CHECK(Choices(SplitChoices("DURIEL")) ==
		std::vector<std::string>{ "Duriel", "Duriel (NM)", "Duriel (Hell)" });

	// By what marks a split as well as by its name.
	const std::vector<Row> cubes = SplitChoices("item box");
	CHECK(Choices(cubes).size() == 3);
	CHECK(Keyed(cubes, "add:level") == NULL);

	// And by the difficulty it is of, which a name only has the short of.
	const std::vector<Row> nightmare = SplitChoices("nightmare");
	CHECK(Headings(nightmare) == std::vector<std::string>{ "Nightmare" });
	CHECK(Choices(nightmare).size() == RouteEdit::Catalogue().size() / 3);

	CHECK(SplitChoices("no such split").empty());
}

TEST_CASE("a level asked for in the text is there to add, over the rest") {
	const std::vector<Row> rows = SplitChoices("level 24");
	REQUIRE(rows.size() == 2);
	CHECK(rows[0].heading);
	CHECK(rows[0].cells[0] == "Any difficulty");
	CHECK(rows[1].key == "add:level");
	CHECK(rows[1].cells[ChoiceName] == "Level 24");
	CHECK(rows[1].cells[ChoiceMarkedBy] == "level 24");
}
