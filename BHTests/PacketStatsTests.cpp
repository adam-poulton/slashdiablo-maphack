#include "doctest.h"
#include <string>
#include <vector>
#include "../BH/Constants.h"
#include "ItemFacts.h"
#include "ItemFactsPacket.h"

/*
 * The stats an item from a packet carries, asked for the two ways the
 * conditions ask for them.
 *
 * Adding a stat up is not simply summing every entry that names it. Several
 * stats carry something alongside the value and only the entries matching it
 * count: which skill, which class, which of a class's skill tabs. Getting that
 * wrong reads as an item quietly failing a rule it should have matched, which
 * is not the kind of thing anyone notices.
 */

using ItemFactsPacket::Defense;
using ItemFactsPacket::PacketStats;

namespace {

ItemProperty Property(unsigned int stat, long value) {
	ItemProperty property = {};
	property.stat = stat;
	property.value = value;
	return property;
}

}  // namespace

TEST_CASE("a stat with nothing beside it adds up every entry naming it") {
	ItemFacts facts = {};
	facts.properties.push_back(Property(STAT_STRENGTH, 10));
	facts.properties.push_back(Property(STAT_STRENGTH, 5));
	facts.properties.push_back(Property(STAT_DEXTERITY, 7));

	PacketStats stats(facts);
	CHECK(stats.Stat(STAT_STRENGTH, 0) == 15);
	CHECK(stats.Stat(STAT_DEXTERITY, 0) == 7);
}

TEST_CASE("a stat the item does not carry is nothing rather than missing") {
	ItemFacts facts = {};
	PacketStats stats(facts);

	CHECK(stats.Stat(STAT_STRENGTH, 0) == 0);
	CHECK(stats.Stats().empty());
}

TEST_CASE("a skill stat counts only the entries for the skill asked about") {
	ItemFacts facts = {};
	ItemProperty first = Property(STAT_SINGLESKILL, 3);
	first.skill = 54;
	ItemProperty second = Property(STAT_SINGLESKILL, 2);
	second.skill = 99;
	facts.properties.push_back(first);
	facts.properties.push_back(second);

	PacketStats stats(facts);
	CHECK(stats.Stat(STAT_SINGLESKILL, 54) == 3);
	CHECK(stats.Stat(STAT_SINGLESKILL, 99) == 2);
	CHECK(stats.Stat(STAT_SINGLESKILL, 1) == 0);
}

TEST_CASE("a class skill stat counts only the entries for that class") {
	ItemFacts facts = {};
	ItemProperty sorceress = Property(STAT_CLASSSKILLS, 2);
	sorceress.characterClass = 1;
	ItemProperty necromancer = Property(STAT_CLASSSKILLS, 1);
	necromancer.characterClass = 2;
	facts.properties.push_back(sorceress);
	facts.properties.push_back(necromancer);

	PacketStats stats(facts);
	CHECK(stats.Stat(STAT_CLASSSKILLS, 1) == 2);
	CHECK(stats.Stat(STAT_CLASSSKILLS, 2) == 1);
}

TEST_CASE("a skill tab is a class and one of its eight tabs together") {
	ItemFacts facts = {};
	ItemProperty tab = Property(STAT_SKILLTAB, 3);
	tab.characterClass = 1;
	tab.tab = 2;
	facts.properties.push_back(tab);

	PacketStats stats(facts);
	CHECK(stats.Stat(STAT_SKILLTAB, 1 * 8 + 2) == 3);
	// The same tab number under a different class is a different thing.
	CHECK(stats.Stat(STAT_SKILLTAB, 2 * 8 + 2) == 0);
	CHECK(stats.Stat(STAT_SKILLTAB, 1 * 8 + 3) == 0);
}

TEST_CASE("sockets are a field of the item rather than one of its properties") {
	ItemFacts facts = {};
	facts.sockets = 4;

	PacketStats stats(facts);
	CHECK(stats.Stat(STAT_SOCKETS, 0) == 4);
}

TEST_CASE("defence is what the item has once enhanced defence is applied") {
	ItemFacts facts = {};
	facts.defense = 100;

	CHECK(Defense(facts) == 100);

	facts.properties.push_back(Property(STAT_ENHANCEDDEFENSE, 50));
	CHECK(Defense(facts) == 150);

	// A second one applies to what the first arrived at, as the game does it.
	facts.properties.push_back(Property(STAT_ENHANCEDDEFENSE, 100));
	CHECK(Defense(facts) == 300);
}

TEST_CASE("asking for defence as a stat gives the enhanced figure") {
	ItemFacts facts = {};
	facts.defense = 200;
	facts.properties.push_back(Property(STAT_ENHANCEDDEFENSE, 25));

	PacketStats stats(facts);
	CHECK(stats.Stat(STAT_DEFENSE, 0) == 250);
}

TEST_CASE("the entries put a charged skill back the way the game packs it") {
	ItemFacts facts = {};
	ItemProperty charged = Property(STAT_CHARGED, 40);
	charged.skill = 54;
	charged.level = 12;
	facts.properties.push_back(charged);

	PacketStats stats(facts);
	const std::vector<StatEntry>& entries = stats.Stats();
	REQUIRE(entries.size() == 1);
	CHECK(entries[0].stat == STAT_CHARGED);
	// The skill sits above the low six bits, the level it is charged at in them,
	// which is how the condition reading this takes it apart.
	CHECK((entries[0].sub >> 6) == 54);
	CHECK((entries[0].sub & 0x3F) == 12);
}

TEST_CASE("the entries keep every property, in the order they were read") {
	ItemFacts facts = {};
	facts.properties.push_back(Property(STAT_STRENGTH, 10));
	facts.properties.push_back(Property(STAT_DEXTERITY, 20));
	facts.properties.push_back(Property(STAT_STRENGTH, 30));

	PacketStats stats(facts);
	const std::vector<StatEntry>& entries = stats.Stats();
	REQUIRE(entries.size() == 3);
	CHECK(entries[0].stat == STAT_STRENGTH);
	CHECK(entries[0].value == 10);
	CHECK(entries[1].stat == STAT_DEXTERITY);
	CHECK(entries[2].value == 30);
}
