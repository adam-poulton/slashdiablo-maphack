#include "doctest.h"
#include <string>
#include "TableFixture.h"
#include "TableReader.h"

/*
 * The lookups that find a property row by its code and a stat cost row by its
 * stat without walking the table.
 *
 * A lookup only earns its place if it answers exactly what a walk of the same
 * table would have answered, so that is what most of this asserts: every row of
 * both tables, found both ways, has to come back as the same row. The rest is
 * about the tables a realm ships rather than the ones the game does, since a
 * lookup built from a table with rows the game itself would never reach has to
 * keep them just as unreachable.
 */

namespace {

const char* kRealmProperties = "BHTests/fixtures/tables/RealmProperties.txt";

// The same question asked as a walk, which is what the lookup has to agree with.
JSONObject* Walk(Table& table, const std::string& field,
		const std::string& value) {
	return table.findEntry([&](JSONObject* row) -> bool {
		return row->getString(field).compare(value) == 0;
	});
}

}  // namespace

TEST_CASE("every property is found by its code") {
	TableFixture::Load();
	for (int i = 0; i < Tables::Properties.size(); i++) {
		std::string code = Tables::Properties.entryAt(i)->getString("code");
		CHECK(Tables::Properties.findEntry("code", code) ==
			Walk(Tables::Properties, "code", code));
	}
}

TEST_CASE("every stat cost row is found by its stat") {
	TableFixture::Load();
	for (int i = 0; i < Tables::ItemStatCost.size(); i++) {
		std::string stat = Tables::ItemStatCost.entryAt(i)->getString("Stat");
		CHECK(Tables::ItemStatCost.findEntry("Stat", stat) ==
			Walk(Tables::ItemStatCost, "Stat", stat));
	}
}

TEST_CASE("a code no table row carries is not found") {
	TableFixture::Load();
	CHECK(Tables::Properties.findEntry("code", "no-such-property") == NULL);
	CHECK(Tables::ItemStatCost.findEntry("Stat", "no-such-stat") == NULL);
}

TEST_CASE("a table a realm has modified reads the same either way") {
	Table properties(kRealmProperties);
	REQUIRE(properties.size() == 5);
	properties.lookupBy("code");

	SUBCASE("a code the game does not ship is found") {
		JSONObject* row = properties.findEntry("code", "realm-regen");
		REQUIRE(row != NULL);
		CHECK(row->getString("stat1") == "hpregen");
	}

	SUBCASE("a divider row the game would ignore is still a row") {
		CHECK(properties.findEntry("code", "Expansion") ==
			Walk(properties, "code", "Expansion"));
		CHECK(properties.findEntry("code", "Expansion") != NULL);
	}

	SUBCASE("a second row with a code already used stays out of reach") {
		JSONObject* row = properties.findEntry("code", "ac");
		REQUIRE(row != NULL);
		CHECK(row->getString("stat1") == "armorclass");
		CHECK(row == Walk(properties, "code", "ac"));
	}

	SUBCASE("a row with no code of its own is found by no code") {
		JSONObject* row = properties.findEntry("code", "");
		REQUIRE(row != NULL);
		CHECK(row->getString("stat1") == "maxhp");
		CHECK(row == Walk(properties, "code", ""));
	}
}
