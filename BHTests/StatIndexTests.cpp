#include "doctest.h"
#include <string>
#include <vector>
#include "Catalogue/Catalogues.h"
#include "Catalogue/RunewordCatalogue.h"
#include "Catalogue/SetCatalogue.h"
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

const char* const kUnique = UniqueCatalogue::Kind;
const char* const kRuneword = RunewordCatalogue::Kind;

// The fixture holds four uniques, the eight pieces of two sets, those two sets'
// own bonuses, and five runewords. A question about one catalogue is scoped to
// its kind, so that what it asserts does not turn on which other catalogues the
// fixture happens to carry.
Query Ask(const std::vector<Criterion>& criteria, const std::string& kind = "") {
	TableFixture::Load();
	Catalogue::Load();
	REQUIRE(Catalogue::Loaded());
	Query query;
	query.kind = kind;
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

std::vector<std::string> AnswersFrom(const std::string& kind,
		const std::vector<Criterion>& criteria) {
	return Names(StatIndex::Find(Ask(criteria, kind)));
}

}  // namespace

// In registration order, which is the order the catalogues are read in and,
// within each, the order it lists its sources.
TEST_CASE("every source of every catalogue is in the index") {
	// Every catalogue, one after another in the order Catalogues.cpp reads
	// them, and each in the order it lists its own sources.
	CHECK(Answers({}) == std::vector<std::string>({
		"Guardian Angel",
		"Harlequin Crest",
		"Mara's Kaleidoscope",
		"Skin of the Vipermagi",
		"Civerb's Ward",
		"Civerb's Icon",
		"Civerb's Cudgel",
		"Tal Rasha's Fine-Spun Cloth",
		"Tal Rasha's Adjudication",
		"Tal Rasha's Lidless Eye",
		"Tal Rasha's Guardianship",
		"Tal Rasha's Horadric Crest",
		"Civerb's Vestments",
		"Tal Rasha's Wrappings",
		"Enigma",
		"Lore",
		"Plague",
		"Spirit",
		"Splendor",
	}));
}

TEST_CASE("a stat criterion is answered on the best roll") {
	// Vipermagi rolls twenty to thirty five, Mara's twenty to thirty. Only the
	// one that can reach thirty five answers.
	CHECK(AnswersFrom(kUnique, { Criterion::OnStat("fireresist", StatIndex::GreaterThan, 30) }) ==
		std::vector<std::string>({ "Skin of the Vipermagi" }));

	// Both can reach twenty five, and Guardian Angel raises maximum resistances
	// rather than resistances, which is a different stat.
	CHECK(AnswersFrom(kUnique, { Criterion::OnStat("fireresist", StatIndex::GreaterThan, 25) }) ==
		std::vector<std::string>({ "Mara's Kaleidoscope", "Skin of the Vipermagi" }));
}

TEST_CASE("a result reports the range the source rolls") {
	std::vector<Result> results = StatIndex::Find(
		Ask({ Criterion::OnStat("fireresist", StatIndex::GreaterThan, 30) },
			kUnique));
	REQUIRE(results.size() == 1);
	REQUIRE(results[0].ranges.size() == 1);
	CHECK(results[0].ranges[0].stat == "fireresist");
	CHECK(results[0].ranges[0].low == 20);
	CHECK(results[0].ranges[0].high == 35);
}

TEST_CASE("a stat a source does not grant answers nothing") {
	// No source in the fixture converts damage to mana, and a comparator a zero
	// would satisfy must not have a stat nobody grants answer it.
	CHECK(AnswersFrom(kUnique, { Criterion::OnStat("item_damagetomana", StatIndex::LessThan, 5) }).empty());
	CHECK(AnswersFrom(kUnique, { Criterion::OnStat("nosuchstat", StatIndex::EqualTo, 0) }).empty());
}

TEST_CASE("less than and equal to are answered on the roll that satisfies them") {
	// Vipermagi's twenty to thirty five can roll under twenty five, and so can
	// Mara's twenty to thirty.
	CHECK(AnswersFrom(kUnique, { Criterion::OnStat("fireresist", StatIndex::LessThan, 25) }) ==
		std::vector<std::string>({ "Mara's Kaleidoscope", "Skin of the Vipermagi" }));

	// A value inside the range is one the source can roll exactly.
	CHECK(AnswersFrom(kUnique, { Criterion::OnStat("fireresist", StatIndex::EqualTo, 32) }) ==
		std::vector<std::string>({ "Skin of the Vipermagi" }));
	CHECK(AnswersFrom(kUnique, { Criterion::OnStat("item_allskills", StatIndex::EqualTo, 2) }) ==
		std::vector<std::string>({ "Harlequin Crest", "Mara's Kaleidoscope" }));
}

