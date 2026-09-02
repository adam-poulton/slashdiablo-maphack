#include "doctest.h"
#include <string>
#include <vector>
#include "Catalogue/BaseItemCatalogue.h"
#include "Catalogue/Catalogues.h"
#include "Catalogue/StatIndex.h"
#include "ItemDescription.h"
#include "TableFixture.h"

/*
 * What the base items catalogue makes of the game's Weapons, Armor and Misc
 * tables, stood up from fixtures with no game running.
 *
 * The fixtures carry bases out of all three tables and one of each of the three
 * tiers, which between them are every way a base can be read. What a base is
 * worth is not asserted again here: that is BaseItemTests, and the assertion
 * this file makes is that all of it is reachable from a source.
 *
 * The order is asserted as the two rules that make it rather than as the whole
 * list of bases, so that a fixture grown for another catalogue's tests does not
 * move it: the item types come in the order the tables reach them, and within a
 * type the bases run from normal to elite and then by the level each starts
 * dropping from.
 *
 * A base grants nothing, so it is the one kind of source that answers no
 * criterion about a stat. That is asserted as a query answering nothing rather
 * than as a source holding no properties, because the first is what a player
 * asking for a stat would see.
 */

using Catalogue::Source;
using StatIndex::Criterion;
using StatIndex::Query;
using StatIndex::Result;

namespace {

const Source& Resolve(const std::string& code) {
	TableFixture::Load();
	const Source* source = BaseItemCatalogue::Find(code);
	REQUIRE_MESSAGE(source != NULL, "not in the fixture: " << code);
	return *source;
}

// The base the source names, which is where everything the game shows about it
// is kept.
const ItemDescription::Base& BaseUnder(const Source& source) {
	const ItemDescription::Base* base = ItemDescription::FindBase(source.baseCode);
	REQUIRE_MESSAGE(base != NULL, "no base under: " << source.code);
	return *base;
}

std::vector<std::string> Names(const std::vector<Source>& sources) {
	std::vector<std::string> names;
	for (unsigned int i = 0; i < sources.size(); i++)
		names.push_back(sources[i].name);
	return names;
}

std::vector<std::string> Answers(const std::vector<Criterion>& criteria) {
	TableFixture::Load();
	Catalogue::Load();
	REQUIRE(Catalogue::Loaded());

	Query query;
	query.kind = BaseItemCatalogue::Kind;
	query.criteria = criteria;

	std::vector<Result> results = StatIndex::Find(query);
	std::vector<std::string> names;
	for (unsigned int i = 0; i < results.size(); i++)
		names.push_back(results[i].entry->source->name);
	return names;
}

// The item types in the order the catalogue reaches them, which is the order
// the panel puts its headings in.
std::vector<std::string> ItemTypesInOrder() {
	TableFixture::Load();
	const std::vector<Source>& bases = BaseItemCatalogue::Sources();
	std::vector<std::string> types;
	for (unsigned int i = 0; i < bases.size(); i++) {
		if (types.empty() || types.back() != bases[i].itemType)
			types.push_back(bases[i].itemType);
	}
	return types;
}

// The bases of one item type, in the order the catalogue lists them.
std::vector<std::string> NamesOfType(const std::string& itemType) {
	TableFixture::Load();
	const std::vector<Source>& bases = BaseItemCatalogue::Sources();
	std::vector<std::string> names;
	for (unsigned int i = 0; i < bases.size(); i++) {
		if (bases[i].itemType == itemType)
			names.push_back(bases[i].name);
	}
	return names;
}

}  // namespace

TEST_CASE("every base the catalogue lists is in the index, in its order") {
	TableFixture::Load();
	CHECK(Answers({}) == Names(BaseItemCatalogue::Sources()));
	CHECK_FALSE(Answers({}).empty());
}

