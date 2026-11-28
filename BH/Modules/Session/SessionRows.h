#pragma once
#include <string>
#include <vector>
#include "../Tracking/ListRows.h"
#include "SessionHistory.h"
#include "SessionModel.h"

// What the session window's tabs list, as rows of text worked out from a
// session without the game. A tab lays them out and draws them.
namespace SessionRows {

using ListRows::Row;
using ListRows::KeyRest;
using ListRows::Matching;

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

// What kind of run a game is, by its name: the name without the number it ends
// in or whatever sets that number off, so that "baal-12", "baal-13" and "baal7"
// are all "baal". A name that is only a number is its own.
std::string RunNameOf(const std::string& game);

}
