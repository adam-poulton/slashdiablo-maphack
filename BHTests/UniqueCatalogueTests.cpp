#include "doctest.h"
#include <string>
#include <vector>
#include "Catalogue/UniqueCatalogue.h"
#include "ItemDescription.h"
#include "TableFixture.h"
#include "TableReader.h"

/*
 * What the uniques catalogue makes of UniqueItems.txt, stood up from fixtures
 * with no game running.
 *
 * The lines are asserted whole, as the player reads them, since the wording is
 * the point of reading the table at all. What else a source carries is asserted
 * as the values it holds, because that is what the stat index and the catalogue
 * item bridge will read rather than the sentences.
 *
 * The realm tables are hand written rather than the game's: they carry the rows
 * a shipped table does not, which is a unique the game has never heard of, a
 * divider, an unreleased row, and a table whose enabled column has been dropped
 * altogether.
 */

using Catalogue::Source;

namespace {

const char* kRealmUniques = "BHTests/fixtures/tables/RealmUniqueItems.txt";
const char* kUnflaggedUniques = "BHTests/fixtures/tables/UnflaggedUniqueItems.txt";

const Source& Resolve(const std::string& code) {
	TableFixture::Load();
	const Source* source = UniqueCatalogue::Find(code);
	REQUIRE_MESSAGE(source != NULL, "not in the fixture: " << code);
	return *source;
}

std::vector<std::string> Names(const std::vector<Source>& sources) {
	std::vector<std::string> names;
	for (unsigned int i = 0; i < sources.size(); i++)
		names.push_back(sources[i].name);
	return names;
}

std::vector<Source> ReadTable(const char* path, int rows) {
	TableFixture::Load();
	Table table(path);
	REQUIRE_MESSAGE(table.size() == rows,
		"fixture not found, run the tests from the repository root: " << path);
	return UniqueCatalogue::Read(table);
}

}  // namespace

TEST_CASE("a unique reads as the game words it") {
	TableFixture::Load();

	SUBCASE("Harlequin Crest") {
		CHECK(Resolve("Harlequin Crest").lines == std::vector<std::string>({
			"+2 to All Skills",
			"+2 to all Attributes",
			"+1.50 to Life (Based on Character Level)",
			"+1.50 to Mana (Based on Character Level)",
			"Damage Reduced by 10%",
			"50% Better Chance of Getting Magic Items",
		}));
	}

	SUBCASE("Skin of the Vipermagi") {
		CHECK(Resolve("Skin of the Vipermagi").lines == std::vector<std::string>({
			"+1 to All Skills",
			"+30% Faster Cast Rate",
			"+120% Enhanced Defense",
			"All Resistances +20-35",
			"Magic Damage Reduced by 9-13",
		}));
	}

	SUBCASE("Guardian Angel") {
		CHECK(Resolve("Guardian Angel").lines == std::vector<std::string>({
			"+1 to Paladin Skill Levels",
			"+30% Faster Block Rate",
			"20% Increased Chance of Blocking",
			"+0.62 to Attack Rating against Demons (Based on Character Level)",
			"+180-200% Enhanced Defense",
			// One line a resistance, as the game words a maximum it raises
			// across all four.
			"+15% to Maximum Poison Resist",
			"+15% to Maximum Cold Resist",
			"+15% to Maximum Lightning Resist",
			"+15% to Maximum Fire Resist",
			"+4 to Light Radius",
		}));
	}

	SUBCASE("Mara's Kaleidoscope") {
		CHECK(Resolve("Mara's Kaleidoscope").lines == std::vector<std::string>({
			"+2 to All Skills",
			"+5 to all Attributes",
			"All Resistances +20-30",
		}));
	}
}

TEST_CASE("a source carries what the tables say about the unique") {
	const Source& shako = Resolve("Harlequin Crest");
	CHECK(shako.name == "Harlequin Crest");
	CHECK(shako.baseCode == "uap");
	CHECK(shako.baseName == "Shako");
	CHECK(shako.itemType == "Helm");
	CHECK(shako.requiredLevel == 62);
	CHECK(shako.rarity == RarityUnique);

	// A unique asking for more than the base it is made on: the Shako itself
	// asks for level 43.
	CHECK(shako.requiredLevel > ItemDescription::FindBase("uap")->requirements.level);

	const Source& mara = Resolve("Mara's Kaleidoscope");
	CHECK(mara.baseName == "Amulet");
	CHECK(mara.itemType == "Amulet");
}

TEST_CASE("the raw property entries are kept beside the worded lines") {
	const Source& mara = Resolve("Mara's Kaleidoscope");
	// Four attribute properties word themselves into one line, so the entries
	// are not recoverable from the lines.
	REQUIRE(mara.properties.size() == 6);
	CHECK(mara.properties[0].code == "allskills");
	CHECK(mara.properties[1].code == "res-all");
	CHECK(mara.properties[1].min == 20);
	CHECK(mara.properties[1].max == 30);
	CHECK(mara.lines.size() == 3);
}

TEST_CASE("every source is ordered for a list") {
	TableFixture::Load();
	CHECK(Names(UniqueCatalogue::Sources()) == std::vector<std::string>({
		"Guardian Angel",
		"Harlequin Crest",
		"Mara's Kaleidoscope",
		"Skin of the Vipermagi",
	}));
}

TEST_CASE("the catalogue is built once and kept") {
	TableFixture::Load();
	const std::vector<Source>& first = UniqueCatalogue::Sources();
	const std::vector<Source>& again = UniqueCatalogue::Sources();
	CHECK(&first == &again);
	// A lookup answers with the source the list holds rather than a copy of it.
	CHECK(UniqueCatalogue::Find("Harlequin Crest") == &first[1]);
	CHECK(UniqueCatalogue::Loaded());
}

TEST_CASE("a code no unique carries is not found") {
	TableFixture::Load();
	CHECK(UniqueCatalogue::Find("Harlequin Hat") == NULL);
	CHECK(UniqueCatalogue::Find("") == NULL);
}

TEST_CASE("a table a realm has modified is read the same way") {
	std::vector<Source> sources = ReadTable(kRealmUniques, 4);

	SUBCASE("a divider row and an unreleased row are left out") {
		REQUIRE(sources.size() == 2);
		CHECK(Names(sources) ==
			std::vector<std::string>({ "Harlequin Crest", "Crown of the Realm" }));
	}

	SUBCASE("a unique the game does not ship is read like any other") {
		REQUIRE(sources.size() == 2);
		const Source& crown = sources[1];
		CHECK(crown.code == "Crown of the Realm");
		// The string table has no entry for it, so it is called what the table
		// calls it.
		CHECK(crown.name == "Crown of the Realm");
		CHECK(crown.baseName == "Cap");
		CHECK(crown.itemType == "Helm");
		CHECK(crown.requiredLevel == 35);
		CHECK(crown.lines == std::vector<std::string>({
			"+25 to Strength",
			"All Resistances +15",
		}));
	}

	SUBCASE("a unique the realm has edited reads as it edited it") {
		REQUIRE(sources.size() == 2);
		CHECK(sources[0].lines == std::vector<std::string>({
			"+3 to All Skills",
			"75% Better Chance of Getting Magic Items",
		}));
	}
}

TEST_CASE("a table with nothing flagged enabled falls back to every row") {
	std::vector<Source> sources = ReadTable(kUnflaggedUniques, 2);
	REQUIRE(sources.size() == 1);
	CHECK(sources[0].code == "Amulet of the Realm");
	CHECK(sources[0].lines == std::vector<std::string>({ "+10 to Strength" }));
}

