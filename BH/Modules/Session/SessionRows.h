#pragma once
#include <string>
#include <vector>
#include "SessionModel.h"

// What the session window's tabs list, as rows of text worked out from a
// session without the game. A tab lays them out and draws them.
namespace SessionRows {

struct Row {
	std::vector<std::string> cells;
	bool heading;		// names the rows under it
	bool summary;		// says something of the rows around it, and is no one of them
};

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

// What kind of run a game is, by its name: the name without the number it ends
// in or whatever sets that number off, so that "baal-12", "baal-13" and "baal7"
// are all "baal". A name that is only a number is its own.
std::string RunNameOf(const std::string& game);

}