TEST_CASE("a text criterion is matched against the search key") {
	// The base under it, which the unique's own name never says.
	CHECK(AnswersFrom(kUnique, { Criterion::OnText("Shako") }) ==
		std::vector<std::string>({ "Harlequin Crest" }));

	// The kind of thing that base is.
	CHECK(AnswersFrom(kUnique, { Criterion::OnText("amulet") }) ==
		std::vector<std::string>({ "Mara's Kaleidoscope" }));

	// Typed in whatever case it comes to hand in.
	CHECK(AnswersFrom(kUnique, { Criterion::OnText("VIPERMAGI") }) ==
		std::vector<std::string>({ "Skin of the Vipermagi" }));

	CHECK(AnswersFrom(kUnique, { Criterion::OnText("nothing named this") }).empty());

	// A search box with nothing typed in it shows the whole list.
	CHECK(Answers({ Criterion::OnText("") }) == Answers({}));

	// A text criterion asks for no stat, so there is no range to report.
	std::vector<Result> results = StatIndex::Find(Ask({ Criterion::OnText("Shako") }));
	REQUIRE(results.size() == 1);
	CHECK(results[0].ranges.empty());
}

TEST_CASE("several criteria must all be satisfied") {
	CHECK(AnswersFrom(kUnique, {
		Criterion::OnStat("item_allskills", StatIndex::GreaterThan, 1),
		Criterion::OnStat("strength", StatIndex::GreaterThan, 1),
	}) == std::vector<std::string>({ "Harlequin Crest", "Mara's Kaleidoscope" }));

	// A stat and a piece of text together.
	CHECK(AnswersFrom(kUnique, {
		Criterion::OnStat("item_allskills", StatIndex::GreaterThan, 1),
		Criterion::OnText("crest"),
	}) == std::vector<std::string>({ "Harlequin Crest" }));

	// One criterion no source satisfies leaves nothing.
	CHECK(AnswersFrom(kUnique, {
		Criterion::OnText("crest"),
		Criterion::OnStat("fireresist", StatIndex::GreaterThan, 1),
	}).empty());

	// Each stat criterion reports its own range, in the order asked.
	std::vector<Result> results = StatIndex::Find(Ask({
		Criterion::OnStat("strength", StatIndex::GreaterThan, 1),
		Criterion::OnText("crest"),
		Criterion::OnStat("item_magicbonus", StatIndex::GreaterThan, 1),
	}, kUnique));
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

	// A set is two kinds of source: its pieces, and its own bonuses.
	query.kind = SetCatalogue::Kind;
	CHECK(StatIndex::Find(query).size() == 8);
	query.kind = SetCatalogue::BonusKind;
	CHECK(StatIndex::Find(query).size() == 2);

	query.kind = RunewordCatalogue::Kind;
	CHECK(StatIndex::Find(query).size() == 5);

	// A kind no catalogue registers, which is what every catalogue not yet
	// written looks like.
	query.kind = "affix";
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
		CHECK(AnswersFrom(kUnique,
			{ Criterion::OnStat("maxhp", StatIndex::GreaterThan, 0) }).empty());

		// Not under the stat it is granted per level of either: the eighths it
		// is held in are not an amount a criterion could be compared against.
		CHECK(AnswersFrom(kUnique, {
			Criterion::OnStat("item_tohit_demon_perlevel", StatIndex::GreaterThan, 0)
		}).empty());
	}

	// What the tables do name a stat for is reachable, which is what makes the
	// two cases above exclusions rather than the index seeing nothing.
	SUBCASE("a stat the tables name is reachable") {
		CHECK(AnswersFrom(kUnique, {
			Criterion::OnStat("item_fasterblockrate", StatIndex::GreaterThan, 0)
		}) == std::vector<std::string>({ "Guardian Angel" }));
	}
}

