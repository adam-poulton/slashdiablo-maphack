#include "doctest.h"
#include <string>
#include <vector>
#include "Catalogue/RunewordCatalogue.h"
#include "Catalogue/SetCatalogue.h"
#include "Catalogue/UniqueCatalogue.h"
#include "Constants.h"
#include "FilterContext.h"
#include "ItemFacts.h"
#include "ItemFactsCatalogue.h"
#include "ItemTables.h"
#include "RuleFixture.h"
#include "TableFixture.h"

/*
 * A catalogue source walked against the real rules, with no game running.
 *
 * The bridge is asserted through the seam it exists for rather than field by
 * field: a rule is written the way a filter line is written and walked against
 * the item, and what comes back is the verdict. A field that is filled in
 * wrongly fails a rule that reads it, which is the failure a filter builder's
 * preview would show a player.
 *
 * The item facts themselves are asserted only where no condition reads them,
 * which is what an absent fact amounts to.
 */

using Catalogue::Source;
using ItemFactsCatalogue::BestRoll;
using ItemFactsCatalogue::CatalogueItem;
using ItemFactsCatalogue::WorstRoll;

namespace {

const Source& Unique(const std::string& code) {
	TableFixture::Load();
	const Source* source = UniqueCatalogue::Find(code);
	REQUIRE_MESSAGE(source != NULL, "not in the fixture: " << code);
	return *source;
}

const Source& SetPiece(const std::string& code) {
	TableFixture::Load();
	const std::vector<Source>& pieces = SetCatalogue::Pieces();
	for (unsigned int i = 0; i < pieces.size(); i++) {
		if (pieces[i].code.compare(code) == 0)
			return pieces[i];
	}
	REQUIRE_MESSAGE(false, "not in the fixture: " << code);
	return pieces[0];
}

const Source& Runeword(const std::string& code) {
	TableFixture::Load();
	const Source* source = RunewordCatalogue::Find(code);
	REQUIRE_MESSAGE(source != NULL, "not in the fixture: " << code);
	return *source;
}

/*
 * The base item's attributes, as the map filled from the game's archives holds
 * them.
 *
 * Only the code is filled: no rule written below asks for one of the item
 * groups, a quality level or an affix level, which are the rest of what the
 * record carries. A test that writes such a rule fills in what it asks for.
 */
ItemAttributes Attributes(const std::string& code) {
	ItemAttributes attrs = {};
	for (int i = 0; i < 4; i++)
		attrs.code[i] = (i < (int)code.length()) ? code[i] : 0;
	return attrs;
}

// A world no rule below asks about, other than where one says the filter level.
FilterContext AWorld() {
	FilterContext context = {};
	return context;
}

// The names of the rules that matched, which is the whole of what a walk says
// about an item this small a rule set is judged against.
std::vector<std::string> Matched(const RuleList& list, const ItemFacts& facts,
		const FilterContext& context) {
	std::vector<const Action*> actions =
		MatchingActions(list.Rules(), facts, context, PING_LEVEL_ALL);
	std::vector<std::string> names;
	for (unsigned int i = 0; i < actions.size(); i++)
		names.push_back(actions[i]->name);
	return names;
}

}  // namespace

TEST_CASE("a rule that names a unique matches it as a catalogue item") {
	const Source& source = Unique("Harlequin Crest");
	ItemAttributes attrs = Attributes(source.baseCode);
	CatalogueItem item(source, &attrs);
	REQUIRE(item.Known());

	RuleList list;
	list.Add("uap UNI2", "the shako%CONTINUE%");
	list.Add("uap SET2", "a set shako%CONTINUE%");
	list.Add("amu UNI2", "an amulet%CONTINUE%");

	// The base code and the unique's own number both have to be right for the
	// first rule and wrong for the other two.
	CHECK(Matched(list, item.Facts(), AWorld()) ==
		std::vector<std::string>({ "the shako" }));
}

TEST_CASE("a rule that names a set item matches it as a catalogue item") {
	const Source& source = SetPiece("Tal Rasha's Adjudication");
	ItemAttributes attrs = Attributes(source.baseCode);
	CatalogueItem item(source, &attrs);
	REQUIRE(item.Known());

	RuleList list;
	list.Add("amu SET4", "the amulet%CONTINUE%");
	list.Add("amu UNI4", "a unique amulet%CONTINUE%");

	CHECK(Matched(list, item.Facts(), AWorld()) ==
		std::vector<std::string>({ "the amulet" }));
}

