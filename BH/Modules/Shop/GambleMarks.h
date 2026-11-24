#pragma once
#include <map>
#include <set>
#include <string>
#include <vector>

// The bases a player has asked to watch for in a gamble window, said by their
// item codes. Nothing here touches the game, so the marks can be read, written
// and matched with no client running.
namespace GambleMarks {

// A base as the item tables describe its upgrades: its own code, and the codes
// its exceptional and elite columns name. A column naming the base itself is
// given as empty, since that is the tier the base already is.
struct BaseUpgrades {
	std::string code;
	std::string exceptional;
	std::string elite;
};

// Every code mapped to the codes of all three tiers of its base, itself
// included and in sorted order. A base the tables give no upgrades is a family
// of its own.
typedef std::map<std::string, std::vector<std::string>> TierFamilies;
TierFamilies FamiliesOf(const std::vector<BaseUpgrades>& bases);

// The codes in a marks setting: split on commas and whitespace, lowercased,
// with repeats dropped, in the order first written. Whether a code names a base
// the game has is not asked, so a code it does not have is kept and simply
// never matches anything.
std::vector<std::string> Parse(const std::string& text);

// The setting a list of marks is written back as.
std::string Format(const std::vector<std::string>& marks);

// Marks the code, or unmarks it where it is already marked. With tiers, a code
// counts as marked when any tier of its base is, and unmarking it takes the
// whole family off, so one press always undoes what one press lit up.
void Toggle(std::vector<std::string>& marks, const std::string& code,
	bool tiers, const TierFamilies& families);

// Every code an item can have and be lit up by these marks.
std::set<std::string> Expand(const std::vector<std::string>& marks, bool tiers,
	const TierFamilies& families);

}
