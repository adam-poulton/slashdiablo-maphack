#include "doctest.h"
#include <string>
#include <vector>
#include "StatDescriptions.h"
#include "StringUtil.h"
#include "TableFixture.h"
#include "TableReader.h"

/*
 * What the stat description module makes of the game's own data tables, stood
 * up from fixtures rather than from a running game.
 *
 * The four items are the ones this rendering has always been checked by hand
 * against, and their lines are asserted as the player reads them rather than as
 * the parts they were assembled from: the line is what the game shows and the
 * parts are this module's own business. Collecting, merging, grouping and
 * ordering are asserted separately because that is where the rules are subtle,
 * and a finished line cannot say which of the four got it wrong.
 */

using StatDescriptions::Stat;

namespace {

// Properties are numbered prop1..prop12 in UniqueItems.txt, each with its
// parameter and its range beside it.
const int kUniqueProperties = 12;

std::vector<Stat> UniqueStats(const std::string& index) {
	TableFixture::Load();
	JSONObject* entry = Tables::UniqueItems.findEntry("index", index);
	REQUIRE_MESSAGE(entry != NULL, "not in the fixture: " << index);

	std::vector<Stat> stats;
	for (int n = 1; n <= kUniqueProperties; n++) {
		std::string suffix = std::to_string(n);
		std::string code = Trim(entry->getString("prop" + suffix));
		if (code.length() == 0)
			continue;
		StatDescriptions::CollectProperty(code,
			Trim(entry->getString("par" + suffix)),
			atoi(entry->getString("min" + suffix).c_str()),
			atoi(entry->getString("max" + suffix).c_str()), stats);
	}
	return stats;
}

std::vector<Stat> Collect(const std::string& code, const std::string& param,
		int min, int max) {
	TableFixture::Load();
	std::vector<Stat> stats;
	StatDescriptions::CollectProperty(code, param, min, max, stats);
	return stats;
}

void CheckLines(const std::string& index,
		const std::vector<std::string>& expected) {
	std::vector<std::string> lines =
		StatDescriptions::BuildLines(UniqueStats(index));
	REQUIRE(lines.size() == expected.size());
	for (unsigned int i = 0; i < expected.size(); i++)
		CHECK(lines[i] == expected[i]);
}

}  // namespace

TEST_CASE("the tables a property is looked up in are held whole") {
	TableFixture::Load();
	// Every row the game's own tables have. Trimming one changes what the
	// wording code can find, so a fixture cut down to the items under test has
	// to fail here rather than pass against a table the game does not have.
	// Regenerating the fixtures from another version of the game moves these,
	// which is a change worth being told about.
	CHECK(Tables::ItemStatCost.size() == 359);
	CHECK(Tables::Properties.size() == 269);
	CHECK(Tables::CharStats.size() == 8);
	CHECK(Tables::Skills.size() == 357);
	CHECK(Tables::SkillDesc.size() == 221);
	// And a stat none of the items under test grants still words itself, which
	// a trimmed table could not do.
	Stat stat;
	stat.stat = "item_fastermovevelocity";
	stat.low = stat.high = 30;
	CHECK(StatDescriptions::Render(stat) == "+30% Faster Run/Walk");
}

TEST_CASE("Harlequin Crest reads as the game words it") {
	CheckLines("Harlequin Crest", {
		"+2 to All Skills",
		"+2 to all Attributes",
		"+1.50 to Life (Based on Character Level)",
		"+1.50 to Mana (Based on Character Level)",
		"Damage Reduced by 10%",
		"50% Better Chance of Getting Magic Items",
	});
}

TEST_CASE("Skin of the Vipermagi reads as the game words it") {
	CheckLines("Skin of the Vipermagi", {
		"+1 to All Skills",
		"+30% Faster Cast Rate",
		"+120% Enhanced Defense",
		"All Resistances +20-35",
		"Magic Damage Reduced by 9-13",
	});
}

TEST_CASE("Mara's Kaleidoscope reads as the game words it") {
	CheckLines("Mara's Kaleidoscope", {
		"+2 to All Skills",
		"+5 to all Attributes",
		"All Resistances +20-30",
	});
}

TEST_CASE("Guardian Angel reads as the game words it") {
	CheckLines("Guardian Angel", {
		"+1 to Paladin Skill Levels",
		"+30% Faster Block Rate",
		"20% Increased Chance of Blocking",
		"+0.62 to Attack Rating against Demons (Based on Character Level)",
		"+180-200% Enhanced Defense",
		"+15% to Maximum Poison Resist",
		"+15% to Maximum Cold Resist",
		"+15% to Maximum Lightning Resist",
		"+15% to Maximum Fire Resist",
		"+4 to Light Radius",
	});
}

