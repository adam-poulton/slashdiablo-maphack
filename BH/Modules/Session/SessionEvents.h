#pragma once
#include <string>
#include <variant>

// What the session tracker was told, one fact at a time and in the order it was
// told them. Every figure the tracker shows is worked out from these and nothing
// else, so a session's log is the session: written a line per event as it
// happens, it can be read back into the same totals.
namespace SessionEvents {

// The first event of every session: whose it is, and what the wall clock said
// when the session's own clock read zero.
struct Started {
	long long startedAt;		// seconds since the Unix epoch
	std::string account;
	std::string realm;
	std::string character;
	unsigned int classId;
	bool hardcore;
	bool expansion;
	std::string version;		// of BH
};

// The level and experience are what the character came into the game with.
// Whatever it gained or lost while out of the game is no part of the session.
struct Joined {
	std::string game;			// empty in single player
	unsigned int difficulty;	// 0 Normal, 1 Nightmare, 2 Hell
	unsigned int level;
	unsigned long long experience;
};

struct Left {};

struct AreaEntered {
	unsigned int area;			// the level's number in Levels.txt
	bool town;
};

// The character's total, which a death in Nightmare or Hell takes down as well
// as a kill takes up.
struct Experience {
	unsigned long long total;
};

struct LevelReached {
	unsigned int level;
};

struct Died {};

// How many players are in the game.
struct Players {
	unsigned int count;
};

typedef std::variant<Started, Joined, Left, AreaEntered, Experience, LevelReached,
	Died, Players> Fact;

struct Event {
	unsigned long long at;		// milliseconds since the session began
	Fact fact;
};

// One line of a session's log, without its newline: a JSON object.
std::string ToLine(const Event& event);

// False for a line that is not an event this build knows, which a reader skips:
// a line cut short by the client dying, or a kind of event from a later build.
bool FromLine(const std::string& line, Event& into);

}
