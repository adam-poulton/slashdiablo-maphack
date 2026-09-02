#include "doctest.h"
#include <string>
#include <vector>
#include "Catalogue/RecipeCatalogue.h"
#include "ItemRarity.h"
#include "TableFixture.h"
#include "TableReader.h"

/*
 * What the recipes catalogue makes of CubeMain.txt, stood up from fixtures with
 * no game running.
 *
 * Almost all of what a recipe costs to read is wording, so that is what is
 * asserted: the phrase a player reads for what goes in, the name worked out for
 * what comes out, and the notes saying what the recipe does beyond making it.
 * Each is asserted whole, as the panel shows it, since the sentence is the
 * point of reading the table at all.
 *
 * The realm tables are hand written rather than the game's: they carry the rows
 * a shipped table does not, which is a recipe the game has never heard of, a
 * divider, a difficulty the shipped table restricts nothing by, a second output,
 * and a table whose enabled column has been dropped altogether.
 */

using Catalogue::Source;

namespace {

const char* kRealmRecipes = "BHTests/fixtures/tables/RealmCubeMain.txt";
const char* kUnflaggedRecipes = "BHTests/fixtures/tables/UnflaggedCubeMain.txt";

const Source& Resolve(const std::string& code) {
	TableFixture::Load();
	const Source* source = RecipeCatalogue::Find(code);
	REQUIRE_MESSAGE(source != NULL, "not in the fixture: " << code);
	return *source;
}

std::vector<std::string> Names(const std::vector<Source>& sources) {
	std::vector<std::string> names;
	for (unsigned int i = 0; i < sources.size(); i++)
		names.push_back(sources[i].name);
	return names;
}

std::vector<std::string> Headings(const std::vector<Source>& sources) {
	std::vector<std::string> headings;
	for (unsigned int i = 0; i < sources.size(); i++)
		headings.push_back(sources[i].heading);
	return headings;
}

std::vector<Source> ReadTable(const char* path, int rows) {
	TableFixture::Load();
	Table table(path);
	REQUIRE_MESSAGE(table.size() == rows,
		"fixture not found, run the tests from the repository root: " << path);
	return RecipeCatalogue::Read(table, Tables::MagicPrefix, Tables::MagicSuffix);
}

}  // namespace

TEST_CASE("what a recipe takes reads as a phrase") {
	TableFixture::Load();

	SUBCASE("how many are wanted, where more than one is") {
		CHECK(Resolve("3 flawless rubies -> perfect ruby").ingredients ==
			"3 Flawless Ruby");
	}

	SUBCASE("the state an input has to be in, and its quality") {
		CHECK(Resolve("r08 + r10 + 1 perfect sapphire + normal helm -> socketed helm")
			.ingredients ==
			"Unsocketed Normal Helm + Ral Rune + Thul Rune + Perfect Sapphire");
	}

	SUBCASE("an item type stands for every item of that type") {
		CHECK(Resolve("r09 + weapon -> repair").ingredients ==
			"Non-Ethereal Weapon + Ort Rune");
	}

	SUBCASE("the cube's own wildcard reads as an item") {
		CHECK(Resolve("r15 + tsc + any socketed item -> unsocket (destroy gems)")
			.ingredients == "Socketed Item + Hel Rune + Scroll of Town Portal");
	}
}

TEST_CASE("what a recipe makes is named from the row") {
	TableFixture::Load();

	SUBCASE("an output naming an item of its own") {
		CHECK(Resolve("3 flawless rubies -> perfect ruby").name == "Perfect Ruby");
	}

	SUBCASE("an output naming the first input takes its quality") {
		CHECK(Resolve("6 perfect skulls + 1 rare item -> 1 low level rare item")
			.name == "Rare Item");
	}

	SUBCASE("a forced prefix names the result instead of its quality") {
		CHECK(Resolve("1 perfect gem of each type + 1 amulet -> prismatic amulet")
			.name == "Prismatic Amulet");
	}

	SUBCASE("a crafted result is named by its family") {
		CHECK(Resolve("magic full helm + jewel + rune 06 + perfect sapphire -> hitpower helm")
			.name == "Hit Power Full Helm");
	}

	SUBCASE("the state the result comes out in reads in front of its name") {
		CHECK(Resolve("r08 + r10 + 1 perfect sapphire + normal helm -> socketed helm")
			.name == "Socketed Normal Helm");
		CHECK(Resolve("r09 + weapon -> repair").name == "Repaired Weapon");
		CHECK(Resolve("r15 + tsc + any socketed item -> unsocket (destroy gems)")
			.name == "Unsocketed Item");
	}

	SUBCASE("the tier an upgrade raises the item to") {
		CHECK(Resolve("r07 + r13 + perfect diamond + basic unique armor -> exceptional unique armor")
			.name == "Exceptional Unique Any Armor");
	}

	SUBCASE("a recipe that opens a portal is named for where it leads") {
		CHECK(Resolve("Pandemonium Finale key").name == "To Tristram");
	}
}

