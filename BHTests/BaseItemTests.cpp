#include "doctest.h"
#include <string>
#include <vector>
#include "ItemDescription.h"
#include "TableFixture.h"
#include "TableReader.h"

/*
 * What the item description module makes of a base item, read out of the
 * game's own Weapons, Armor and Misc tables stood up from fixtures.
 *
 * The bases in the fixtures are chosen for what they carry rather than for
 * being interesting: between them they cover each of the three tiers, damage
 * in each of the ways a weapon can be swung, defense, a durability the game
 * hides, and a name that only the string table gets right. What a base is
 * worth is asserted as the numbers it holds, and then again as the panel the
 * base items tab draws from them, since the two can disagree.
 */

using ItemDescription::Base;
using ItemDescription::Description;
using ItemDescription::FindBase;

namespace {

const Base& Resolve(const std::string& code) {
	TableFixture::Load();
	const Base* base = FindBase(code);
	REQUIRE_MESSAGE(base != NULL, "not in the fixture: " << code);
	return *base;
}

std::vector<std::string> Panel(const std::string& code) {
	TableFixture::Load();
	Description item;
	item.AddBase(code, White);
	item.AddBaseLimits(code);

	std::vector<Drawing::TooltipLine> built = ItemDescription::Build(item);
	std::vector<std::string> lines;
	for (unsigned int i = 0; i < built.size(); i++)
		lines.push_back(built[i].text);
	return lines;
}

void CheckPanel(const std::string& code,
		const std::vector<std::string>& expected) {
	std::vector<std::string> lines = Panel(code);
	REQUIRE(lines.size() == expected.size());
	for (unsigned int i = 0; i < expected.size(); i++)
		CHECK(lines[i] == expected[i]);
}

}  // namespace

TEST_CASE("the table an item type is looked up in is held whole") {
	TableFixture::Load();
	// Every row the game's own table has, since a type trimmed to the items
	// under test would let their sockets and their type names pass against a
	// table the game does not have. Regenerating the fixtures from another
	// version of the game moves this, which is worth being told about.
	CHECK(Tables::ItemTypes.size() == 104);
	// And a type no base under test belongs to still names itself.
	CHECK(ItemDescription::TypeName("ashd") == "Auric Shields");
	CHECK(ItemDescription::TypeName("zzz") == "zzz");
}

TEST_CASE("a base item resolves with no game running") {
	const Base& shako = Resolve("uap");
	CHECK(shako.name == "Shako");
	CHECK(shako.type == "helm");
	CHECK(shako.typeName == "Helm");
	CHECK_FALSE(shako.weapon);
	CHECK(shako.spawnable);
	CHECK(shako.level == 58);
	CHECK(shako.quest == 0);
	CHECK(shako.defense.low == 98);
	CHECK(shako.defense.high == 141);
	CHECK(shako.durability == 12);
	CHECK(shako.requirements.level == 43);
	CHECK(shako.requirements.strength.low == 50);
	CHECK(shako.requirements.dexterity.high == 0);
	// Two by the base, three by the type, and the lower of the two wins.
	CHECK(shako.maxSockets == 2);
}

TEST_CASE("a base is the tier its upgrade columns leave it") {
	// A base names the tiers above it, and names itself in the column of the
	// tier it already is.
	const Base& cap = Resolve("cap");
	CHECK(cap.tier == ItemDescription::TierNormal);
	CHECK(cap.exceptional == "xap");
	CHECK(cap.elite == "uap");

	const Base& serpentskin = Resolve("xea");
	CHECK(serpentskin.tier == ItemDescription::TierExceptional);
	CHECK(serpentskin.exceptional == "");
	CHECK(serpentskin.elite == "uea");

	const Base& shako = Resolve("uap");
	CHECK(shako.tier == ItemDescription::TierElite);
	CHECK(shako.exceptional == "xap");
	CHECK(shako.elite == "");

	// Misc.txt has no upgrade columns at all, which leaves everything in it
	// normal with nothing above it.
	const Base& amulet = Resolve("amu");
	CHECK(amulet.tier == ItemDescription::TierNormal);
	CHECK(amulet.exceptional == "");
	CHECK(amulet.elite == "");
}

