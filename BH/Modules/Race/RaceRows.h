#pragma once
#include <string>
#include <vector>
#include "../Tracking/ListRows.h"
#include "RouteEdit.h"
#include "RunModel.h"
#include "RunStore.h"
#include "SplitRoute.h"

// What the race window lists, as rows of text worked out without the game. The
// window lays them out and draws them.
namespace RaceRows {

using ListRows::Row;

// Text colours as the game numbers them, for the cells that have one.
const int kColorBehind = 1;		// red
const int kColorAhead = 2;		// green
const int kColorBest = 4;		// gold

// The columns of Runs(), left to right.
enum SplitColumn {
	SplitName,
	SplitTime,
	SplitDelta,
	SplitBest,
	SplitSegment,
	SplitLevel,
	SplitColumns
};

// What a route's runs are listed for.
struct RunsShown {
	const Splits::Route* route;		// the route picked, or NULL for none
	const RunStore* store;
	RunStore::Clock clock;
	unsigned long long now;

	// Whose runs they are: a route's bests are kept for each class, and apart
	// for hardcore.
	unsigned int classId;
	bool hardcore;
};

// What the runs that are over are listed under.
const char* const kEarlierRuns = "Earlier runs";

// The route picked, split by split. While a run is going on it, the run against
// the best the route has been run: when each split was reached, how far ahead
// or behind, the time to beat, the best that stretch has been done, and the
// level it was reached at. Otherwise the route with its times to beat. Under
// either, the runs on it that are over, latest first.
std::vector<Row> Runs(const RunModel& model, const RunsShown& shown);

// The routes to pick from, and what a route is changed with: its splits, and
// the splits there are to add to it.

// How the keys of their rows start.
namespace RouteKeys {
	const char* const kEdit = "edit:";	// a route, by its id
	const char* const kAdd = "add:";	// a split to add: its place in the catalogue, or "level"
}

// The columns of RouteList(), left to right.
enum RouteColumn {
	RouteName,
	RouteNote,
	RouteColumns
};

// What is said after the route a run is going on.
const char* const kRunningNote = "live";

// A row to a route, the one a run is going on said to be: `running` is its id,
// or "" for none.
std::vector<Row> RouteList(const std::vector<Splits::Route>& routes, const std::string& running);

// The columns of RouteSplits(), left to right.
enum RouteSplitColumn {
	RouteSplitPlace,
	RouteSplitName,
	RouteSplitColumns
};

// A row to each split of a route, in the order they are run in. A row's place
// in the rows is its split's place in the route.
std::vector<Row> RouteSplits(const Splits::Route& route);

// The columns of SplitChoices(), left to right.
enum ChoiceColumn {
	ChoiceName,
	ChoiceMarkedBy,
	ChoiceColumns
};

// The splits there are to add to a route, each with what marks it, under the
// difficulty it is of. The text narrows them to the ones that have it in their
// name, their difficulty or what marks them, and puts a level it asks for
// ("level 24") at the top.
std::vector<Row> SplitChoices(const std::string& text);

}