TEST_CASE("collecting reads every stat a property grants") {
	std::vector<Stat> stats = Collect("res-all", "", 20, 35);
	REQUIRE(stats.size() == 4);
	CHECK(stats[0].stat == "fireresist");
	CHECK(stats[1].stat == "lightresist");
	CHECK(stats[2].stat == "coldresist");
	CHECK(stats[3].stat == "poisonresist");
	for (unsigned int i = 0; i < stats.size(); i++) {
		CHECK(stats[i].low == 20);
		CHECK(stats[i].high == 35);
	}
}

TEST_CASE("collecting leaves out what the tables give no wording to") {
	// Ethereality belongs to the base item rather than to the property, and
	// shows nothing on the finished item.
	CHECK(Collect("ethereal", "", 1, 1).size() == 0);
	// A code the tables do not have at all is not a line either.
	CHECK(Collect("not-a-property", "", 1, 1).size() == 0);
}

TEST_CASE("collecting reads the properties the game hardcodes") {
	std::vector<Stat> stats = Collect("dmg%", "", 100, 100);
	REQUIRE(stats.size() == 1);
	// Described by a label of its own rather than by a stat, since the game
	// builds this line itself.
	CHECK(stats[0].stat == "");
	CHECK(StatDescriptions::Render(stats[0]) == "+100% Enhanced damage");
}

TEST_CASE("merging adds equal stats together") {
	std::vector<Stat> stats = Collect("str", "", 5, 5);
	StatDescriptions::CollectProperty("str", "", 10, 10, stats);
	REQUIRE(stats.size() == 2);

	StatDescriptions::MergeStats(stats);
	REQUIRE(stats.size() == 1);
	CHECK(stats[0].low == 15);
	CHECK(stats[0].high == 15);
	CHECK(StatDescriptions::Render(stats[0]) == "+15 to Strength");
}

TEST_CASE("merging keeps apart stats that were granted differently") {
	// Two classes' skills are the same stat and different bonuses, so the
	// labels they resolved to are what tells them apart.
	std::vector<Stat> stats = Collect("pal", "", 1, 1);
	StatDescriptions::CollectProperty("sor", "", 1, 1, stats);
	StatDescriptions::MergeStats(stats);
	REQUIRE(stats.size() == 2);
	CHECK(StatDescriptions::Render(stats[0]) == "+1 to Paladin Skill Levels");
	CHECK(StatDescriptions::Render(stats[1]) == "+1 to Sorceress Skill Levels");
}

TEST_CASE("grouping folds a whole group into the line the game shows") {
	std::vector<Stat> stats = Collect("all-stats", "", 5, 5);
	REQUIRE(stats.size() == 4);

	StatDescriptions::GroupStats(stats);
	REQUIRE(stats.size() == 1);
	CHECK(StatDescriptions::Render(stats[0]) == "+5 to all Attributes");
}

TEST_CASE("grouping leaves a part of a group as it stands") {
	std::vector<Stat> stats = Collect("str", "", 5, 5);
	StatDescriptions::CollectProperty("dex", "", 5, 5, stats);
	StatDescriptions::CollectProperty("vit", "", 5, 5, stats);

	StatDescriptions::GroupStats(stats);
	REQUIRE(stats.size() == 3);
	CHECK(StatDescriptions::Render(stats[0]) == "+5 to Strength");
}

TEST_CASE("grouping leaves a group granted unevenly as it stands") {
	// The four resistances, three at twenty and the fourth at thirty.
	std::vector<Stat> stats = Collect("res-fire", "", 20, 20);
	StatDescriptions::CollectProperty("res-ltng", "", 20, 20, stats);
	StatDescriptions::CollectProperty("res-cold", "", 20, 20, stats);
	StatDescriptions::CollectProperty("res-pois", "", 30, 30, stats);

	StatDescriptions::GroupStats(stats);
	CHECK(stats.size() == 4);
}

TEST_CASE("ordering puts the stats in the order an item shows them") {
	// Collected back to front, so only the ordering can put them right.
	std::vector<Stat> stats = Collect("light", "", 4, 4);
	StatDescriptions::CollectProperty("str", "", 5, 5, stats);
	StatDescriptions::CollectProperty("allskills", "", 2, 2, stats);

	StatDescriptions::SortStats(stats);
	REQUIRE(stats.size() == 3);
	CHECK(StatDescriptions::Render(stats[0]) == "+2 to All Skills");
	CHECK(StatDescriptions::Render(stats[1]) == "+5 to Strength");
	CHECK(StatDescriptions::Render(stats[2]) == "+4 to Light Radius");
}

TEST_CASE("ordering keeps equal stats in the order they were collected") {
	// Two classes' skills are one stat and so one priority, which leaves the
	// order they were written in as the only thing that can settle them.
	std::vector<Stat> stats = Collect("sor", "", 1, 1);
	StatDescriptions::CollectProperty("pal", "", 1, 1, stats);

	StatDescriptions::SortStats(stats);
	REQUIRE(stats.size() == 2);
	CHECK(StatDescriptions::Render(stats[0]) == "+1 to Sorceress Skill Levels");
	CHECK(StatDescriptions::Render(stats[1]) == "+1 to Paladin Skill Levels");
}
