#include "doctest.h"
#include <algorithm>
#include <string>
#include <vector>
#include "Catalogue/RunewordCatalogue.h"
#include "TableFixture.h"
#include "TableReader.h"

/*
 * What the runewords catalogue makes of runes.txt, stood up from fixtures with
 * no game running.
 *
 * The lines are asserted whole, as the player reads them, since the wording is
 * the point of reading the table at all. A runeword is worded once per kind of
 * base it is allowed in, so the runewords under test between them reach each of
 * the three kinds, and one of them reaches two.
 *
 * The realm tables are hand written rather than the game's: they carry the rows
 * a shipped table does not, which is a runeword the game has never heard of, a
 * divider, a row named but not flagged complete, a row allowed in a type that
 * belongs to no root category at all, and a table whose complete column has
 * been dropped altogether.
 */

using Catalogue::Source;
using Catalogue::Variant;

namespace {

const char* kRealmRunes = "BHTests/fixtures/tables/RealmRunes.txt";
const char* kUnflaggedRunes = "BHTests/fixtures/tables/UnflaggedRunes.txt";

const Source& Resolve(const std::string& code) {
	TableFixture::Load();
	const Source* source = RunewordCatalogue::Find(code);
	REQUIRE_MESSAGE(source != NULL, "not in the fixture: " << code);
	return *source;
}

std::vector<std::string> Names(const std::vector<Source>& sources) {
	std::vector<std::string> names;
	for (unsigned int i = 0; i < sources.size(); i++)
		names.push_back(sources[i].name);
	return names;
}

std::vector<std::string> BaseKinds(const std::vector<Variant>& variants) {
	std::vector<std::string> kinds;
	for (unsigned int i = 0; i < variants.size(); i++)
		kinds.push_back(variants[i].baseKind);
	return kinds;
}

std::vector<Source> ReadTable(const char* path, int rows) {
	TableFixture::Load();
	Table table(path);
	REQUIRE_MESSAGE(table.size() == rows,
		"fixture not found, run the tests from the repository root: " << path);
	return RunewordCatalogue::Read(table);
}

const Source& Named(const std::vector<Source>& sources,
		const std::string& name) {
	for (unsigned int i = 0; i < sources.size(); i++) {
		if (sources[i].name.compare(name) == 0)
			return sources[i];
	}
	REQUIRE_MESSAGE(false, "not read: " << name);
	return sources[0];
}

}  // namespace

TEST_CASE("a runeword reads as the game words it") {
	TableFixture::Load();

	SUBCASE("Lore, whose runes give their helm bonuses") {
		CHECK(Resolve("Runeword75").lines == std::vector<std::string>({
			"+1 to All Skills",
			"+10 to Energy",
			"Lightning Resist +30%",
			"Damage Reduced by 7",
			"+2 to Mana after each Kill",
			"+2 to Light Radius",
		}));
	}

	SUBCASE("Splendor, whose runes give their shield bonuses") {
		CHECK(Resolve("Runeword131").lines == std::vector<std::string>({
			"+1 to All Skills",
			"+10% Faster Cast Rate",
			"+20% Faster Block Rate",
			"+60-100% Enhanced Defense",
			"+10 to Energy",
			"Regenerate Mana 15%",
			"50% Extra Gold from Monsters",
			"20% Better Chance of Getting Magic Items",
			"+3 to Light Radius",
		}));
	}

	SUBCASE("Enigma, a body armour worded as a helm is") {
		CHECK(Resolve("Runeword33").lines == std::vector<std::string>({
			"+2 to All Skills",
			"+45% Faster Run/Walk",
			"+1 to Teleport",
			"+750-775 Defense",
			"+0.75 to Strength (Based on Character Level)",
			"Increase Maximum Life 5%",
			"Damage Reduced by 8%",
			"+14 Life after each Kill",
			"15% Damage Taken Goes To Mana",
			"1% Better Chance of Getting Magic Items (Based on Character Level)",
		}));
	}
}

TEST_CASE("a source carries what the tables say about the runeword") {
	const Source& enigma = Resolve("Runeword33");
	// The string table is what gives it a name; the table's own column is only
	// the fallback.
	CHECK(enigma.code == "Runeword33");
	CHECK(enigma.name == "Enigma");
	CHECK(enigma.itemType == "Armor");
	CHECK(enigma.rarity == RarityRuneword);

	// The runes in socket order, which is the order they have to be put in.
	CHECK(enigma.ingredientCodes ==
		std::vector<std::string>({ "r31", "r06", "r30" }));
	CHECK(enigma.ingredients == "Jah + Ith + Ber");

	// A runeword can be made as soon as its highest rune can be worn, which is
	// the level Jah asks for.
	CHECK(enigma.requiredLevel == 65);

	// Its own bonuses, kept beside the lines they and the runes word themselves
	// into: seven entries against ten lines.
	REQUIRE(enigma.properties.size() == 7);
	CHECK(enigma.properties[0].code == "ac");
	CHECK(enigma.properties[0].min == 750);
	CHECK(enigma.properties[0].max == 775);
	CHECK(enigma.properties[6].code == "oskill");
	CHECK(enigma.properties[6].param == "Teleport");
	CHECK(enigma.lines.size() == 10);

	// A runeword is made on a range of bases rather than on one, so it names
	// none.
	CHECK(enigma.baseCode == "");
	CHECK(enigma.baseName == "");
}

