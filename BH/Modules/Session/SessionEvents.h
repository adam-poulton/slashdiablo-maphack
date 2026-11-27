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
	std::string name;			// as the game names it, or empty where it was not asked
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

// What kind of monster died, from the least of them to the most.
enum Tier {
	TierNormal,
	TierMinion,			// of a unique monster's pack
	TierChampion,
	TierUnique,
	TierSuperUnique,	// a unique monster the game names and places
	TierBoss,			// an act boss
	TierCount
};

// Monsters of one kind that died in view, in one area, since the last such
// event. Never a unique monster or better: each of those is a Notable.
struct Killed {
	unsigned int area;
	unsigned int classId;		// the monster's row in MonStats.txt
	std::string name;
	Tier tier;
	unsigned int count;
};

// One unique monster, super unique or act boss that died in view.
struct Notable {
	unsigned int area;
	unsigned int classId;
	std::string name;			// its own: "Pindleskin"
	std::string kind;			// and what kind of monster it is: "Defiled Warrior"
	Tier tier;
	int superUnique;			// its row in SuperUniques.txt, or -1
};

// An item on the ground that the item filter pinged.
struct Dropped {
	unsigned int id;			// the item's unit id, which is the game's for that game
	std::string name;			// as the filter named it, without its colours
	std::string code;
	unsigned int quality;
	unsigned int color;			// the text colour it was announced in
	unsigned int ping;			// the ping tier of the rule that pinged it
	unsigned int area;
	std::string areaName;
};

typedef std::variant<Started, Joined, Left, AreaEntered, Experience, LevelReached,
	Died, Players, Killed, Notable, Dropped> Fact;

struct Event {
	unsigned long long at;		// milliseconds since the session began
	Fact fact;
};

// What a tier is called, in the log and wherever it is shown.
const char* NameOf(Tier tier);

// One line of a session's log, without its newline: a JSON object.
std::string ToLine(const Event& event);

// False for a line that is not an event this build knows, which a reader skips:
// a line cut short by the client dying, or a kind of event from a later build.
bool FromLine(const std::string& line, Event& into);

}
