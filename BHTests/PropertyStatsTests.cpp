#include "doctest.h"
#include <string>
#include <vector>
#include "PropertyStats.h"
#include "StatDescriptions.h"
#include "StringUtil.h"
#include "TableFixture.h"
#include "TableReader.h"

/*
 * The one path from a list of properties to the lines a player reads and to the
 * totals those properties come to.
 *
 * The lines are asserted whole, as the player reads them, because the line is
 * what the game shows. The totals are asserted as the numbers they are, since
 * nothing reading them cares how they were worded.
 */

using PropertyStats::Property;

namespace {

// Properties are numbered prop1..prop12 in UniqueItems.txt, each with its
// parameter and its range beside it.
const int kUniqueProperties = 12;

std::vector<Property> UniqueProperties(const std::string& index) {
	TableFixture::Load();
	JSONObject* entry = Tables::UniqueItems.findEntry("index", index);
	REQUIRE_MESSAGE(entry != NULL, "not in the fixture: " << index);

	std::vector<Property> properties;
	for (int n = 1; n <= kUniqueProperties; n++) {
		std::string suffix = std::to_string(n);
		Property property;
		property.code = Trim(entry->getString("prop" + suffix));
		if (property.code.length() == 0)
			continue;
		property.param = Trim(entry->getString("par" + suffix));
		property.min = atoi(entry->getString("min" + suffix).c_str());
		property.max = atoi(entry->getString("max" + suffix).c_str());
		properties.push_back(property);
	}
	return properties;
}

Property Make(const std::string& code, int min, int max, int itemCount) {
	TableFixture::Load();
	Property property;
	property.code = code;
	property.min = min;
	property.max = max;
	property.itemCount = itemCount;
	return property;
}

}  // namespace

TEST_CASE("a property list reads as the game words it") {
	std::vector<std::string> lines =
		PropertyStats::Lines(UniqueProperties("Skin of the Vipermagi"));
	CHECK(lines == std::vector<std::string>({
		"+1 to All Skills",
		"+30% Faster Cast Rate",
		"+120% Enhanced Defense",
		"All Resistances +20-35",
		"Magic Damage Reduced by 9-13",
	}));
}

TEST_CASE("a property list adds up to the totals it grants") {
	std::vector<StatDescriptions::StatTotal> totals =
		PropertyStats::Totals(UniqueProperties("Mara's Kaleidoscope"));

	int low = 0, high = 0;
	StatDescriptions::TotalFor(totals, "fireresist", low, high);
	CHECK(low == 20);
	CHECK(high == 30);
	StatDescriptions::TotalFor(totals, "strength", low, high);
	CHECK(low == 5);
	CHECK(high == 5);
	// A stat nothing on the amulet grants comes back as nothing.
	StatDescriptions::TotalFor(totals, "armorclass", low, high);
	CHECK(low == 0);
	CHECK(high == 0);
}

TEST_CASE("bonuses given as ready made text follow the worded ones") {
	std::vector<Property> properties;
	properties.push_back(Make("str", 5, 5, 0));

	std::vector<std::string> lines = PropertyStats::Lines(properties,
		{ "0.3% Deadly Strike (Based on Character Level)" });
	CHECK(lines == std::vector<std::string>({
		"+5 to Strength",
		"0.3% Deadly Strike (Based on Character Level)",
	}));
}

TEST_CASE("a count at a time tags each line with the pieces it asks for") {
	std::vector<Property> properties;
	properties.push_back(Make("str", 10, 10, 2));
	properties.push_back(Make("dex", 10, 10, 3));

	CHECK(PropertyStats::CountedLines(properties) == std::vector<std::string>({
		"+10 to Strength (2 Items)",
		"+10 to Dexterity (3 Items)",
	}));
}

TEST_CASE("a count at a time adds up only within its own count") {
	// Both grant strength, so one pass over the two would say +30 once.
	std::vector<Property> properties;
	properties.push_back(Make("str", 20, 20, 4));
	properties.push_back(Make("str", 10, 10, 2));

	CHECK(PropertyStats::CountedLines(properties) == std::vector<std::string>({
		"+10 to Strength (2 Items)",
		"+20 to Strength (4 Items)",
	}));
}

TEST_CASE("a count at a time leaves out what no count unlocks") {
	std::vector<Property> properties;
	properties.push_back(Make("str", 10, 10, 0));
	CHECK(PropertyStats::CountedLines(properties).empty());
}