TEST_CASE("the item types come in the order the game's tables reach them") {
	// The weapons the tables reach first, then the armour, then the miscellany,
	// rather than the two hundred armour bases scattered through the alphabet.
	// Adding a base to the fixtures for another catalogue's tests can add a type
	// here, which is worth being told about.
	CHECK(ItemTypesInOrder() == std::vector<std::string>({
		"Scepter",
		"Sword",
		"Throwing Knife",
		"Polearm",
		"Bow",
		"Orb",
		"Helm",
		"Shield",
		"Armor",
		"Belt",
		"Amulet",
		"Scroll",
		"Amethyst",
		"Topaz",
		"Sapphire",
		"Emerald",
		"Ruby",
		"Diamond",
		"Skull",
		"Small Charm",
		"Rune",
		"Jewel",
	}));
}

TEST_CASE("within an item type the bases run by tier and then by dropping level") {
	// All three tiers in one type, and two bases in each of them, so the level
	// tiebreak is asserted as well as the tiers.
	CHECK(NamesOfType("Helm") == std::vector<std::string>({
		"Cap",			// normal, from level 1
		"Full Helm",	// normal, from level 15
		"Basinet",		// exceptional, from level 45
		"Death Mask",	// exceptional, from level 48
		"Giant Conch",	// elite, from level 54
		"Shako",		// elite, from level 58
	}));
}

TEST_CASE("a base carries what it is and when it starts dropping") {
	const Source& shako = Resolve("uap");
	CHECK(shako.code == "uap");
	CHECK(shako.name == "Shako");
	CHECK(shako.itemType == "Helm");
	CHECK(shako.tier == ItemDescription::TierElite);
	CHECK(shako.level == 58);
	CHECK(shako.requiredLevel == 43);
	// Nothing has been made of it, which is how the game draws one.
	CHECK(shako.rarity == RarityNormal);

	// A base is the base it is made on, so whatever reads a source's base reads
	// the same field whichever catalogue the source came out of.
	CHECK(shako.baseCode == "uap");
	CHECK(shako.baseName == "Shako");

	// It grants nothing, which is what makes it the one kind of source with no
	// properties and no lines to word them into.
	CHECK(shako.properties.empty());
	CHECK(shako.lines.empty());
}

TEST_CASE("a base of each tier reads as the tier it is") {
	CHECK(Resolve("cap").tier == ItemDescription::TierNormal);
	CHECK(Resolve("xea").tier == ItemDescription::TierExceptional);
	CHECK(Resolve("uap").tier == ItemDescription::TierElite);
	// Misc.txt has no upgrade columns at all, which leaves everything in it
	// normal.
	CHECK(Resolve("amu").tier == ItemDescription::TierNormal);
}

TEST_CASE("a base out of each of the three tables is read") {
	SUBCASE("Weapons.txt") {
		const Source& colossusBlade = Resolve("7gd");
		CHECK(colossusBlade.name == "Colossus Blade");
		CHECK(colossusBlade.itemType == "Sword");
	}

	SUBCASE("Armor.txt") {
		const Source& serpentskin = Resolve("xea");
		CHECK(serpentskin.name == "Serpentskin Armor");
		CHECK(serpentskin.itemType == "Armor");
	}

	SUBCASE("Misc.txt") {
		// Misc.txt calls this one "Charm Small", and only the string table gets
		// it the way the player reads it.
		const Source& smallCharm = Resolve("cm1");
		CHECK(smallCharm.name == "Small Charm");
		CHECK(smallCharm.itemType == "Small Charm");
	}
}

TEST_CASE("what the game shows about a base is reachable from the source") {
	SUBCASE("a weapon's damage in each way it can be swung") {
		const ItemDescription::Base& colossusBlade = BaseUnder(Resolve("7gd"));
		CHECK(colossusBlade.oneHandDamage.low == 25);
		CHECK(colossusBlade.oneHandDamage.high == 65);
		CHECK(colossusBlade.twoHandDamage.low == 58);
		CHECK(colossusBlade.twoHandDamage.high == 115);
		CHECK(colossusBlade.durability == 50);
	}

	SUBCASE("armour's defense and what it takes to wear") {
		const ItemDescription::Base& shako = BaseUnder(Resolve("uap"));
		CHECK(shako.defense.low == 98);
		CHECK(shako.defense.high == 141);
		CHECK(shako.durability == 12);
		CHECK(shako.requirements.level == 43);
		CHECK(shako.requirements.strength.low == 50);
		CHECK(shako.maxSockets == 2);
	}
}