TEST_CASE("the result is drawn in the colour the game gives it") {
	TableFixture::Load();

	CHECK(NameColor(Resolve("1 perfect gem of each type + 1 amulet -> prismatic amulet")
		.rarity) == Blue);
	CHECK(NameColor(Resolve("6 perfect skulls + 1 rare item -> 1 low level rare item")
		.rarity) == Yellow);
	CHECK(NameColor(Resolve("r07 + r13 + perfect diamond + basic unique armor -> exceptional unique armor")
		.rarity) == Gold);
	CHECK(NameColor(Resolve("magic full helm + jewel + rune 06 + perfect sapphire -> hitpower helm")
		.rarity) == Orange);

	// A rune carries no quality, but the game gives its name a colour anyway.
	CHECK(NameColor(Resolve("3 rune 14 + 1 chipped emerald -> rune 15").rarity) == Orange);

	// Gold wherever the game gives the name no colour of its own, which a plain
	// item and every quality the game draws plain both come to.
	CHECK(NameColor(Resolve("3 flawless rubies -> perfect ruby").rarity) == Gold);
	CHECK(NameColor(Resolve("r08 + r10 + 1 perfect sapphire + normal helm -> socketed helm")
		.rarity) == Gold);
}

TEST_CASE("the bonuses the result is given read as stat lines") {
	TableFixture::Load();

	SUBCASE("a forced prefix grants what its own row grants") {
		const Source& amulet =
			Resolve("1 perfect gem of each type + 1 amulet -> prismatic amulet");
		CHECK(amulet.lines ==
			std::vector<std::string>({ "All Resistances +16-20" }));
		// The raw entries are kept beside the worded line, since four
		// resistances word themselves into one.
		REQUIRE(amulet.properties.size() == 1);
		CHECK(amulet.properties[0].code == "res-all");
		CHECK(amulet.properties[0].min == 16);
		CHECK(amulet.properties[0].max == 20);
	}

	SUBCASE("bonuses given as ready made text follow the described ones") {
		const Source& helm = Resolve(
			"magic full helm + jewel + rune 06 + perfect sapphire -> hitpower helm");
		REQUIRE(helm.lines.size() >= 1);
		CHECK(helm.lines[helm.lines.size() - 1] == "+1-4 Random Affixes");
	}

	SUBCASE("a recipe granting nothing carries no lines") {
		CHECK(Resolve("3 flawless rubies -> perfect ruby").lines.empty());
	}
}

TEST_CASE("what a recipe does beyond making the result is kept") {
	TableFixture::Load();

	SUBCASE("the sockets it adds") {
		CHECK(Resolve("r08 + r10 + 1 perfect sapphire + normal helm -> socketed helm")
			.notes == std::vector<std::string>({ "Socketed (1 to 6)" }));
	}

	SUBCASE("the levels it costs") {
		CHECK(Resolve("r07 + r13 + perfect diamond + basic unique armor -> exceptional unique armor")
			.notes == std::vector<std::string>({ "Required Level: +5" }));
	}

	SUBCASE("the level the result is made at") {
		CHECK(Resolve("1 perfect gem of each type + 1 amulet -> prismatic amulet")
			.notes == std::vector<std::string>({ "Craft ilvl: 50" }));
		CHECK(Resolve("6 perfect skulls + 1 rare item -> 1 low level rare item")
			.notes == std::vector<std::string>({ "Craft ilvl: 40% clvl + 40% ilvl" }));
	}

	SUBCASE("the conditions it is only allowed under") {
		CHECK(Resolve("3 rune 14 + 1 chipped emerald -> rune 15").notes ==
			std::vector<std::string>({ "Ladder only" }));
	}

	SUBCASE("what it destroys") {
		CHECK(Resolve("r15 + tsc + any socketed item -> unsocket (destroy gems)")
			.notes ==
			std::vector<std::string>({ "The socketted items are destroyed" }));
	}

	SUBCASE("the bases an input is allowed at any tier reaches") {
		CHECK(Resolve("magic full helm + jewel + rune 06 + perfect sapphire -> hitpower helm")
			.notes == std::vector<std::string>({
				"Alt bases: Basinet, Giant Conch",
				"Craft ilvl: 50% clvl + 50% ilvl",
				"Always 4 affixes at craft ilvl 71",
			}));
	}
}

