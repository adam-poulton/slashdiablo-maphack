#include "doctest.h"
#include <string>
#include <vector>
#include "Catalogue/AffixCatalogue.h"
#include "TableFixture.h"
#include "TableReader.h"

/*
 * What the affixes catalogue makes of MagicPrefix.txt and MagicSuffix.txt,
 * stood up from fixtures with no game running.
 *
 * An affix name spells many rows, so an affix under test is named by its code
 * and the level it starts rolling from together, which is what tells the eight
 * "Serpent's" apart. The lines are asserted whole, as the player reads them.
 *
 * The realm tables are hand written rather than the game's: they carry the rows
 * a shipped table does not, which is a divider granting nothing, a row the
 * realm has turned off, and a table whose spawnable column has been dropped
 * altogether.
 */

using Catalogue::Source;

namespace {

const char* kRealmPrefix = "BHTests/fixtures/tables/RealmMagicPrefix.txt";
const char* kUnflaggedPrefix = "BHTests/fixtures/tables/UnflaggedMagicPrefix.txt";

const Source& At(const std::vector<Source>& sources, const std::string& code,
		int level) {
	for (unsigned int i = 0; i < sources.size(); i++) {
		if (sources[i].code.compare(code) == 0 && sources[i].level == level)
			return sources[i];
	}
	REQUIRE_MESSAGE(false, "not in the fixture: " << code << " at " << level);
	return sources[0];
}

const Source& Prefix(const std::string& code, int level) {
	TableFixture::Load();
	return At(AffixCatalogue::Prefixes(), code, level);
}

const Source& Suffix(const std::string& code, int level) {
	TableFixture::Load();
	return At(AffixCatalogue::Suffixes(), code, level);
}

std::vector<Source> ReadTable(const char* path, int rows) {
	TableFixture::Load();
	Table table(path);
	REQUIRE_MESSAGE(table.size() == rows,
		"fixture not found, run the tests from the repository root: "
		<< std::string(path));
	return AffixCatalogue::Read(table);
}

std::vector<std::string> Codes(const std::vector<Source>& sources) {
	std::vector<std::string> codes;
	for (unsigned int i = 0; i < sources.size(); i++)
		codes.push_back(sources[i].code);
	return codes;
}

}  // namespace

TEST_CASE("an affix reads as the game words it") {
	SUBCASE("a prefix granting one thing") {
		CHECK(Prefix("Burgundy", 12).lines ==
			std::vector<std::string>({ "Fire Resist +11-20%" }));
	}

	SUBCASE("a suffix granting one thing") {
		CHECK(Suffix("of Wizardry", 30).lines ==
			std::vector<std::string>({ "+21-30 to Mana" }));
	}

	SUBCASE("an affix granting two things, in the order the game shows them") {
		CHECK(Suffix("of Incineration", 43).lines == std::vector<std::string>({
			"+10-20 to Minimum Fire Damage",
			"+21-75 to Maximum Fire Damage",
		}));
	}
}

TEST_CASE("an affix is called what the player reads, not what the table keys it by") {
	const Source& fiery = Prefix("Ember", 25);
	CHECK(fiery.code == "Ember");
	CHECK(fiery.name == "Fiery");

	// A magic item is the one thing an affix names on its own, and blue is how
	// the game draws that.
	CHECK(fiery.rarity == RarityMagic);
}

TEST_CASE("an affix carries what it rolls on and from what level") {
	SUBCASE("the kinds of base it is allowed on") {
		CHECK(Prefix("Bahamut's", 45).itemType ==
			"Staves And Rods, Ring, Amulet, Orb, Circlet");
	}

	SUBCASE("down to the ones it is not, which the broader kind would include") {
		CHECK(Prefix("Burgundy", 35).itemType == "Weapon (not Staves And Rods)");
		CHECK(Suffix("of Incineration", 43).itemType ==
			"Melee Weapon (not Wand, Orb)");
	}

	SUBCASE("a kind the table names twice is said once") {
		// The shipped table lists "amul" in two of Crimson's seven columns.
		CHECK(Prefix("Crimson", 5).itemType ==
			"Any Armor, Staves And Rods, Missile Weapon, Ring, Amulet");
	}

	SUBCASE("the level it starts rolling from and the level it asks of a character") {
		const Source& garnet = Prefix("Garnet", 55);
		CHECK(garnet.level == 55);
		CHECK(garnet.requiredLevel == 41);
	}

	SUBCASE("the one class that can carry it, where only a class rolls it") {
		// Said as a note rather than as a kind of base, since a Large Charm is
		// a Large Charm whoever picks it up.
		const Source& fletcher = Prefix("Fletcher's", 50);
		CHECK(fletcher.itemType == "Large Charm");
		CHECK(fletcher.notes == std::vector<std::string>({ "(Amazon Only)" }));

		// Every other affix says nothing about a class, having none.
		CHECK(Prefix("Bahamut's", 45).notes.empty());
	}

	SUBCASE("the same affix at another band of levels is a source of its own") {
		CHECK(Prefix("Garnet", 18).requiredLevel == 13);
		CHECK(Prefix("Garnet", 18).itemType ==
			"Any Armor, Staves And Rods, Ring, Amulet (not Gloves)");
	}
}

TEST_CASE("both tables are read whole") {
	TableFixture::Load();
	CHECK(AffixCatalogue::Prefixes().size() == 587);
	CHECK(AffixCatalogue::Suffixes().size() == 574);
	CHECK(AffixCatalogue::Loaded());
}

TEST_CASE("only the rows the game rolls are read") {
	// The divider and the row the realm turned off are both left out, which
	// leaves the one affix the table really carries. The blank row the table is
	// padded with never reaches the catalogue at all.
	std::vector<Source> realm = ReadTable(kRealmPrefix, 3);
	CHECK(Codes(realm) == std::vector<std::string>({ "Realmish" }));

	REQUIRE(realm.size() == 1);
	const Source& realmish = realm[0];
	CHECK(realmish.level == 5);
	CHECK(realmish.requiredLevel == 3);
	CHECK(realmish.itemType == "Weapon (not Bow)");
	CHECK(realmish.lines == std::vector<std::string>({ "+10-20 to Strength" }));

	// A table with no spawnable column at all says nothing about what the game
	// rolls, so every row granting something is read rather than none of them.
	CHECK(Codes(ReadTable(kUnflaggedPrefix, 3)) ==
		std::vector<std::string>({ "Realmish", "Withdrawn" }));
}
