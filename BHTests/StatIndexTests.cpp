#include "doctest.h"
#include <string>
#include <vector>
#include "Catalogue/Catalogues.h"
#include "Catalogue/StatIndex.h"
#include "Catalogue/UniqueCatalogue.h"
#include "TableFixture.h"

/*
 * What the stat index answers, stood up from fixtures with no game running.
 *
 * The index is the seam the whole catalogue design is tested through: a source
 * missing from it fails the query that should have found it, so registration
 * needs no test of its own, and a result's handle back to its source is how the
 * record building and the wording are reached.
 *
 * Queries are asked and answers are asserted. Nothing here reaches for how the
 * index stores its entries or when it built them.
 */

using StatIndex::Criterion;
using StatIndex::Query;
using StatIndex::Result;

namespace {

// The fixture holds four uniques: Guardian Angel, Harlequin Crest, Mara's
// Kaleidoscope and Skin of the Vipermagi, in that order.
Query Ask(const std::vector<Criterion>& criteria) {
	TableFixture::Load();
	Catalogue::Load();
	REQUIRE(Catalogue::Loaded());
	Query query;
	query.criteria = criteria;
	return query;
}

std::vector<std::string> Names(const std::vector<Result>& results) {
	std::vector<std::string> names;
	for (unsigned int i = 0; i < results.size(); i++)
		names.push_back(results[i].entry->source->name);
	return names;
}

std::vector<std::string> Answers(const std::vector<Criterion>& criteria) {
	return Names(StatIndex::Find(Ask(criteria)));
}

}  // namespace

TEST_CASE("every source of every catalogue is in the index") {
	CHECK(Answers({}) == std::vector<std::string>({
		"Guardian Angel",
		"Harlequin Crest",
		"Mara's Kaleidoscope",
		"Skin of the Vipermagi",
	}));
}

TEST_CASE("a stat criterion is answered on the best roll") {
	// Vipermagi rolls twenty to thirty five, Mara's twenty to thirty. Only the
	// one that can reach thirty five answers.
	CHECK(Answers({ Criterion::OnStat("fireresist", StatIndex::GreaterThan, 30) }) ==
		std::vector<std::string>({ "Skin of the Vipermagi" }));

	// Both can reach twenty five, and Guardian Angel raises maximum resistances
	// rather than resistances, which is a different stat.
	CHECK(Answers({ Criterion::OnStat("fireresist", StatIndex::GreaterThan, 25) }) ==
		std::vector<std::string>({ "Mara's Kaleidoscope", "Skin of the Vipermagi" }));
}

TEST_CASE("a result reports the range the source rolls") {
	std::vector<Result> results = StatIndex::Find(
		Ask({ Criterion::OnStat("fireresist", StatIndex::GreaterThan, 30) }));
	REQUIRE(results.size() == 1);
	REQUIRE(results[0].ranges.size() == 1);
	CHECK(results[0].ranges[0].stat == "fireresist");
	CHECK(results[0].ranges[0].low == 20);
	CHECK(results[0].ranges[0].high == 35);
}

TEST_CASE("a stat a source does not grant answers nothing") {
	// No source in the fixture converts damage to mana, and a comparator a zero
	// would satisfy must not have a stat nobody grants answer it.
	CHECK(Answers({ Criterion::OnStat("item_damagetomana", StatIndex::LessThan, 5) }).empty());
	CHECK(Answers({ Criterion::OnStat("nosuchstat", StatIndex::EqualTo, 0) }).empty());
}

TEST_CASE("less than and equal to are answered on the roll that satisfies them") {
	// Vipermagi's twenty to thirty five can roll under twenty five, and so can
	// Mara's twenty to thirty.
	CHECK(Answers({ Criterion::OnStat("fireresist", StatIndex::LessThan, 25) }) ==
		std::vector<std::string>({ "Mara's Kaleidoscope", "Skin of the Vipermagi" }));

	// A value inside the range is one the source can roll exactly.
	CHECK(Answers({ Criterion::OnStat("fireresist", StatIndex::EqualTo, 32) }) ==
		std::vector<std::string>({ "Skin of the Vipermagi" }));
	CHECK(Answers({ Criterion::OnStat("item_allskills", StatIndex::EqualTo, 2) }) ==
		std::vector<std::string>({ "Harlequin Crest", "Mara's Kaleidoscope" }));
}

