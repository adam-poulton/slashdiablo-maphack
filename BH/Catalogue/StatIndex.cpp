#include "StatIndex.h"
#include "../PropertyStats.h"
#include "../StringUtil.h"

namespace StatIndex {

// In registration order, which is the order the catalogues list their sources
// in, so that results come back in the order a list would show them.
static std::vector<Entry> entries;

Criterion Criterion::OnStat(const std::string& stat, Comparator comparator,
		int value) {
	Criterion criterion;
	criterion.stat = stat;
	criterion.comparator = comparator;
	criterion.value = value;
	return criterion;
}

Criterion Criterion::OnText(const std::string& text) {
	Criterion criterion;
	criterion.text = text;
	return criterion;
}

// Answered on the roll most favourable to the criterion, which for "more than"
// is the best roll: the player is asking what could grant them the stat, so a
// source whose range reaches the value answers.
static bool Compares(Comparator comparator, int value, int low, int high) {
	switch (comparator) {
		case GreaterThan:	return high > value;
		case LessThan:		return low < value;
		case EqualTo:		return low <= value && value <= high;
	}
	return false;
}

// Appends the range where the criterion is about a stat and the source answers
// it. A text criterion has no range, which is what leaves it out of a result.
// A range appended for a source that goes on to fail a later criterion goes
// with the result it was appended to.
static bool Satisfies(const Entry& entry, const Criterion& criterion,
		const std::string& text, std::vector<Range>& ranges) {
	if (criterion.stat.length() == 0)
		return entry.searchKey.find(text) != std::string::npos;

	// The first kind of base that satisfies it answers, and the range reported
	// is the one that kind rolls. A source that grants a stat in one kind and
	// not in another still answers, which is the best roll read across the
	// kinds a source can be made on.
	for (unsigned int t = 0; t < entry.totals.size(); t++) {
		// A stat nothing granted comes back as a range of zero to zero, which
		// "less than five" would otherwise be satisfied by for every source in
		// the index.
		Range range;
		range.stat = criterion.stat;
		if (!StatDescriptions::TotalFor(entry.totals[t], criterion.stat,
				range.low, range.high))
			continue;
		if (!Compares(criterion.comparator, criterion.value, range.low,
				range.high))
			continue;

		ranges.push_back(range);
		return true;
	}
	return false;
}

// Everything the source can grant on one kind of base: what it grants whenever
// it is worn, and the bonuses a count of set pieces unlocks. A stat that
// arrives only with four pieces worn is one the source grants, and a criterion
// asking for it has to find it.
static std::vector<PropertyStats::Property> Granting(
		const std::vector<PropertyStats::Property>& properties,
		const std::vector<PropertyStats::Property>& partial) {
	std::vector<PropertyStats::Property> granting = properties;
	granting.insert(granting.end(), partial.begin(), partial.end());
	return granting;
}

// What a source can grant, a set at a time. A source made on several kinds of
// base grants something different in each, so each kind is added up on its own
// and answers for itself.
static std::vector<StatTotals> TotalsFor(const Catalogue::Source& source) {
	std::vector<StatTotals> totals;
	if (source.variants.empty()) {
		totals.push_back(PropertyStats::Totals(
			Granting(source.properties, source.partial)));
		return totals;
	}

	for (unsigned int v = 0; v < source.variants.size(); v++) {
		totals.push_back(PropertyStats::Totals(
			Granting(source.variants[v].properties, source.partial)));
	}
	return totals;
}

void Register(const std::string& kind, const std::string& searchKey,
		const Catalogue::Source& source) {
	Entry entry;
	entry.kind = kind;
	entry.searchKey = searchKey;
	entry.totals = TotalsFor(source);
	entry.source = &source;
	entries.push_back(entry);
}

std::vector<Result> Find(const Query& query) {
	// Lowercased once for the whole walk rather than once an entry, since a
	// panel asks this again on every keystroke.
	std::vector<std::string> text;
	for (unsigned int c = 0; c < query.criteria.size(); c++)
		text.push_back(ToLower(query.criteria[c].text));

	std::vector<Result> results;
	for (unsigned int i = 0; i < entries.size(); i++) {
		const Entry& entry = entries[i];
		if (query.kind.length() > 0 && entry.kind.compare(query.kind) != 0)
			continue;

		Result result;
		result.entry = &entry;
		bool satisfied = true;
		for (unsigned int c = 0; c < query.criteria.size() && satisfied; c++)
			satisfied = Satisfies(entry, query.criteria[c], text[c], result.ranges);
		if (satisfied)
			results.push_back(result);
	}
	return results;
}

}
