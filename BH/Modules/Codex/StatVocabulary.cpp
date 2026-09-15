#include "StatVocabulary.h"
#include <algorithm>
#include <map>
#include "../../Catalogue/Catalogues.h"
#include "../../Catalogue/StatIndex.h"
#include "../../StatDescriptions.h"
#include "../../StringUtil.h"

namespace StatVocabulary {

namespace {

// The stats the game's tables describe with nothing at all, and the two halves
// of the magic damage line, which they describe identically.
//
// This is the whole of what is written down here, and it is written down only
// because there is nothing to read instead: a stat with no `descfunc` has no
// words anywhere in the tables, and `magicmindam` and `magicmaxdam` share every
// column that could tell them apart. Everything else is worded from the tables
// and disambiguated from them, so this does not grow as the data does.
struct NamedStat {
	const char* stat;
	const char* label;
};

const NamedStat kNamed[] = {
	{ "coldlength",         "Cold Duration" },
	{ "poisonlength",       "Poison Duration" },
	{ "item_numsockets",    "Sockets" },
	{ "maxdurability",      "Durability" },
	{ "item_extrablood",    "Extra Blood" },
	{ "item_singleskill",   "Skill Level" },
	{ "item_nonclassskill", "Skill from Another Class" },
	{ "magicmindam",        "Minimum Magic Damage" },
	{ "magicmaxdam",        "Maximum Magic Damage" },
};

const char* NamedLabel(const std::string& stat) {
	for (unsigned int i = 0; i < (sizeof(kNamed) / sizeof(kNamed[0])); i++) {
		if (stat.compare(kNamed[i].stat) == 0)
			return kNamed[i].label;
	}
	return NULL;
}

// One vocabulary per kind, worked out the first time it is asked for. Held by
// kind rather than in one list because a kind answers only for its own stats,
// which is what keeps a stat that nothing in front grants out of the picker.
std::map<std::string, std::vector<Entry>> vocabularies;

bool ByLabel(const Entry& one, const Entry& two) {
	// Compared lowercased so that a label the tables happen to capitalise does
	// not sort away from its neighbours.
	return ToLower(one.label).compare(ToLower(two.label)) < 0;
}

// The tables word a stat granting points and the one beside it granting a share
// of them with the same words: flat and percentage cold absorb are both "Cold
// Absorb". Which is which is in the tables even though the wording is not, so
// the one that reads as a percentage says so.
//
// Worked out here rather than written into the labels because it applies to
// whichever pairs the data happens to hold, this version and the next.
void SplitPercentages(std::vector<Entry>& entries) {
	std::map<std::string, unsigned int> seen;
	for (unsigned int i = 0; i < entries.size(); i++)
		seen[entries[i].label]++;

	for (unsigned int i = 0; i < entries.size(); i++) {
		if (seen[entries[i].label] < 2)
			continue;
		if (StatDescriptions::StatIsPercent(entries[i].stat))
			entries[i].label += " %";
	}
}

// Nothing above can be trusted to have left every label distinct for data
// nobody has seen yet, and two rows a player cannot tell apart are worse than
// one row wearing a name out of the tables. This is the last resort and is
// expected never to fire; docs/adr/0010 says what to do if it ever does.
void SplitRemaining(std::vector<Entry>& entries) {
	std::map<std::string, unsigned int> seen;
	for (unsigned int i = 0; i < entries.size(); i++)
		seen[entries[i].label]++;

	for (unsigned int i = 0; i < entries.size(); i++) {
		if (seen[entries[i].label] > 1)
			entries[i].label += " (" + entries[i].stat + ")";
	}
}

std::vector<Entry> Build(const std::string& kind) {
	std::vector<std::string> stats = StatIndex::StatsGranted(kind);

	std::vector<Entry> entries;
	entries.reserve(stats.size());
	for (unsigned int i = 0; i < stats.size(); i++) {
		const char* named = NamedLabel(stats[i]);
		std::string label = named ? named : StatDescriptions::StatLabel(stats[i]);
		// A stat with neither is one the tables stopped describing since the
		// list above was written. It is still offered, since it still answers,
		// and its own name is the only thing left to offer it under.
		entries.push_back(Entry(stats[i], label.length() > 0 ? label : stats[i]));
	}

	SplitPercentages(entries);
	SplitRemaining(entries);
	std::sort(entries.begin(), entries.end(), ByLabel);
	return entries;
}

}	// namespace

const std::vector<Entry>& For(const std::string& kind) {
	static const std::vector<Entry> none;
	if (!Catalogue::Loaded() || !StatDescriptions::IsInitialized())
		return none;

	std::map<std::string, std::vector<Entry>>::iterator it =
		vocabularies.find(kind);
	if (it != vocabularies.end())
		return it->second;

	return vocabularies.insert(std::make_pair(kind, Build(kind))).first->second;
}

std::vector<Entry> Matching(const std::string& kind, const std::string& text) {
	const std::vector<Entry>& all = For(kind);
	std::string wanted = ToLower(Trim(text));

	std::vector<Entry> matches;
	for (unsigned int i = 0; i < all.size(); i++) {
		// The stat's own name is matched as well as its label, though only the
		// label is shown. It costs nothing, and someone who knows the tables
		// should not have to guess what the words for a stat came out as.
		if (ToLower(all[i].label).find(wanted) != std::string::npos ||
				ToLower(all[i].stat).find(wanted) != std::string::npos)
			matches.push_back(all[i]);
	}
	return matches;
}

std::string LabelFor(const std::string& kind, const std::string& stat) {
	const std::vector<Entry>& all = For(kind);
	for (unsigned int i = 0; i < all.size(); i++) {
		if (all[i].stat.compare(stat) == 0)
			return all[i].label;
	}
	return stat;
}

}
