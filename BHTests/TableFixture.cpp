#include "TableFixture.h"
#include "doctest.h"
#include <fstream>
#include <string>
#include "StatDescriptions.h"
#include "TableReader.h"

/*
 * Paths are relative to the working directory, which is the repository root
 * both in CI and when the tests are run by hand from there, the same as the
 * packet and filter replay fixtures.
 */

// TableReader resolves a table path against the directory the game is installed
// in. The fixtures are read from the repository root, so there is no prefix.
// Defined here rather than linked from BH.cpp, which needs the game to build.
namespace BH {
std::string path;
}

namespace {

const char* kFixtures = "BHTests/fixtures/";

void LoadTable(const std::string& name, Table& table) {
	table = Table(std::string(kFixtures) + "tables/" + name);
	REQUIRE_MESSAGE(table.size() > 0,
		"fixture not found, run the tests from the repository root: " << name);
}

}  // namespace

void TableFixture::Load() {
	static bool loaded = false;
	if (loaded)
		return;

	LoadTable("ItemStatCost.txt", Tables::ItemStatCost);
	LoadTable("Properties.txt", Tables::Properties);
	LoadTable("CharStats.txt", Tables::CharStats);
	LoadTable("Skills.txt", Tables::Skills);
	LoadTable("SkillDesc.txt", Tables::SkillDesc);
	LoadTable("ItemTypes.txt", Tables::ItemTypes);
	LoadTable("UniqueItems.txt", Tables::UniqueItems);
	LoadTable("Weapons.txt", Tables::Weapons);
	LoadTable("Armor.txt", Tables::Armor);
	LoadTable("Misc.txt", Tables::Misc);
	// The keying the game does once its archives are read, so the tests search
	// the tables the way it does.
	Tables::buildLookups();

	std::ifstream strings(std::string(kFixtures) + "strings.txt");
	REQUIRE_MESSAGE(strings.is_open(),
		"fixture not found, run the tests from the repository root: strings.txt");
	StatDescriptions::LoadStrings(strings);
	REQUIRE(StatDescriptions::IsInitialized());

	// Marked at the end, so a missing fixture is reported by every test that
	// wanted it rather than only by the first one to ask.
	loaded = true;
}