TEST_CASE("a weapon carries damage in each way it can be swung") {
	const Base& crystalSword = Resolve("crs");
	CHECK(crystalSword.weapon);
	CHECK(crystalSword.oneHandDamage.low == 5);
	CHECK(crystalSword.oneHandDamage.high == 15);
	CHECK_FALSE(crystalSword.twoHandDamage.Any());
	CHECK_FALSE(crystalSword.throwDamage.Any());

	// Two handed and held in either hand, so it carries both lines.
	const Base& colossusBlade = Resolve("7gd");
	CHECK(colossusBlade.oneHandDamage.low == 25);
	CHECK(colossusBlade.oneHandDamage.high == 65);
	CHECK(colossusBlade.twoHandDamage.low == 58);
	CHECK(colossusBlade.twoHandDamage.high == 115);

	// Two handed only, so the one handed line is not its to carry.
	const Base& colossusVoulge = Resolve("7vo");
	CHECK_FALSE(colossusVoulge.oneHandDamage.Any());
	CHECK(colossusVoulge.twoHandDamage.low == 17);
	CHECK(colossusVoulge.twoHandDamage.high == 165);

	const Base& wingedKnife = Resolve("7bk");
	CHECK(wingedKnife.throwDamage.low == 23);
	CHECK(wingedKnife.throwDamage.high == 39);
	CHECK(wingedKnife.oneHandDamage.low == 27);
}

TEST_CASE("a base carries only the durability the game shows") {
	CHECK(Resolve("crs").durability == 20);
	// A bow's durability is real and never printed.
	CHECK(Resolve("6lw").durability == 0);
	// A throwing weapon gives the line over to its stack.
	CHECK(Resolve("7bk").durability == 0);
	// And a phase blade has none to begin with.
	CHECK(Resolve("7cr").durability == 0);
}

TEST_CASE("a weapon carries the speed the tables give it") {
	CHECK(Resolve("7cr").speed == -30);
	CHECK(Resolve("7gd").speed == 5);
	// Nothing that is not a weapon has a speed at all.
	CHECK(Resolve("uap").speed == 0);
}

TEST_CASE("a base is named as the string table names it") {
	// Misc.txt calls this one "Charm Small", and only the string table gets it
	// the way the player reads it.
	CHECK(Resolve("cm1").name == "Small Charm");
	CHECK(Resolve("r33").name == "Zod Rune");
	CHECK(ItemDescription::BaseName("amu") == "Amulet");
	// A code the tables do not carry falls back to itself.
	CHECK(ItemDescription::BaseName("zzz") == "zzz");
}

TEST_CASE("every base the tables carry is listed in the order they are read") {
	TableFixture::Load();
	const std::vector<const Base*>& bases = ItemDescription::AllBases();
	// The bases the fixtures carry, in the order make_fixtures.py names them:
	// six weapons, then four pieces of armour, then three of the miscellany.
	// Adding one to SUBJECT_TABLES moves these, which is worth being told about.
	REQUIRE(bases.size() == 13);
	CHECK(bases[0]->code == "crs");
	CHECK(bases[6]->code == "cap");
	CHECK(bases[10]->code == "amu");
}

TEST_CASE("armour reads as a panel of its own") {
	CheckPanel("uap", {
		"Shako",
		"Defense: 98 to 141",
		"Durability: 12",
		"Sockets: 1 to 2",
		"Required Strength: 50",
		"Required Level: 43",
	});
}

TEST_CASE("a weapon held in either hand reads as a panel of its own") {
	CheckPanel("7gd", {
		"Colossus Blade",
		"One-Hand Damage: 25 to 65",
		"Two-Hand Damage: 58 to 115",
		"Durability: 50",
		"Attack Speed: [5]",
		"Sockets: 1 to 6",
		"Required Dexterity: 110",
		"Required Strength: 189",
		"Required Level: 63",
	});
}

TEST_CASE("a thrown weapon reads as a panel of its own") {
	CheckPanel("7bk", {
		"Winged Knife",
		"Throw Damage: 23 to 39",
		"One-Hand Damage: 27 to 35",
		"Attack Speed: [-20]",
		"Required Dexterity: 142",
		"Required Strength: 45",
		"Required Level: 57",
	});
}

TEST_CASE("a base with no numbers of its own reads as its name alone") {
	CheckPanel("amu", { "Amulet" });
}

TEST_CASE("an item's own bonuses are folded into the numbers its base carries") {
	TableFixture::Load();
	ItemDescription::Modifiers modifiers;
	modifiers.defensePercent = ItemDescription::Range(150, 200);
	modifiers.requirementPercent = ItemDescription::Range(-20, -20);

	Description item;
	item.AddBase("uap", White, modifiers);

	REQUIRE(item.attributes.size() == 2);
	// The game adds the percentage on as a share of the base and truncates that
	// share toward zero, so 98 and a half of itself is 245.
	CHECK(item.attributes[0] == "Defense: 245 to 423");
	CHECK(item.attributes[1] == "Durability: 12");
	CHECK(item.requirements.strength.low == 40);
	CHECK(item.requirements.strength.high == 40);
	// A requirement modifier leaves the level where it is.
	CHECK(item.requirements.level == 43);
}
