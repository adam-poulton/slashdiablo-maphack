#include "StatIndex.h"
#include "../PropertyStats.h"
#include "../StringUtil.h"

namespace StatIndex {

// In registration order, which is the order the catalogues list their sources
// in, so that an answer comes back in the order a list would show it.
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

// A stat nothing granted reads as a range of zero to zero, which "less than
// five" would otherwise answer for every source in the index. Whether the
// source writes the stat at all is the question, and only the totals can say.
static bool Grants(const std::vector<StatDescriptions::StatTotal>& totals,
		const std::string& stat) {
	for (unsigned int i = 0; i < totals.size(); i++) {
		if (totals[i].stat.compare(stat) == 0)
			return true;
	}
	return false;
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

// Fills in the range where the criterion is about a stat and the source answers
// it. A text criterion has no range, which is what leaves it out of a result.
static bool Satisfies(const Entry& entry, const Criterion& criterion,
		Range& range, bool& ranged) {
	ranged = false;
	if (criterion.stat.length() == 0)
		return entry.searchKey.find(ToLower(criterion.text)) != std::string::npos;

	if (!Grants(entry.totals, criterion.stat))
		return false;

	int low = 0, high = 0;
	StatDescriptions::TotalFor(entry.totals, criterion.stat, low, high);
	if (!Compares(criterion.comparator, criterion.value, low, high))
		return false;

	range.stat = criterion.stat;
	range.low = low;
	range.high = high;
	ranged = true;
	return true;
}

void Register(const std::string& kind, const std::string& searchKey,
		const Catalogue::Source& source) {
	Entry entry;
	entry.kind = kind;
	entry.searchKey = searchKey;
	entry.totals = PropertyStats::Totals(source.properties);
	entry.source = &source;
	entries.push_back(entry);
}

std::vector<Result> Find(const Query& query) {
	std::vector<Result> results;
	for (unsigned int i = 0; i < entries.size(); i++) {
		const Entry& entry = entries[i];
		if (query.kind.length() > 0 && entry.kind.compare(query.kind) != 0)
			continue;

		Result result;
		result.entry = &entry;
		bool satisfied = true;
		for (unsigned int c = 0; c < query.criteria.size() && satisfied; c++) {
			Range range;
			bool ranged = false;
			satisfied = Satisfies(entry, query.criteria[c], range, ranged);
			if (satisfied && ranged)
				result.ranges.push_back(range);
		}
		if (satisfied)
			results.push_back(result);
	}
	return results;
}

}