TEST_CASE("every recipe is ordered for a list under its heading") {
	TableFixture::Load();

	// The order CubeMain.txt gives them, with the headings gathered up: the two
	// quest recipes the file reaches at either end of it are next to each other,
	// and so are the two that make sockets.
	CHECK(Names(RecipeCatalogue::Sources()) == std::vector<std::string>({
		"Horadric Staff",
		"To Tristram",
		"Prismatic Amulet",
		"Perfect Ruby",
		"Rare Item",
		"Hit Power Full Helm",
		"Hel Rune",
		"Socketed Normal Helm",
		"Unsocketed Item",
		"Exceptional Unique Any Armor",
		"Repaired Weapon",
	}));

	// A heading read from what the recipe makes where that says the most, and
	// from what it does where that says more.
	CHECK(Headings(RecipeCatalogue::Sources()) == std::vector<std::string>({
		"Quest",
		"Quest",
		"Amulet",
		"Gem",
		"Rerolling",
		"Hit Power",
		"Rune",
		"Sockets",
		"Sockets",
		"Upgrading",
		"Repairing",
	}));
}

TEST_CASE("the catalogue is built once and kept") {
	TableFixture::Load();
	const std::vector<Source>& first = RecipeCatalogue::Sources();
	const std::vector<Source>& again = RecipeCatalogue::Sources();
	CHECK(&first == &again);
	// A lookup answers with the source the list holds rather than a copy of it.
	CHECK(RecipeCatalogue::Find("3 flawless rubies -> perfect ruby") == &first[3]);
	CHECK(RecipeCatalogue::Loaded());
}

TEST_CASE("a row the file has not enabled is left out") {
	TableFixture::Load();
	CHECK(RecipeCatalogue::Find("r08 + jew + superior weapon -> tempered weapon") == NULL);
	CHECK(RecipeCatalogue::Find("") == NULL);
}

TEST_CASE("a table a realm has modified is read the same way") {
	std::vector<Source> sources = ReadTable(kRealmRecipes, 3);

	SUBCASE("a divider row and an unfinished row are left out") {
		REQUIRE(sources.size() == 1);
		CHECK(sources[0].code == "3 perfect rubies + 1 amulet -> realm amulet");
	}

	SUBCASE("a recipe the game does not ship is read like any other") {
		REQUIRE(sources.size() == 1);
		const Source& amulet = sources[0];
		CHECK(amulet.ingredients == "3 Perfect Ruby + Amulet");
		CHECK(amulet.name == "Magic Amulet");
		CHECK(amulet.heading == "Amulet");
		CHECK(amulet.rarity == RarityMagic);
		CHECK(amulet.lines == std::vector<std::string>({ "+25 to Strength" }));
	}

	SUBCASE("a second output and a difficulty are said in notes") {
		REQUIRE(sources.size() == 1);
		CHECK(sources[0].notes == std::vector<std::string>({
			"Also makes Perfect Ruby",
			"Nightmare or Hell only",
		}));
	}
}

TEST_CASE("a table with nothing flagged enabled falls back to every row") {
	std::vector<Source> sources = ReadTable(kUnflaggedRecipes, 2);
	REQUIRE(sources.size() == 1);
	CHECK(sources[0].code == "3 flawless rubies -> realm ruby");
	CHECK(sources[0].name == "Perfect Ruby");
	CHECK(sources[0].ingredients == "3 Flawless Ruby");
}