TEST_CASE("the kind of base is decided by walking the item type chain") {
	SUBCASE("a type that is a root category itself") {
		CHECK(BaseKinds(Resolve("Runeword75").variants) ==
			std::vector<std::string>({ "helm" }));
	}

	SUBCASE("body armour, which the runes treat as a helm does") {
		CHECK(BaseKinds(Resolve("Runeword33").variants) ==
			std::vector<std::string>({ "helm" }));
	}

	SUBCASE("a shield, which hangs off Any Armor and is looked for first") {
		CHECK(BaseKinds(Resolve("Runeword131").variants) ==
			std::vector<std::string>({ "shield" }));
	}

	SUBCASE("a type two steps below Any Weapon") {
		// A sword equivs to Melee Weapon, which equivs to Weapon.
		CHECK(BaseKinds(Resolve("Runeword130").variants) ==
			std::vector<std::string>({ "weapon", "shield" }));
	}

	SUBCASE("a type that reaches a root only through its parent") {
		// Auric Shields equiv to Any Shield rather than being it.
		std::vector<Source> sources = ReadTable(kRealmRunes, 8);
		const Source& guard = Named(sources, "Guard of the Realm");
		CHECK(BaseKinds(guard.variants) == std::vector<std::string>({ "shield" }));
		CHECK(guard.variants[0].label == "Auric Shields");
	}

	SUBCASE("a type that reaches no root category at all") {
		// Paladin Item equivs to Class Specific and stops there, which is one
		// of the leftover types the game treats as a weapon.
		std::vector<Source> sources = ReadTable(kRealmRunes, 8);
		const Source& sigil = Named(sources, "Sigil of the Realm");
		CHECK(BaseKinds(sigil.variants) == std::vector<std::string>({ "weapon" }));
	}
}

TEST_CASE("a runeword allowed in two kinds of base is worded for each") {
	const Source& spirit = Resolve("Runeword130");
	CHECK(spirit.itemType == "Sword, Any Shield");
	REQUIRE(spirit.variants.size() == 2);
	CHECK(spirit.variants[0].label == "Sword");
	CHECK(spirit.variants[1].label == "Any Shield");

	SUBCASE("each variant carries the whole of what that kind grants") {
		// The runeword's own seven bonuses and what its four runes add in that
		// kind of base.
		CHECK(spirit.variants[0].properties.size() > spirit.properties.size());
		CHECK(spirit.variants[0].lines != spirit.variants[1].lines);

		// What the runes give a weapon, which a shield never sees.
		CHECK(std::find(spirit.variants[0].lines.begin(),
			spirit.variants[0].lines.end(), "7% Life stolen per hit") !=
			spirit.variants[0].lines.end());
		CHECK(std::find(spirit.variants[1].lines.begin(),
			spirit.variants[1].lines.end(), "7% Life stolen per hit") ==
			spirit.variants[1].lines.end());
	}

	SUBCASE("what every kind grants is said once and only the rest is tagged") {
		CHECK(spirit.lines == std::vector<std::string>({
			// The runeword's own bonuses, which no base changes.
			"+2 to All Skills",
			"+25-35% Faster Cast Rate",
			"+55% Faster Hit Recovery",
			"+250 Defense vs. Missile",
			"+22 to Vitality",
			"+89-112 to Mana",
			"+3-8 Magic Absorb",
			// What the runes add in a sword.
			"+1-50 Lightning Damage  (Sword)",
			"+3-14 Cold Damage  (Sword)",
			"+75 poison damage over 5 seconds  (Sword)",
			"7% Life stolen per hit  (Sword)",
			// And in a shield.
			"Cold Resist +35%  (Any Shield)",
			"Lightning Resist +35%  (Any Shield)",
			"Poison Resist +35%  (Any Shield)",
			"Attacker Takes Damage of 14  (Any Shield)",
		}));
	}
}

