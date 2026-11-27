#pragma once
#include <string>
#include <vector>
#include "RouteEdit.h"
#include "RunStore.h"
#include "SessionHistory.h"
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
const int kColorAction = 3;		// blue: a row that does something when clicked

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

// The rest of a key that starts as given. False where it starts otherwise.
bool KeyRest(const std::string& key, const char* prefix, std::string& rest);

// The columns of History(), left to right.
enum HistoryColumn {
	HistoryStarted,
	HistoryCharacter,
	HistoryLevel,
	HistoryPlayed,
	HistoryGames,
	HistoryExperience,
	HistoryRate,
	HistoryKills,
	HistoryDrops,
	HistoryColumns
};

// What the History tab is told beside the sessions there have been.
struct HistoryShown {
	bool hasCurrent;						// there is a session being played to go back to
	std::string viewing;					// the log being looked at, or "" for that session
	std::string (*started)(long long);		// a moment, as the tab writes one
};

// The key of a session's row, and the log a key is of: "" for the session being
// played. False for a key that is no session's.
std::string SessionKey(const std::string& file);
bool SessionOf(const std::string& key, std::string& file);

// The session being played, where there is one, and under it every session
// there is a log of, as the index has them: when it began, whose it was, and
// what it came to. The one being looked at is marked.
std::vector<Row> History(const std::vector<SessionIndex::Entry>& entries,
	const HistoryShown& shown);

// The columns of Routes(), left to right.
enum RouteColumn {
	RouteName,
	RouteDetail,
	RouteColumns
};

// How the keys of the Routes tab's rows start.
namespace RouteKeys {
	const char* const kEdit = "edit:";	// a route to change, by its id
	const char* const kPick = "pick:";	// a split of that route, by its place
	const char* const kAdd = "add:";	// a split to add: its place in the catalogue, or "level"
	const char* const kDo = "do:";		// new, copy, restore, up, down, remove, use, rename, delete
}

// What the Routes tab is told.
struct RoutesShown {
	const std::vector<Splits::Route>* routes;
	std::string chosen;		// the id of the route runs are timed on
	std::string editing;	// the id of the route being changed
	int split;				// the split of it picked out, or -1
	std::string text;		// what the search box holds
	bool deleting;			// deleting the route has been asked for once, and waits to be asked again
};

// The routes there are, the one being changed split by split with what can be
// done to it, and the splits there are to add to it. The text narrows what
// there is to add, names a level to add ("level 24"), and is what the route can
// be renamed to.
std::vector<Row> Routes(const RoutesShown& shown);

// The rows that have the text in them, whatever its case, under the headings
// they stood under. A heading that has it keeps every row under it. A summary
// goes: it is of rows that are no longer all there.
std::vector<Row> Matching(const std::vector<Row>& rows, const std::string& text);

// What kind of run a game is, by its name: the name without the number it ends
// in or whatever sets that number off, so that "baal-12", "baal-13" and "baal7"
// are all "baal". A name that is only a number is its own.
std::string RunNameOf(const std::string& game);

}
