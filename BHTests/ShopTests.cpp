#include "doctest.h"
#include <string>
#include <vector>
#include "Modules/Item/ItemFilter.h"
#include "Modules/Shop/GambleMarks.h"
#include "Modules/Shop/ShopHighlights.h"

/*
 * What a trade window is made to say about its items: which bases a gamble
 * window's marks light up, how the marks are read and written as a setting,
 * which colour a shop item the filter marks is drawn in, and the chat line
 * naming what is on offer.
 */

using GambleMarks::BaseUpgrades;
using GambleMarks::TierFamilies;

namespace {

// Rows as Armor.txt has them, with a column naming the row itself given as
// empty. Circlet and Coronet are both normal bases sharing one exceptional and
// one elite, which is what makes them one family.
TierFamilies Headgear() {
	std::vector<BaseUpgrades> rows = {
		{ "cap", "xap", "uap" },
		{ "xap", "", "uap" },
		{ "uap", "", "" },
		{ "ci0", "ci2", "ci3" },
		{ "ci1", "ci2", "ci3" },
		{ "ci2", "", "ci3" },
		{ "ci3", "", "" },
		{ "rin", "", "" },
	};
	return GambleMarks::FamiliesOf(rows);
}

std::vector<std::string> Codes(std::initializer_list<const char*> codes) {
	return std::vector<std::string>(codes.begin(), codes.end());
}

}  // namespace

TEST_CASE("a base's family is all three of its tiers, whichever tier is asked") {
	TierFamilies families = Headgear();
	CHECK(families["cap"] == Codes({ "cap", "uap", "xap" }));
	CHECK(families["xap"] == Codes({ "cap", "uap", "xap" }));
	CHECK(families["uap"] == Codes({ "cap", "uap", "xap" }));
}

TEST_CASE("normal bases sharing an upgrade are one family") {
	TierFamilies families = Headgear();
	CHECK(families["ci0"] == Codes({ "ci0", "ci1", "ci2", "ci3" }));
	CHECK(families["ci3"] == Codes({ "ci0", "ci1", "ci2", "ci3" }));
}

TEST_CASE("a base with no upgrades is a family of its own") {
	CHECK(Headgear()["rin"] == Codes({ "rin" }));
}

TEST_CASE("a mark matches only its own base without tiers") {
	std::set<std::string> lit = GambleMarks::Expand(Codes({ "ci0" }), false, Headgear());
	CHECK(lit.count("ci0") == 1);
	CHECK(lit.count("ci1") == 0);
	CHECK(lit.count("ci3") == 0);
}

TEST_CASE("a mark matches every tier of its base with tiers") {
	std::set<std::string> lit = GambleMarks::Expand(Codes({ "ci0" }), true, Headgear());
	CHECK(lit == std::set<std::string>({ "ci0", "ci1", "ci2", "ci3" }));
	CHECK(lit.count("cap") == 0);
}

TEST_CASE("a code the tables do not have is kept and matches only itself") {
	std::set<std::string> lit = GambleMarks::Expand(Codes({ "zzz" }), true, Headgear());
	CHECK(lit == std::set<std::string>({ "zzz" }));
}

TEST_CASE("the marks text is split on commas and spaces, lowercased and deduplicated") {
	CHECK(GambleMarks::Parse(" CI0 ,cap,, rin  ci0,Cap ") == Codes({ "ci0", "cap", "rin" }));
}

TEST_CASE("empty marks text is no marks") {
	CHECK(GambleMarks::Parse("").empty());
	CHECK(GambleMarks::Parse(" , ,").empty());
}

TEST_CASE("unknown codes survive being read and written") {
	std::vector<std::string> marks = GambleMarks::Parse("ci0, nonsense");
	CHECK(marks == Codes({ "ci0", "nonsense" }));
	CHECK(GambleMarks::Format(marks) == "ci0, nonsense");
}

TEST_CASE("marks are written comma separated and read back the same") {
	std::vector<std::string> marks = Codes({ "ci0", "cap", "rin" });
	CHECK(GambleMarks::Format(marks) == "ci0, cap, rin");
	CHECK(GambleMarks::Parse(GambleMarks::Format(marks)) == marks);
	CHECK(GambleMarks::Format({}) == "");
}

TEST_CASE("toggling marks a base and toggling it again unmarks it") {
	std::vector<std::string> marks;
	GambleMarks::Toggle(marks, "ci0", false, Headgear());
	CHECK(marks == Codes({ "ci0" }));
	GambleMarks::Toggle(marks, "ci0", false, Headgear());
	CHECK(marks.empty());
}

TEST_CASE("toggling an item lit by another tier's mark takes the family off") {
	std::vector<std::string> marks = Codes({ "ci0", "cap" });
	GambleMarks::Toggle(marks, "ci3", true, Headgear());
	CHECK(marks == Codes({ "cap" }));
}

TEST_CASE("without tiers, toggling another tier marks it alongside") {
	std::vector<std::string> marks = Codes({ "ci0" });
	GambleMarks::Toggle(marks, "ci3", false, Headgear());
	CHECK(marks == Codes({ "ci0", "ci3" }));
}

namespace {

Action MapAction(int border, int onMap) {
	Action action;
	action.borderColor = border;
	action.colorOnMap = onMap;
	return action;
}

}  // namespace

TEST_CASE("the backdrop takes the first map action's border colour") {
	Action first = MapAction(0x0A, 0x20);
	Action second = MapAction(0x97, 0x97);
	std::vector<const Action*> actions = { &first, &second };
	CHECK(ShopHighlights::BackdropColor(actions, ShopHighlights::ColorFromRule, 0x84) == 0x0A);
}

TEST_CASE("the backdrop falls back to the map colour, then to the fixed colour") {
	Action mapOnly = MapAction(UNDEFINED_COLOR, 0x20);
	CHECK(ShopHighlights::BackdropColor({ &mapOnly }, ShopHighlights::ColorFromRule, 0x84) == 0x20);

	Action neither = MapAction(UNDEFINED_COLOR, UNDEFINED_COLOR);
	CHECK(ShopHighlights::BackdropColor({ &neither }, ShopHighlights::ColorFromRule, 0x84) == 0x84);

	Action dead = MapAction(DEAD_COLOR, DEAD_COLOR);
	CHECK(ShopHighlights::BackdropColor({ &dead }, ShopHighlights::ColorFromRule, 0x84) == 0x84);
}

TEST_CASE("the fixed colour wins when the setting asks for it") {
	Action rule = MapAction(0x0A, 0x20);
	CHECK(ShopHighlights::BackdropColor({ &rule }, ShopHighlights::ColorFixed, 0x84) == 0x84);
}

TEST_CASE("the announcement names every match up to five") {
	CHECK(ShopHighlights::AnnounceLine("Charsi", "item for you", "items for you",
		{ "Shako", "Grand Charm", "Grand Charm" }) ==
		"Charsi has 3 items for you: Shako, Grand Charm, Grand Charm");
	CHECK(ShopHighlights::AnnounceLine("Charsi", "item for you", "items for you",
		{ "Shako" }) == "Charsi has 1 item for you: Shako");
	CHECK(ShopHighlights::AnnounceLine("Gheed", "marked item", "marked items",
		{ "a", "b", "c", "d", "e" }) == "Gheed has 5 marked items: a, b, c, d, e");
}

TEST_CASE("the announcement counts the matches past five") {
	CHECK(ShopHighlights::AnnounceLine("Gheed", "marked item", "marked items",
		{ "a", "b", "c", "d", "e", "f", "g" }) ==
		"Gheed has 7 marked items: a, b, c, d, e and 2 more");
}