// User story 9: a stat that arrives only with four pieces worn is found by a
// search that never names a piece.
TEST_CASE("a set bonus answers for what its set grants") {
	const std::string bonus = SetCatalogue::BonusKind;
	const std::string piece = SetCatalogue::Kind;

	SUBCASE("including a stat none of its pieces grants") {
		// Wearing all three of Civerb's is the only way to have lightning
		// resistance out of that set, so the set is what answers for it.
		std::vector<Criterion> criteria = {
			Criterion::OnText("civerb"),
			Criterion::OnStat("lightresist", StatIndex::GreaterThan, 20),
		};
		CHECK(AnswersFrom(bonus, criteria) ==
			std::vector<std::string>({ "Civerb's Vestments" }));
		CHECK(AnswersFrom(piece, criteria).empty());
	}

	SUBCASE("and a bonus that arrives before the set is complete") {
		CHECK(AnswersFrom(bonus, {
			Criterion::OnStat("item_fastergethitrate", StatIndex::GreaterThan, 20)
		}) == std::vector<std::string>({ "Tal Rasha's Wrappings" }));
	}
}

TEST_CASE("a piece answers for what more of its set unlocks on it") {
	// Civerb's Icon grants cold resistance only once a second piece is worn,
	// and a search asking for it has to find the piece that grants it.
	CHECK(AnswersFrom(SetCatalogue::Kind, {
		Criterion::OnText("civerb's icon"),
		Criterion::OnStat("coldresist", StatIndex::GreaterThan, 20),
	}) == std::vector<std::string>({ "Civerb's Icon" }));
}

TEST_CASE("a piece is found by the set it belongs to") {
	// Its set's name, which four of the five pieces' own names do not carry.
	CHECK(AnswersFrom(SetCatalogue::Kind, { Criterion::OnText("wrappings") }) ==
		std::vector<std::string>({
			"Tal Rasha's Fine-Spun Cloth",
			"Tal Rasha's Adjudication",
			"Tal Rasha's Lidless Eye",
			"Tal Rasha's Guardianship",
			"Tal Rasha's Horadric Crest",
		}));

	std::vector<Result> results = StatIndex::Find(
		Ask({ Criterion::OnText("civerb's ward") }, SetCatalogue::Kind));
	REQUIRE(results.size() == 1);
	CHECK(results[0].entry->searchKey ==
		"civerb's ward large shield shield civerb's vestments");
	CHECK(results[0].entry->source->setName == "Civerb's Vestments");
}

TEST_CASE("a source made on several kinds of base is answered by the kind that satisfies") {
	// Spirit's runes give cold resistance only in a shield and steal life only
	// in a weapon. Both are what it could grant, so a criterion finds it under
	// either.
	CHECK(AnswersFrom(kRuneword,
		{ Criterion::OnStat("coldresist", StatIndex::GreaterThan, 30) }) ==
		std::vector<std::string>({ "Spirit" }));
	CHECK(AnswersFrom(kRuneword,
		{ Criterion::OnStat("lifedrainmindam", StatIndex::GreaterThan, 5) }) ==
		std::vector<std::string>({ "Spirit" }));

	// And the range reported is the one that kind of base rolls rather than
	// anything the kinds were read across.
	std::vector<Result> results = StatIndex::Find(
		Ask({ Criterion::OnStat("coldresist", StatIndex::GreaterThan, 30) },
			kRuneword));
	REQUIRE(results.size() == 1);
	REQUIRE(results[0].ranges.size() == 1);
	CHECK(results[0].ranges[0].low == 35);
	CHECK(results[0].ranges[0].high == 35);

	// A stat the runeword grants itself is the same in every kind of base, and
	// is reported once rather than once a kind.
	results = StatIndex::Find(
		Ask({ Criterion::OnStat("maxmana", StatIndex::GreaterThan, 100) },
			kRuneword));
	REQUIRE(results.size() == 1);
	CHECK(results[0].entry->source->name == "Spirit");
	REQUIRE(results[0].ranges.size() == 1);
	CHECK(results[0].ranges[0].low == 89);
	CHECK(results[0].ranges[0].high == 112);
}

TEST_CASE("a runeword is reached by the runes it is made from") {
	// Neither the name nor the kind of base says so, which is what puts the
	// runes in the search key.
	CHECK(AnswersFrom(kRuneword, { Criterion::OnText("jah") }) ==
		std::vector<std::string>({ "Enigma" }));
	CHECK(AnswersFrom(kRuneword, { Criterion::OnText("any shield") }) ==
		std::vector<std::string>({ "Spirit", "Splendor" }));
}
