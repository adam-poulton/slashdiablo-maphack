#pragma once
#include <string>
#include <vector>
#include "RunStore.h"
#include "SessionModel.h"
#include "SplitRoute.h"

// What the session window's tabs list, as rows of text worked out from a
// session without the game. A tab lays them out and draws them.
namespace SessionRows {

struct Row {
	std::vector<std::string> cells;
	bool heading;		// names the rows under it
	bool summary;		// says something of the rows around it, and is no one of them

	// A text colour for each cell that has one of its own, as the game numbers
	// them, and -1 for a cell that takes its column's. Shorter than cells, or
	// empty, where the rest take their column's.
	std::vector<int> colors;

	// What the row stands for, where a tab does something when it is clicked.
	std::string key;
};

// Text colours as the game numbers them, for the cells that have one.
const int kColorBehind = 1;		// red
const int kColorAhead = 2;		// green
const int kColorBest = 4;		// gold

// What the overview is told that a session's events do not hold.
struct Level {
	unsigned long long from;	// experience the character's level starts at
	unsigned long long to;		// and the next one, or 0 where there is none
};

// A name and a figure to a row, under headings: the session, its experience,
// and the game being played while there is one.
std::vector<Row> Overview(const SessionModel& model, unsigned long long now,
	const Level& level, const std::string& startedAt);

// The columns of Games(), left to right.
enum GameColumn {
	GameNumber,
	GameName,
	GameLength,
	GameExperience,
	GameRate,
	GameLevel,
	GameColumns
};

// A row to a game under a heading for each kind of run, the run played most
// lately first and its games latest first. A run with two finished games or
// more opens with what they average.
std::vector<Row> Games(const SessionModel& model, unsigned long long now);

// The columns of Kills(), left to right.
enum KillColumn {
	KillName,
	KillTotal,
	KillChampions,
	KillUniques,
	KillTime,
	KillDetail,
	KillColumns
};

// Three headings. The areas fought in, with what died there, how long was
// spent and the experience earned, most kills first. The kinds of monster,
// most killed first. And each unique monster, super unique and act boss, latest
// first, with when it died and where.
std::vector<Row> Kills(const SessionModel& model, unsigned long long now);

// The columns of Drops(), left to right.
enum DropColumn {
	DropTime,
	DropItem,
	DropArea,
	DropGame,
	DropPing,
	DropColumns
};

// A row to each drop the item filter pinged, latest first, the item in the
// colour it was announced in.
std::vector<Row> Drops(const SessionModel& model);

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

// What the Splits tab is told beside the session.
struct RunsShown {
	const std::vector<Splits::Route>* routes;
	std::string selected;					// the id of the route a run would be started on
	const RunStore* store;
	RunStore::Clock clock;
	unsigned long long now;
};

// The key of a route's row, and the route a key is of, or "" for none.
std::string RouteKey(const std::string& routeId);
std::string RouteOf(const std::string& key);

// The run in hand split by split, against the best its route has been run:
// when each was reached, how far ahead or behind, the time to beat, the best
// that stretch has been done, and the level it was reached at. Under it the
// routes there are to run, each with its best, and the runs of the session
// that are over.
std::vector<Row> Runs(const SessionModel& model, const RunsShown& shown);

// The rows that have the text in them, whatever its case, under the headings
// they stood under. A heading that has it keeps every row under it. A summary
// goes: it is of rows that are no longer all there.
std::vector<Row> Matching(const std::vector<Row>& rows, const std::string& text);

// What kind of run a game is, by its name: the name without the number it ends
// in or whatever sets that number off, so that "baal-12", "baal-13" and "baal7"
// are all "baal". A name that is only a number is its own.
std::string RunNameOf(const std::string& game);

}
