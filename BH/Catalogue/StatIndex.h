#pragma once
#include <string>
#include <vector>
#include "../StatDescriptions.h"
#include "Source.h"

/*
 * Every source of every catalogue, keyed by what it can grant, so that what
 * grants a stat can be asked without knowing which catalogue the answer is in.
 *
 * The index knows nothing about uniques, set items, runewords or affixes. A
 * catalogue registers one entry per source and names its own kind; adding a
 * catalogue is registration and nothing else. Catalogues.h is where the list of
 * them lives, so that this file stays ignorant of all of them.
 *
 * A question is a Query: a list of criteria, all of which must be satisfied.
 * Criteria are structured rather than written as text, so that a condition
 * builder can offer a stat, a comparator and a value as rows without a syntax
 * having to be parsed.
 *
 * A stat criterion is answered on the roll most favourable to it, which for
 * "more than" is the best roll. The question a player is asking is what could
 * grant them a stat, so hiding a source because its worst roll falls short is
 * the answer being wrong. Every result carries the range the source rolls, so
 * that a possible amount is not read as a guaranteed one.
 *
 * A criterion is answered from the stat totals a source's properties add up to,
 * and some of what a source grants is outside those totals on purpose: amounts
 * granted per character level, poison damage, and properties the tables give no
 * stat to beyond the handful StatDescriptions spells out by name. Which roll
 * answers each comparator is a decision too. docs/adr/0005 is where all of it
 * is written down and reasoned about.
 */
namespace StatIndex {

	// One source as the index holds it. The source is what a result hands back
	// so that a caller can read the whole record: its lines, its base, its
	// rarity.
	struct Entry {
		// What the catalogue that registered it calls itself, which is also
		// what a query scopes to. The index never reads it for anything but
		// that comparison.
		std::string kind;

		// What a text criterion is matched against, lowercased. Supplied at
		// registration rather than worked out here, since which of a source's
		// words are worth searching is not a question the index can answer for
		// a kind it knows nothing about.
		std::string searchKey;

		std::vector<StatDescriptions::StatTotal> totals;
		const Catalogue::Source* source;

		Entry() : source(NULL) {};
	};

	// How a stat criterion compares what a source can roll against the value it
	// was asked with. The same three the item filter's conditions offer, so that
	// a builder showing both does not have two vocabularies.
	enum Comparator {
		GreaterThan,
		LessThan,
		EqualTo
	};

	// One question asked of a source: a stat with a comparator and a value, or
	// a piece of text matched against the search key.
	struct Criterion {
		std::string stat;	// empty for a text criterion
		Comparator comparator;
		int value;
		std::string text;	// empty for a stat criterion

		Criterion() : comparator(GreaterThan), value(0) {};

		static Criterion OnStat(const std::string& stat, Comparator comparator,
				int value);

		// Matched case insensitively, so a player types what they read. Empty
		// text is carried by every search key, which is a search box with
		// nothing typed in it answering with the whole list.
		static Criterion OnText(const std::string& text);
	};

	// A question asked of the index. Every criterion must be satisfied; a query
	// carrying none answers with every source in scope, which is a panel
	// showing its whole list.
	struct Query {
		// The one kind to answer from, or empty for every kind.
		std::string kind;

		std::vector<Criterion> criteria;
	};

	// The range one stat rolls on the source a result names.
	struct Range {
		std::string stat;
		int low;
		int high;

		Range() : low(0), high(0) {};
	};

	// One source that answered a query.
	struct Result {
		const Entry* entry;

		// The range each of the query's stat criteria rolls on this source, in
		// the order the criteria were asked. Text criteria contribute nothing,
		// so a query asking only for text answers with none.
		std::vector<Range> ranges;

		Result() : entry(NULL) {};
	};

	// Adds one source under the kind the catalogue registering it calls itself.
	// The totals are worked out here, since what a source can grant is the one
	// thing the index does read.
	//
	// The source is held by address and is not copied, so it has to outlive the
	// index. A catalogue's sources are built once and kept, which is what makes
	// that true.
	void Register(const std::string& kind, const std::string& searchKey,
			const Catalogue::Source& source);

	// Every source that satisfies the query, in the order they were registered,
	// which is the order the catalogues list them in. The entry a result names
	// is valid until something else is registered.
	std::vector<Result> Find(const Query& query);

}