TEST_CASE("a recipe shipped outside the table is read like the rest") {
	const Source& plague = Resolve("Plague");
	CHECK(plague.name == "Plague");
	CHECK(plague.ingredients == "Cham + Fal + Um");
	CHECK(plague.itemType == "Weapon");
	CHECK(plague.requiredLevel == 67);
	CHECK(BaseKinds(plague.variants) == std::vector<std::string>({ "weapon" }));

	// Its own bonuses, what its runes add in a weapon, and last the bonus the
	// property tables cannot express.
	CHECK(plague.lines == std::vector<std::string>({
		"25% Chance to cast level 15 Poison Nova on striking",
		"20% Chance to cast level 12 Lower Resist when struck",
		"Level 13-17 Cleansing Aura When Equipped",
		"+1-2 to All Skills",
		"+260-380% Damage to Demons",
		"+5-30 Fire Damage",
		"-23% to Enemy Poison Resistance",
		"25% Chance of Open Wounds",
		"Freezes target +3",
		"+10 to Strength",
		"0.3% Deadly Strike (Based on Character Level)",
	}));
}

TEST_CASE("every source is ordered for a list") {
	TableFixture::Load();
	CHECK(Names(RunewordCatalogue::Sources()) == std::vector<std::string>({
		"Enigma",
		"Lore",
		"Plague",
		"Spirit",
		"Splendor",
	}));
}

TEST_CASE("the catalogue is built once and kept") {
	TableFixture::Load();
	const std::vector<Source>& first = RunewordCatalogue::Sources();
	const std::vector<Source>& again = RunewordCatalogue::Sources();
	CHECK(&first == &again);
	// A lookup answers with the source the list holds rather than a copy of it.
	CHECK(RunewordCatalogue::Find("Runeword75") == &first[1]);
	CHECK(RunewordCatalogue::Loaded());
}

TEST_CASE("a code no runeword carries is not found") {
	TableFixture::Load();
	CHECK(RunewordCatalogue::Find("Runeword999") == NULL);
	CHECK(RunewordCatalogue::Find("") == NULL);
}

TEST_CASE("a table a realm has modified is read the same way") {
	std::vector<Source> sources = ReadTable(kRealmRunes, 8);

	SUBCASE("a divider and a row not flagged complete are left out") {
		CHECK(Names(sources) == std::vector<std::string>({
			"Blade of the Realm",
			"Guard of the Realm",
			"Lore",
			"Plague",
			"Sigil of the Realm",
			"Ward of the Realm",
		}));
	}

	SUBCASE("a runeword the game does not ship is read like any other") {
		const Source& blade = Named(sources, "Blade of the Realm");
		// The string table has no entry for it, so it is called what the table
		// calls it.
		CHECK(blade.code == "Realmword2");
		CHECK(blade.ingredients == "Jah + Ber");
		CHECK(blade.requiredLevel == 65);
		CHECK(blade.lines == std::vector<std::string>({
			"Ignore Target's Defense",
			"20% Chance of Crushing Blow",
			"+25 to Strength",
		}));
	}

	SUBCASE("the types a runeword is not allowed in are named too") {
		CHECK(Named(sources, "Blade of the Realm").itemType ==
			"Weapon (not Bow, Crossbow)");
	}

	SUBCASE("a runeword the realm has edited reads as it edited it") {
		// The game gives Lore one skill level; this table gives two.
		CHECK(Named(sources, "Lore").lines == std::vector<std::string>({
			"+2 to All Skills",
			"Lightning Resist +30%",
			"Damage Reduced by 7",
		}));
	}

	SUBCASE("a stat two kinds of base grant differently is said once each") {
		// Um gives fifteen of every resistance in body armour and twenty two
		// in a shield, so neither line is what the other base rolls. Ber gives
		// both the same, which is what leaves its line untagged.
		const Source& ward = Named(sources, "Ward of the Realm");
		CHECK(BaseKinds(ward.variants) ==
			std::vector<std::string>({ "helm", "shield" }));
		CHECK(ward.lines == std::vector<std::string>({
			"Damage Reduced by 8%",
			"All Resistances +15  (Armor)",
			"All Resistances +22  (Any Shield)",
		}));
	}

	SUBCASE("a recipe the realm has since shipped is the realm's") {
		CHECK(Named(sources, "Plague").code == "Realmword5");
		CHECK(Named(sources, "Plague").lines == std::vector<std::string>({
			"25% Chance of Open Wounds",
			"Freezes target +3",
			"+15 to Strength",
		}));
	}
}

TEST_CASE("a table with nothing flagged complete falls back to every row") {
	std::vector<Source> sources = ReadTable(kUnflaggedRunes, 2);
	CHECK(Names(sources) ==
		std::vector<std::string>({ "Cloak of the Realm", "Plague" }));
	CHECK(Named(sources, "Cloak of the Realm").lines ==
		std::vector<std::string>({
			"+10 to Strength",
			"Lightning Resist +30%",
			"Damage Reduced by 7",
		}));
}
