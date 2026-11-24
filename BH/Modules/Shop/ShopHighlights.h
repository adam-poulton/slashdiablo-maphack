#pragma once
#include <string>
#include <vector>

struct Action;

// What a trade or gamble window is made to say about the items on offer. Pure,
// so it can be tested with no client running.
namespace ShopHighlights {

// Where the backdrop of an item the rules mark on the map takes its colour from.
enum ColorSource {
	ColorFromRule,
	ColorFixed,
};

// The palette colour for an item's backdrop: from the rule, the first map
// action's border colour, else its map colour; and the fixed colour where the
// setting asks for it or the rule names neither.
unsigned int BackdropColor(const std::vector<const Action*>& mapActions,
	ColorSource source, unsigned int fixed);

// How many names an announcement lists before it says how many more there are.
const unsigned int ANNOUNCE_NAMES = 5;

// "Charsi has 3 items for you: Shako, Grand Charm, Grand Charm", with the noun
// given in the singular and the plural so the count reads right.
std::string AnnounceLine(const std::string& npc, const std::string& singular,
	const std::string& plural, const std::vector<std::string>& names);

}
