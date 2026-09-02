#include "doctest.h"
#include <string>
#include <vector>
#include "Catalogue/SetCatalogue.h"
#include "ItemDescription.h"
#include "TableFixture.h"
#include "TableReader.h"

/*
 * What the set items catalogue makes of Sets.txt and SetItems.txt, stood up
 * from fixtures with no game running.
 *
 * A set grants in two places, so both are asserted: what a piece is worth on
 * its own, and what its set is worth once enough of it is worn. The set bonus
 * is read as a source of its own, so what it grants is asked of it rather than
 * of any piece.
 *
 * The realm tables are hand written rather than the game's: they carry the rows
 * a shipped table does not, which is a set the game has never heard of, a
 * divider, and a piece belonging to a set the sets table does not carry.
 */

using Catalogue::Source;

namespace {

const char* kRealmSets = "BHTests/fixtures/tables/RealmSets.txt";
const char* kRealmSetItems = "BHTests/fixtures/tables/RealmSetItems.txt";

const Source& Piece(const std::string& code) {
	TableFixture::Load();
	const std::vector<Source>& pieces = SetCatalogue::Pieces();
	for (unsigned int i = 0; i < pieces.size(); i++) {
		if (pieces[i].code.compare(code) == 0)
			return pieces[i];
	}
	REQUIRE_MESSAGE(false, "not in the fixture: " << code);
	return pieces[0];
}

const Source& Bonus(const std::string& setCode) {
	TableFixture::Load();
	const Source* bonus = SetCatalogue::FindBonus(setCode);
	REQUIRE_MESSAGE(bonus != NULL, "not in the fixture: " << setCode);
	return *bonus;
}

std::vector<std::string> Names(const std::vector<Source>& sources) {
	std::vector<std::string> names;
	for (unsigned int i = 0; i < sources.size(); i++)
		names.push_back(sources[i].name);
	return names;
}

Table Fixture(const char* path, int rows) {
	TableFixture::Load();
	Table table(path);
	REQUIRE_MESSAGE(table.size() == rows,
		"fixture not found, run the tests from the repository root: " << path);
	return table;
}

}  // namespace

TEST_CASE("a piece carries what the tables say about it") {
	const Source& ward = Piece("Civerb's Ward");
	CHECK(ward.name == "Civerb's Ward");
	CHECK(ward.baseCode == "lrg");
	CHECK(ward.baseName == "Large Shield");
	CHECK(ward.itemType == "Shield");
	CHECK(ward.setCode == "Civerb's Vestments");
	CHECK(ward.setName == "Civerb's Vestments");
	CHECK(ward.requiredLevel == 9);
	CHECK(ward.rarity == RaritySet);

	// What its own always-on properties do to the numbers its base carries. Its
	// set's bonuses are not in here: they only apply once enough of the set is
	// worn, and the piece on its own is what this describes.
	CHECK(ward.modifiers.defenseFlat.low == 15);
	CHECK(ward.modifiers.defenseFlat.high == 15);
}

TEST_CASE("a piece the file calls by a working title reads as the game names it") {
	CHECK(Piece("Tal Rasha's Fire-Spun Cloth").name ==
		"Tal Rasha's Fine-Spun Cloth");
	CHECK(Piece("Tal Rasha's Howling Wind").name == "Tal Rasha's Guardianship");
}

TEST_CASE("a piece reads as the game words it") {
	SUBCASE("what it grants on its own") {
		CHECK(Piece("Civerb's Ward").lines == std::vector<std::string>({
			"15% Increased Chance of Blocking",
			"+15 Defense",
		}));
	}

	SUBCASE("and what more of its set unlocks, each line carrying its count") {
		CHECK(Piece("Civerb's Ward").partialLines == std::vector<std::string>({
			"+21-22 to Mana (2 Items)",
			"Poison Resist +25-26% (2 Items)",
		}));
	}
}

TEST_CASE("how many pieces unlock a bonus is what add func says") {
	SUBCASE("all at once, as soon as one other piece is worn") {
		const Source& ward = Piece("Civerb's Ward");
		REQUIRE(ward.partial.size() == 2);
		CHECK(ward.partial[0].itemCount == 2);
		CHECK(ward.partial[1].itemCount == 2);
	}

	SUBCASE("one at a time, a piece at a time") {
		const Source& icon = Piece("Civerb's Icon");
		REQUIRE(icon.partial.size() == 2);
		CHECK(icon.partial[0].code == "res-cold");
		CHECK(icon.partial[0].itemCount == 2);
		CHECK(icon.partial[1].code == "ac");
		CHECK(icon.partial[1].itemCount == 3);
		CHECK(icon.partialLines == std::vector<std::string>({
			"Cold Resist +25% (2 Items)",
			"+25 Defense (3 Items)",
		}));
	}

	SUBCASE("never, whatever the file lists") {
		// A blank add func is not merely a piece with no aprops listed:
		// Civerb's Cudgel lists a per level damage bonus the game has never
		// granted it.
		const Source& cudgel = Piece("Civerb's Cudgel");
		CHECK(cudgel.partial.empty());
		CHECK(cudgel.partialLines.empty());
	}
}

TEST_CASE("a set's own bonuses are a source of its own") {
	const Source& civerb = Bonus("Civerb's Vestments");
	CHECK(civerb.name == "Civerb's Vestments");
	CHECK(civerb.rarity == RaritySet);

	// A set is made on no base and asks for no level of its own; what it grants
	// is all it is.
	CHECK(civerb.baseCode.empty());
	CHECK(civerb.requiredLevel == 0);

	SUBCASE("what wearing all of it gives") {
		CHECK(civerb.lines == std::vector<std::string>({
			"+200% Damage to Undead",
			"+15 to Strength",
			"Lightning Resist +25%",
		}));
	}

	SUBCASE("and what arrives before that, each line carrying its count") {
		CHECK(civerb.partialLines ==
			std::vector<std::string>({ "Fire Resist +15% (2 Items)" }));
	}
}