TEST_CASE("a stat is answered at the roll the item was said at") {
	// Vipermagi rolls twenty to thirty five of every resistance.
	const Source& source = Unique("Skin of the Vipermagi");
	ItemAttributes attrs = Attributes(source.baseCode);

	RuleList list;
	list.Add("xea FRES>30", "well rolled%CONTINUE%");

	SUBCASE("the best roll") {
		CatalogueItem item(source, &attrs, BestRoll);
		CHECK(Matched(list, item.Facts(), AWorld()) ==
			std::vector<std::string>({ "well rolled" }));
	}

	SUBCASE("the worst roll") {
		CatalogueItem item(source, &attrs, WorstRoll);
		CHECK(Matched(list, item.Facts(), AWorld()).empty());
	}
}

TEST_CASE("defence is the base's own under what the source adds to it") {
	// Guardian Angel is a Templar Coat, 252 to 274 defence, with 180 to 200
	// percent enhanced defence of its own: 705 at worst and 822 at best.
	const Source& source = Unique("Guardian Angel");
	ItemAttributes attrs = Attributes(source.baseCode);

	RuleList list;
	list.Add("xlt DEF>800", "high defence%CONTINUE%");

	SUBCASE("the best roll") {
		CatalogueItem item(source, &attrs, BestRoll);
		CHECK(item.Facts().stats->Stat(STAT_DEFENSE, 0) == 822);
		CHECK(Matched(list, item.Facts(), AWorld()) ==
			std::vector<std::string>({ "high defence" }));
	}

	SUBCASE("the worst roll") {
		CatalogueItem item(source, &attrs, WorstRoll);
		CHECK(item.Facts().stats->Stat(STAT_DEFENSE, 0) == 705);
		CHECK(Matched(list, item.Facts(), AWorld()).empty());
	}
}

TEST_CASE("what only an item that exists can answer is absent") {
	const Source& source = Unique("Harlequin Crest");
	ItemAttributes attrs = Attributes(source.baseCode);
	CatalogueItem item(source, &attrs);

	// The same answer a packet item gives, and for the same reason: neither
	// exists, so neither has a price or a level to be used at.
	CHECK(item.Facts().liveOnly == NULL);

	RuleList list;
	list.Add("uap PRICE>1", "worth something%CONTINUE%");
	list.Add("uap", "a shako%CONTINUE%");

	// ADR 0002: the rule asking is not judged at all, rather than its condition
	// being answered false, so that negating it does not turn not knowing into
	// a match.
	CHECK(Matched(list, item.Facts(), AWorld()) ==
		std::vector<std::string>({ "a shako" }));
	list.Add("uap !PRICE>1", "not worth anything%CONTINUE%");
	CHECK(Matched(list, item.Facts(), AWorld()) ==
		std::vector<std::string>({ "a shako" }));
}

TEST_CASE("the same catalogue item is judged differently at two filter levels") {
	const Source& source = Unique("Harlequin Crest");
	ItemAttributes attrs = Attributes(source.baseCode);
	CatalogueItem item(source, &attrs);

	// What a filter builder's preview is for: the rule that answers for an item
	// is not the same rule at every setting the player can choose.
	RuleList list;
	list.Add("uap FILTLVL>1", "hidden further up%CONTINUE%");
	list.Add("uap", "always shown%CONTINUE%");

	FilterContext showing = AWorld();
	showing.filterLevel = 0;
	CHECK(Matched(list, item.Facts(), showing) ==
		std::vector<std::string>({ "always shown" }));

	FilterContext hiding = AWorld();
	hiding.filterLevel = 2;
	CHECK(Matched(list, item.Facts(), hiding) ==
		std::vector<std::string>({ "hidden further up", "always shown" }));
}

TEST_CASE("a source that names no one base is not an item") {
	// A runeword is made in a range of bases and grants different things in
	// each, so there is no one item to say it as. Nothing here picks a base.
	const Source& source = Runeword("Runeword130");
	REQUIRE(source.baseCode.empty());

	CatalogueItem item(source, NULL);
	CHECK_FALSE(item.Known());
}

TEST_CASE("a base the tables do not describe is not an item") {
	const Source& source = Unique("Harlequin Crest");
	CatalogueItem item(source, NULL);
	CHECK_FALSE(item.Known());
}
