#pragma once
#include <set>
#include <string>
#include <vector>

// What a run is timed against: a route is a list of splits, and a split is a
// name and what in the game marks it reached. Routes are data, read from a file
// the player can edit, with the ones BH comes with written to it the first time.
namespace Splits {

// One thing in the game that can mark a split.
struct Trigger {
	enum Kind {
		Boss,			// a monster of this kind died: its row in MonStats.txt
		SuperUnique,	// this super unique died: its row in SuperUniques.txt
		Quest,			// any of these bits of this quest's state is set
		Area,			// this area was entered: its number in Levels.txt
		Level,			// the character is this level or above
		Item,			// an item with this code came into the character's keeping
		AllQuests		// every quest of the difficulty is complete
	};

	Kind kind;
	unsigned int value;
	unsigned int mask;	// Quest
	std::string code;	// Item
};

struct Split {
	std::string name;

	// The difficulty its triggers have to happen in, 0 to 2, or -1 for any.
	int difficulty;

	// Whether every trigger has to be met, where otherwise any one is enough:
	// a boss seen to die, or the quest for killing it done, whichever comes.
	bool all;

	std::vector<Trigger> when;
};

struct Route {
	std::string id;		// what it is saved and asked for by
	std::string name;
	std::vector<Split> splits;
};

// The quest bits that say a quest is done: its reward given, or waiting to be.
const unsigned int kQuestDone = 0x0003;

// The quests of a difficulty, by their place in the game's quest state.
const std::vector<unsigned int>& QuestIds();

// The routes BH comes with.
std::vector<Route> Presets();

// Routes as the text of their file, and back. Reading refuses text that is not
// routes and says where it stopped being; a route or a split it can make
// nothing of is left out and the rest are read.
std::string Write(const std::vector<Route>& routes);
bool Read(const std::string& text, std::vector<Route>& routes, std::string* error);

// The route asked for by its id or its name, whatever the case, or NULL.
const Route* Find(const std::vector<Route>& routes, const std::string& asked);

// Every item code a route's triggers name, which is what has to be watched for.
std::set<std::string> ItemCodes(const std::vector<Route>& routes);

}