TEST_CASE("a partial bonus carries how many pieces have to be worn") {
	const Source& tal = Bonus("Tal Rasha's Wrappings");
	REQUIRE(tal.partial.size() == 3);
	CHECK(tal.partial[0].code == "regen");
	CHECK(tal.partial[0].itemCount == 2);
	CHECK(tal.partial[1].code == "mag%");
	CHECK(tal.partial[1].itemCount == 3);
	CHECK(tal.partial[2].code == "balance2");
	CHECK(tal.partial[2].itemCount == 4);

	// Rendered a count at a time and in ascending order of it, since a count is
	// its own line on the item.
	CHECK(tal.partialLines == std::vector<std::string>({
		"Replenish Life +10 (2 Items)",
		"65% Better Chance of Getting Magic Items (3 Items)",
		"+25% Faster Hit Recovery (4 Items)",
	}));
}

TEST_CASE("a set bonus grants what none of its pieces do") {
	// Nothing Civerb's three pieces carry grants lightning resistance; wearing
	// all three is the only way to have it, so the set is what grants it.
	const Source& civerb = Bonus("Civerb's Vestments");
	bool granted = false;
	for (unsigned int i = 0; i < civerb.properties.size(); i++)
		granted |= civerb.properties[i].code.compare("res-ltng") == 0;
	CHECK(granted);

	const std::vector<Source>& pieces = SetCatalogue::Pieces();
	for (unsigned int i = 0; i < pieces.size(); i++) {
		if (pieces[i].setCode.compare("Civerb's Vestments") != 0)
			continue;
		for (unsigned int p = 0; p < pieces[i].properties.size(); p++)
			CHECK(pieces[i].properties[p].code != "res-ltng");
		for (unsigned int p = 0; p < pieces[i].partial.size(); p++)
			CHECK(pieces[i].partial[p].code != "res-ltng");
	}
}

TEST_CASE("every source is ordered for a list") {
	TableFixture::Load();

	SUBCASE("the sets by name") {
		CHECK(Names(SetCatalogue::Bonuses()) == std::vector<std::string>({
			"Civerb's Vestments",
			"Tal Rasha's Wrappings",
		}));
	}

	// Within a set the table's own order, which is the game's head to toe one.
	SUBCASE("the pieces by set, and by the order the table holds them") {
		CHECK(Names(SetCatalogue::Pieces()) == std::vector<std::string>({
			"Civerb's Ward",
			"Civerb's Icon",
			"Civerb's Cudgel",
			"Tal Rasha's Fine-Spun Cloth",
			"Tal Rasha's Adjudication",
			"Tal Rasha's Lidless Eye",
			"Tal Rasha's Guardianship",
			"Tal Rasha's Horadric Crest",
		}));
	}
}

TEST_CASE("the catalogue is built once and kept") {
	TableFixture::Load();
	const std::vector<Source>& first = SetCatalogue::Pieces();
	const std::vector<Source>& again = SetCatalogue::Pieces();
	CHECK(&first == &again);
	// A lookup answers with the source the list holds rather than a copy of it.
	CHECK(SetCatalogue::FindBonus("Civerb's Vestments") ==
		&SetCatalogue::Bonuses()[0]);
	CHECK(SetCatalogue::Loaded());
}

TEST_CASE("a code no set carries is not found") {
	TableFixture::Load();
	CHECK(SetCatalogue::FindBonus("Vestments of the Realm") == NULL);
	CHECK(SetCatalogue::FindBonus("") == NULL);
}

TEST_CASE("a table a realm has modified is read the same way") {
	Table sets = Fixture(kRealmSets, 1);
	Table setItems = Fixture(kRealmSetItems, 4);

	SUBCASE("a set the game does not ship is read like any other") {
		std::vector<Source> bonuses = SetCatalogue::ReadBonuses(sets);
		REQUIRE(bonuses.size() == 1);
		CHECK(bonuses[0].code == "Vestments of the Realm");
		// The string table has no entry for it, so it is called what the table
		// calls it.
		CHECK(bonuses[0].name == "Vestments of the Realm");
		CHECK(bonuses[0].partialLines ==
			std::vector<std::string>({ "Fire Resist +20% (2 Items)" }));
		CHECK(bonuses[0].lines == std::vector<std::string>({
			"Lightning Resist +30%",
			"50% Better Chance of Getting Magic Items",
		}));
	}

	SUBCASE("a divider row is left out") {
		std::vector<Source> pieces = SetCatalogue::ReadPieces(setItems);
		REQUIRE(pieces.size() == 3);
		CHECK(Names(pieces) == std::vector<std::string>({
			"Ward of the Realm",
			"Icon of the Realm",
			"Charm of the Realm",
		}));
	}

	SUBCASE("a piece whose set the sets table does not carry keeps its set") {
		std::vector<Source> pieces = SetCatalogue::ReadPieces(setItems);
		REQUIRE(pieces.size() == 3);
		const Source& orphan = pieces[2];
		CHECK(orphan.setCode == "Baubles of the Realm");
		CHECK(orphan.setName == "Baubles of the Realm");
		CHECK(orphan.baseName == "Cap");
		CHECK(orphan.lines == std::vector<std::string>({ "+10 to Strength" }));
	}
}