TEST_CASE("a text criterion is matched against the search key") {
	// The base under it, which the unique's own name never says.
	CHECK(Answers({ Criterion::OnText("Shako") }) ==
		std::vector<std::string>({ "Harlequin Crest" }));

	// The kind of thing that base is.
	CHECK(Answers({ Criterion::OnText("amulet") }) ==
		std::vector<std::string>({ "Mara's Kaleidoscope" }));

	// Typed in whatever case it comes to hand in.
	CHECK(Answers({ Criterion::OnText("VIPERMAGI") }) ==
		std::vector<std::string>({ "Skin of the Vipermagi" }));

	CHECK(Answers({ Criterion::OnText("nothing named this") }).empty());

	// A text criterion asks for no stat, so there is no range to report.
	std::vector<Result> results = StatIndex::Find(Ask({ Criterion::OnText("Shako") }));
	REQUIRE(results.size() == 1);
	CHECK(results[0].ranges.empty());
}

TEST_CASE("several criteria must all be satisfied") {
	CHECK(Answers({
		Criterion::OnStat("item_allskills", StatIndex::GreaterThan, 1),
		Criterion::OnStat("strength", StatIndex::GreaterThan, 1),
	}) == std::vector<std::string>({ "Harlequin Crest", "Mara's Kaleidoscope" }));

	// A stat and a piece of text together.
	CHECK(Answers({
		Criterion::OnStat("item_allskills", StatIndex::GreaterThan, 1),
		Criterion::OnText("crest"),
	}) == std::vector<std::string>({ "Harlequin Crest" }));

	// One criterion no source satisfies leaves nothing.
	CHECK(Answers({
		Criterion::OnText("crest"),
		Criterion::OnStat("fireresist", StatIndex::GreaterThan, 1),
	}).empty());

	// Each stat criterion reports its own range, in the order asked.
	std::vector<Result> results = StatIndex::Find(Ask({
		Criterion::OnStat("strength", StatIndex::GreaterThan, 1),
		Criterion::OnText("crest"),
		Criterion::OnStat("item_magicbonus", StatIndex::GreaterThan, 1),
	}));
	REQUIRE(results.size() == 1);
	REQUIRE(results[0].ranges.size() == 2);
	CHECK(results[0].ranges[0].stat == "strength");
	CHECK(results[0].ranges[0].low == 2);
	CHECK(results[0].ranges[1].stat == "item_magicbonus");
	CHECK(results[0].ranges[1].low == 50);
	CHECK(results[0].ranges[1].high == 50);
}

TEST_CASE("a query can be scoped to one kind") {
	Query query = Ask({});
	query.kind = UniqueCatalogue::Kind;
	CHECK(StatIndex::Find(query).size() == 4);

	// A kind no catalogue registers, which is what every catalogue not yet
	// written looks like.
	query.kind = "runeword";
	CHECK(StatIndex::Find(query).empty());
}

TEST_CASE("an entry carries what the index was registered with") {
	std::vector<Result> results = StatIndex::Find(Ask({ Criterion::OnText("Shako") }));
	REQUIRE(results.size() == 1);
	const StatIndex::Entry& entry = *results[0].entry;

	CHECK(entry.kind == UniqueCatalogue::Kind);
	CHECK(entry.searchKey == "harlequin crest shako helm");

	// The handle back to the full record, which is how a result reaches the
	// lines a player reads and the base the source is made on.
	CHECK(entry.source == UniqueCatalogue::Find("Harlequin Crest"));
	CHECK(entry.source->baseName == "Shako");
	CHECK(entry.source->rarity == RarityUnique);
	CHECK(entry.source->lines[0] == "+2 to All Skills");
}

// The totals mechanism leaves these out on purpose; see docs/adr/0005.
TEST_CASE("what the totals leave out cannot be asked for") {
	SUBCASE("an amount granted per character level") {
		// Harlequin Crest grants one and a half life a level and nothing flat,
		// so no criterion on life finds it however low the value asked for.
		CHECK(Answers({ Criterion::OnStat("maxhp", StatIndex::GreaterThan, 0) }).empty());

		// Not under the stat it is granted per level of either: the eighths it
		// is held in are not an amount a criterion could be compared against.
		CHECK(Answers({
			Criterion::OnStat("item_tohit_demon_perlevel", StatIndex::GreaterThan, 0)
		}).empty());
	}

	// What the tables do name a stat for is reachable, which is what makes the
	// two cases above exclusions rather than the index seeing nothing.
	SUBCASE("a stat the tables name is reachable") {
		CHECK(Answers({
			Criterion::OnStat("item_fasterblockrate", StatIndex::GreaterThan, 0)
		}) == std::vector<std::string>({ "Guardian Angel" }));
	}
}