TEST_CASE("a base is found by everything the panel says can be searched") {
	SUBCASE("its name") {
		CHECK(Answers({ Criterion::OnText("voulge") }) ==
			std::vector<std::string>({ "Colossus Voulge" }));
	}

	SUBCASE("its item type") {
		CHECK(Answers({ Criterion::OnText("sword") }) == NamesOfType("Sword"));
	}

	SUBCASE("its tier, which nothing about its name says") {
		CHECK(Answers({ Criterion::OnText("exceptional") }) ==
			std::vector<std::string>({ "Swirling Crystal", "Basinet",
				"Death Mask", "Serpentskin Armor", "Templar Coat",
				"Mesh Belt" }));
	}

	SUBCASE("the code the tables know it by") {
		CHECK(Answers({ Criterion::OnText("7gd") }) ==
			std::vector<std::string>({ "Colossus Blade" }));
	}

	SUBCASE("a search box with nothing typed in it shows the whole list") {
		CHECK(Answers({ Criterion::OnText("") }) == Answers({}));
	}
}

// The one criterion a base can never answer, since granting nothing is what a
// base is.
TEST_CASE("a source that grants nothing matches no criterion about a stat") {
	CHECK(Answers({ Criterion::OnStat("strength", StatIndex::GreaterThan, 0) }).empty());

	// Not even a comparator a zero would satisfy: a base does not grant zero
	// strength, it grants no strength at all.
	CHECK(Answers({ Criterion::OnStat("strength", StatIndex::LessThan, 5) }).empty());
	CHECK(Answers({ Criterion::OnStat("strength", StatIndex::EqualTo, 0) }).empty());

	// And a stat criterion beside text it does carry still leaves nothing.
	CHECK(Answers({
		Criterion::OnText("shako"),
		Criterion::OnStat("armorclass", StatIndex::GreaterThan, 0),
	}).empty());
}

TEST_CASE("a base is looked up by the code the tables know it by") {
	TableFixture::Load();
	CHECK(BaseItemCatalogue::Find("7cr")->name == "Phase Blade");
	// A code the tables do not carry is not a base.
	CHECK(BaseItemCatalogue::Find("zzz") == NULL);
}

TEST_CASE("only what the game drops is listed") {
	TableFixture::Load();

	ItemDescription::Base dropped;
	dropped.code = "aaa";
	dropped.name = "Dropped";
	dropped.typeName = "Sword";
	dropped.spawnable = true;

	ItemDescription::Base unfinished;
	unfinished.code = "bbb";
	unfinished.name = "Unfinished";
	unfinished.typeName = "Sword";
	unfinished.spawnable = false;

	// A base with no name has nothing to be listed under, whoever drops it.
	ItemDescription::Base nameless;
	nameless.code = "ccc";
	nameless.typeName = "Sword";
	nameless.spawnable = true;

	SUBCASE("what the game never drops is left out") {
		std::vector<const ItemDescription::Base*> bases({
			&dropped, &unfinished, &nameless });
		CHECK(Names(BaseItemCatalogue::Read(bases)) ==
			std::vector<std::string>({ "Dropped" }));
	}

	// A realm that has dropped the column flags nothing, and an empty tab is
	// worse than one listing the unreleased rows too.
	SUBCASE("a table flagging nothing as dropped is read whole") {
		std::vector<const ItemDescription::Base*> bases({ &unfinished });
		CHECK(Names(BaseItemCatalogue::Read(bases)) ==
			std::vector<std::string>({ "Unfinished" }));
	}
}
