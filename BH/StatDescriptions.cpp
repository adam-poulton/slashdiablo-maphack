#include "StatDescriptions.h"
#include <algorithm>
#include <istream>
#include <map>
#include <Windows.h>
#include "StatDescriptionsStrings.h"
#include "StringUtil.h"
#include "TableReader.h"

namespace {

std::map<std::string, std::string> strings;

int ToInt(const std::string& text, int fallback = 0) {
	if (text.length() == 0)
		return fallback;
	return atoi(text.c_str());
}

std::string ToText(int value) {
	char buffer[16];
	sprintf_s(buffer, "%d", value);
	return buffer;
}

// "5" for a fixed value, "3-8" for a range.
std::string Range(int min, int max) {
	return (min == max) ? ToText(min) : (ToText(min) + "-" + ToText(max));
}

// Per level values are stored in eighths.
std::string Eighths(int value) {
	char buffer[16];
	sprintf_s(buffer, "%.2f", (double)value / 8.0);
	std::string text = buffer;
	// Trim a trailing ".00" so whole numbers read normally.
	if (text.length() > 3 && text.compare(text.length() - 3, 3, ".00") == 0)
		text.erase(text.length() - 3);
	return text;
}

std::string Collapse(const std::string& text) {
	std::string out;
	bool space = false;
	for (unsigned int i = 0; i < text.length(); i++) {
		if (text[i] == ' ') {
			space = true;
			continue;
		}
		if (space && out.length() > 0)
			out += ' ';
		space = false;
		out += text[i];
	}
	return out;
}

// Substitutes the printf style placeholders some description strings carry, in
// the order the game fills them.
std::string Substitute(std::string text, const std::vector<std::string>& values) {
	unsigned int used = 0;
	std::string out;
	for (unsigned int i = 0; i < text.length(); i++) {
		if (text[i] != '%') {
			out += text[i];
			continue;
		}
		// "%%" is a literal per cent sign.
		if (i + 1 < text.length() && text[i + 1] == '%') {
			out += '%';
			i++;
			continue;
		}
		unsigned int end = i + 1;
		while (end < text.length() && strchr("+-0123456789.", text[end]))
			end++;
		if (end < text.length() && (text[end] == 'd' || text[end] == 'i' || text[end] == 's')) {
			out += (used < values.size()) ? values[used] : "";
			used++;
			i = end;
			continue;
		}
		out += text[i];
	}
	return out;
}

// True when a description string formats itself, e.g. "+%d magic damage". The
// value must then be substituted into it rather than tacked on to either end.
bool HasPlaceholder(const std::string& text) {
	for (unsigned int i = 0; i < text.length(); i++) {
		if (text[i] != '%' || i + 1 >= text.length())
			continue;
		if (text[i + 1] == '%') {
			i++;
			continue;
		}
		unsigned int j = i + 1;
		while (j < text.length() && strchr("+-0123456789.", text[j]))
			j++;
		if (j < text.length() && (text[j] == 'd' || text[j] == 'i' || text[j] == 's'))
			return true;
	}
	return false;
}

// A skill tab property's parameter is an index into the game's own ordering of
// the tabs, which picks one of the StrSklTabItem strings. The order isn't
// derivable, so it is listed out; the three runewords that grant a skill tab
// confirm entries 0, 7 and 10.
static const int kSkillTabStrings[] = {
	3, 2, 1,		// Amazon: bow, passive, javelin
	15, 14, 13,		// Sorceress: fire, lightning, cold
	8, 7, 6,		// Necromancer: curses, poison and bone, summoning
	11, 4, 5,		// Paladin: combat, defensive auras, offensive auras
	11, 12, 10,		// Barbarian: combat, masteries, warcries
	9, 17, 18,		// Druid: summoning, shape shifting, elemental
	19, 20, 21		// Assassin: traps, shadow disciplines, martial arts
};

// A handful of properties are built into the game rather than described by a
// stat in Properties.txt, so they have to be spelled out. "dmg%" alone is used
// by half the runewords.
//
// Not every property without a stat belongs here. Famine lists "ethereal", but
// ethereality belongs to the base item rather than the runeword and shows
// nothing on the finished item, so it is left to describe nothing.
//
// grants[] is what the property really writes, which is not always what
// describes it: "dmg%" reads as one line but grants two stats. Both of a
// property's readings are kept in the one row so that adding a fifth built in
// is a single edit and the two passes cannot drift apart.
struct BuiltInProperty {
	const char* code;
	const char* stat;		// stand in stat to describe it, if there is one
	const char* stringKey;	// otherwise the string table key for the label
	const char* orderStat;	// stat whose descpriority places the line, if the
							// property is not described by a stat at all
	bool percent;
	bool valueless;
	const char* grants[2];	// the stats it really writes
};

static const BuiltInProperty kBuiltIns[] = {
	{ "dmg%",       "",                     "strModEnhancedDamage",
		"item_mindamage_percent", true,  false,
		{ "item_mindamage_percent", "item_maxdamage_percent" } },
	{ "dmg-min",    "mindamage",            "", "", false, false,
		{ "mindamage", 0 } },
	{ "dmg-max",    "maxdamage",            "", "", false, false,
		{ "maxdamage", 0 } },
	{ "indestruct", "item_indesctructible", "", "", false, true,
		{ "item_indesctructible", 0 } },
};

// Elemental damage is stored as separate minimum and maximum stats which the
// game shows as a single line.
struct DamagePair {
	const char* min;
	const char* max;
};

static const DamagePair kDamagePairs[] = {
	{ "mindamage", "maxdamage" },
	{ "firemindam", "firemaxdam" },
	{ "lightmindam", "lightmaxdam" },
	{ "magicmindam", "magicmaxdam" },
	{ "coldmindam", "coldmaxdam" },
};

// The three letter codes the item tables restrict a class item by, in the order
// CharStats.txt lists the classes they name.
const char* const kClassCodes[] = {
	"ama", "sor", "nec", "pal", "bar", "dru", "ass"
};

// CharStats.txt lists the classes in the order the class skill properties number
// them, with an "Expansion" divider row partway down that is not a class.
JSONObject* CharClass(int index) {
	if (index < 0)
		return NULL;
	int seen = 0;
	for (int i = 0; i < Tables::CharStats.size(); i++) {
		JSONObject* entry = Tables::CharStats.entryAt(i);
		if (!entry)
			continue;
		std::string name = Trim(entry->getString("class"));
		if (name.length() == 0 || name.compare("Expansion") == 0)
			continue;
		if (seen++ == index)
			return entry;
	}
	return NULL;
}

// The game's own word for "Level", used when building a charges line.
std::string GetLevelWord() {
	std::string word = StatDescriptions::GetString("ModStre10b");
	return (word.length() > 0) ? word : "Level";
}

struct StatDescription {
	int func;
	int val;
	int priority;	// descpriority, which is what orders the line
	std::string positive;
	std::string second;

	StatDescription() : func(0), val(1), priority(0) {};
};

bool LookupStat(const std::string& stat, StatDescription& out) {
	JSONObject* entry = Tables::ItemStatCost.findEntry("Stat", stat);
	if (!entry)
		return false;
	std::string func = entry->getString("descfunc");
	if (func.length() == 0)
		return false;	// an internal stat with no description of its own
	out.func = ToInt(func);
	out.val = ToInt(entry->getString("descval"), 1);
	out.priority = ToInt(entry->getString("descpriority"));
	out.positive = StatDescriptions::GetString(entry->getString("descstrpos"));
	out.second = StatDescriptions::GetString(entry->getString("descstr2"));
	return true;
}

// Where a line without a stat of its own sits in the order, taken from the stat
// that stands in for it.
int LookupPriority(const std::string& stat) {
	JSONObject* entry = Tables::ItemStatCost.findEntry("Stat", stat);
	return entry ? ToInt(entry->getString("descpriority")) : 0;
}

// Which of the description rules put the value in front of the text, and which
// write it as a percentage. Shared by the stat lines and the grouped lines, so
// both read the same way.
void FormatFlags(int func, bool& percent, bool& plus) {
	percent = false;
	plus = false;
	switch (func) {
	case 2: case 5: case 7: case 10: case 20: case 22:
		percent = true;
		break;
	case 4: case 8:
		percent = true;
		plus = true;
		break;
	case 1: case 6: case 12: case 13:
		plus = true;
		break;
	default:
		break;
	}
}

// Builds one line from a stat's description rule and a value.
std::string Describe(const StatDescription& desc, const std::string& value,
		const std::string& skill, bool percent, bool plus,
		const std::string& low, const std::string& high) {
	std::string text = desc.positive;
	std::string shown = value;
	if (percent)
		shown += "%";
	if (plus && shown.length() > 0 && shown[0] != '-')
		shown = "+" + shown;

	switch (desc.func) {
	case 15:	// chance to cast: the chance is the min and the level the max
		return Collapse(Substitute(text, { low, high, skill }));
	case 16:	// aura when equipped: level, skill
		return Collapse(Substitute(text, { value, skill }));
	case 24:
		// Charges read "Level 21 Cyclone Armor (30/30 Charges)": the level is
		// the max and the number of charges the min.
		return Collapse(GetLevelWord() + " " + high + " " + skill + " " +
			Substitute(text, { low, low }));
	case 27:	// a skill, optionally limited to one class
	case 28:
		return Collapse("+" + value + " to " + (skill.length() ? skill : text));
	default:
		break;
	}

	// A string that formats itself takes the value inline.
	if (HasPlaceholder(text))
		return Collapse(Substitute(text, { value }));

	if (desc.val == 0)
		return Collapse(text);
	if (desc.val == 2)
		return Collapse(text + " " + shown);
	return Collapse(shown + " " + text +
		(desc.second.length() ? (" " + desc.second) : ""));
}


// One of the game's display groups: the stats that fold into a single line when
// an item grants all of them at the same value, and how that line is written.
// ItemStatCost.txt defines these in its dgrp columns, so "All Resistances +30"
// and "+15 to all Attributes" come from the game's own data rather than from a
// rule spelled out here.
struct StatGroup {
	std::vector<std::string> members;
	StatDescription desc;
	int priority;	// the highest priority among its members, which is where the
					// group takes the place of the stats it replaces
};

std::map<int, StatGroup> statGroups;
bool statGroupsLoaded = false;

void LoadStatGroups() {
	if (statGroupsLoaded)
		return;
	// The combined line's wording comes from the string tables, so there is
	// nothing worth keeping until those are in.
	if (!StatDescriptions::IsInitialized() || Tables::ItemStatCost.size() == 0)
		return;
	statGroupsLoaded = true;
	for (int i = 0; i < Tables::ItemStatCost.size(); i++) {
		JSONObject* entry = Tables::ItemStatCost.entryAt(i);
		if (!entry)
			continue;
		std::string group = Trim(entry->getString("dgrp"));
		std::string stat = Trim(entry->getString("Stat"));
		if (group.length() == 0 || stat.length() == 0)
			continue;

		StatGroup& target = statGroups[ToInt(group)];
		if (target.members.empty()) {
			target.priority = 0;
			target.desc.func = ToInt(entry->getString("dgrpfunc"));
			target.desc.val = ToInt(entry->getString("dgrpval"), 1);
			target.desc.positive = StatDescriptions::GetString(entry->getString("dgrpstrpos"));
			target.desc.second = StatDescriptions::GetString(entry->getString("dgrpstr2"));
		}
		target.members.push_back(stat);

		int priority = ToInt(entry->getString("descpriority"));
		if (priority > target.priority)
			target.priority = priority;
	}
}

// The stats a damage property grants, as the single line the game shows.
static bool CollectDamage(JSONObject* property, const std::string& param,
		int min, int max, std::vector<StatDescriptions::Stat>& stats) {
	std::string first = property->getString("stat1");
	std::string second = property->getString("stat2");

	// Poison damage is held per frame in 256ths and shown as a total spread
	// over the duration the parameter gives in frames.
	if (first.compare("poisonmindam") == 0) {
		StatDescriptions::Stat stat;
		stat.kind = StatDescriptions::StatKindPoison;
		stat.priority = LookupPriority(first);
		stat.param = param;
		stat.low = min;
		stat.high = max;
		stat.id = "poison|" + param;
		stats.push_back(stat);
		return true;
	}

	for (unsigned int i = 0; i < (sizeof(kDamagePairs) / sizeof(kDamagePairs[0])); i++) {
		if (first.compare(kDamagePairs[i].min) != 0 || second.compare(kDamagePairs[i].max) != 0)
			continue;
		StatDescription desc;
		if (!LookupStat(second, desc))
			return false;

		StatDescriptions::Stat stat;
		stat.kind = StatDescriptions::StatKindDamage;
		stat.priority = desc.priority;
		stat.low = min;
		stat.high = max;
		if (HasPlaceholder(desc.positive)) {
			stat.text = desc.positive;
		} else {
			// "to Maximum Cold Damage" reads as "+3-14 Cold Damage" combined.
			std::string label = desc.positive;
			size_t maximum = label.find("Maximum ");
			if (maximum != std::string::npos)
				label.erase(maximum, 8);
			if (label.compare(0, 3, "to ") == 0)
				label.erase(0, 3);
			stat.text = label;
		}
		stat.id = "damage|" + stat.text;
		stats.push_back(stat);
		return true;
	}
	return false;
}

}	// namespace

namespace StatDescriptions {

bool IsInitialized() {
	return !strings.empty();
}

void AddString(const std::string& key, const std::string& text) {
	if (key.length() == 0)
		return;
	strings[key] = text;
}

void LoadStrings(std::istream& stream) {
	std::string line;
	while (std::getline(stream, line)) {
		if (line.length() > 0 && line[line.length() - 1] == '\r')
			line.erase(line.length() - 1);
		size_t tab = line.find('\t');
		if (tab == std::string::npos)
			continue;
		AddString(line.substr(0, tab), line.substr(tab + 1));
	}
}

std::string GetString(const std::string& key) {
	if (key.length() == 0)
		return "";
	std::map<std::string, std::string>::iterator it = strings.find(key);
	return (it == strings.end()) ? "" : it->second;
}

std::string GetClassOnly(const std::string& classCode) {
	for (unsigned int i = 0; i < (sizeof(kClassCodes) / sizeof(kClassCodes[0])); i++) {
		if (classCode.compare(kClassCodes[i]) != 0)
			continue;
		JSONObject* charClass = CharClass((int)i);
		return charClass ? GetString(charClass->getString("StrClassOnly")) : "";
	}
	return "";
}

std::string GetSkillName(const std::string& idOrName) {
	if (idOrName.length() == 0)
		return "";

	JSONObject* skill = NULL;
	bool numeric = true;
	for (unsigned int i = 0; i < idOrName.length() && numeric; i++)
		numeric = (idOrName[i] >= '0' && idOrName[i] <= '9');
	if (numeric) {
		skill = Tables::Skills.findEntry("Id", idOrName);
	} else {
		std::string wanted = idOrName;
		std::transform(wanted.begin(), wanted.end(), wanted.begin(), ::tolower);
		skill = Tables::Skills.findEntry([&wanted](JSONObject* row) -> bool {
			std::string name = row->getString("skill");
			std::transform(name.begin(), name.end(), name.begin(), ::tolower);
			return name.compare(wanted) == 0;
		});
	}
	if (!skill)
		return idOrName;

	// skills.txt points at skilldesc.txt, which holds the string table key.
	JSONObject* desc = Tables::SkillDesc.findEntry("skilldesc", skill->getString("skilldesc"));
	if (desc) {
		std::string name = GetString(desc->getString("str name"));
		if (name.length() > 0)
			return name;
	}
	return skill->getString("skill");
}

void CollectProperty(const std::string& code, const std::string& param,
		int min, int max, std::vector<Stat>& stats) {
	for (unsigned int i = 0; i < (sizeof(kBuiltIns) / sizeof(kBuiltIns[0])); i++) {
		if (code.compare(kBuiltIns[i].code) != 0)
			continue;
		Stat stat;
		stat.low = min;
		stat.high = max;
		stat.percent = kBuiltIns[i].percent;
		stat.priority = LookupPriority(kBuiltIns[i].stat[0] != 0 ?
			kBuiltIns[i].stat : kBuiltIns[i].orderStat);
		if (kBuiltIns[i].stat[0] != 0) {
			// Described by a stat that exists, just not on this property.
			stat.stat = kBuiltIns[i].stat;
			stat.id = stat.stat + "|" + param;
			stat.param = param;
		} else {
			stat.kind = kBuiltIns[i].valueless ? StatKindText : StatKindDamage;
			stat.text = GetString(kBuiltIns[i].stringKey);
			if (stat.text.length() == 0)
				stat.text = code;	// no string of its own in the tables
			stat.id = "builtin|" + code;
		}
		stats.push_back(stat);
		return;
	}

	JSONObject* property = Tables::Properties.findEntry("code", code);
	if (!property)
		return;

	if (CollectDamage(property, param, min, max, stats))
		return;

	for (int n = 1; n <= 7; n++) {
		std::string index = ToText(n);
		std::string name = property->getString("stat" + index);
		if (name.length() == 0)
			continue;

		int func = ToInt(property->getString("func" + index));
		StatDescription desc;
		if (!LookupStat(name, desc)) {
			// Stats such as the poison and cold durations have no description of
			// their own; the game folds them into the damage line.
			continue;
		}

		Stat stat;
		stat.stat = name;
		stat.param = param;
		stat.low = min;
		stat.high = max;
		stat.priority = desc.priority;

		if (func == 17 || func == 18) {
			// Granted per character level, with the amount in the parameter.
			stat.perLevel = true;
			stat.low = stat.high = ToInt(param);
		}

		if (func == 10) {
			// A skill tab's parameter picks which tab string to use.
			int tab = ToInt(param, -1);
			if (tab >= 0 && tab < (int)(sizeof(kSkillTabStrings) / sizeof(kSkillTabStrings[0])))
				stat.text = GetString("StrSklTabItem" + ToText(kSkillTabStrings[tab]));
		}

		if (func == 21 && name.compare("item_addclassskills") == 0) {
			// Which class's skills it raises is in the property's own value
			// column rather than in the item's parameter, so the line has to be
			// labelled from there. Without it every class reads as the first one.
			JSONObject* charClass = CharClass(ToInt(property->getString("val" + index), -1));
			if (charClass)
				stat.text = GetString(charClass->getString("StrAllSkills"));
		}

		// Two properties granting the same stat are only the same bonus if they
		// resolved to the same label, so an item raising two classes' skills
		// keeps them apart.
		stat.id = name + "|" + param +
			(stat.text.length() > 0 ? ("|" + stat.text) : "");
		stats.push_back(stat);

		// These describe the whole property in one line; the remaining stats are
		// parameters to it rather than separate bonuses.
		if (func == 10 || func == 19 || func == 21 || func == 22)
			break;
	}
}

// The same rows CollectProperty walks, read for what they grant rather than for
// how they read: no descfunc gate, since a stat with no wording of its own still
// counts; no folding of a minimum and a maximum into one line, since the two
// ends are wanted apart; and no stopping early on the properties whose later
// stats are arguments to the first, since those are arguments to a description
// and not to a sum.
void CollectTotals(const std::string& code, const std::string& param,
		int min, int max, std::vector<StatTotal>& totals) {
	for (unsigned int i = 0; i < (sizeof(kBuiltIns) / sizeof(kBuiltIns[0])); i++) {
		if (code.compare(kBuiltIns[i].code) != 0)
			continue;
		for (int n = 0; n < 2 && kBuiltIns[i].grants[n]; n++) {
			StatTotal total;
			total.stat = kBuiltIns[i].grants[n];
			total.low = min;
			total.high = max;
			totals.push_back(total);
		}
		return;
	}

	JSONObject* property = Tables::Properties.findEntry("code", code);
	if (!property)
		return;

	for (int n = 1; n <= 7; n++) {
		std::string index = ToText(n);
		std::string name = property->getString("stat" + index);
		if (name.length() == 0)
			continue;

		// Granted per character level, and held in eighths in the parameter
		// rather than in the range, so it is nothing a sum of flat amounts can
		// take in.
		int func = ToInt(property->getString("func" + index));
		if (func == 17 || func == 18)
			continue;

		// Poison damage is held per frame in 256ths, and the total the game
		// shows needs the duration the parameter carries. The raw amount is not
		// one a caller could compare against a number of points.
		if (name.compare("poisonmindam") == 0 || name.compare("poisonmaxdam") == 0)
			continue;

		StatTotal total;
		total.stat = name;
		if (func == 15) {
			// The minimum half of a damage line: "Adds 10-40 Fire Damage" grants
			// exactly ten of the minimum and exactly forty of the maximum, so
			// neither half rolls the range the row carries.
			total.low = total.high = min;
		} else if (func == 16) {
			total.low = total.high = max;
		} else {
			total.low = min;
			total.high = max;
		}
		totals.push_back(total);
	}
}

int StatId(const std::string& stat) {
	JSONObject* entry = Tables::ItemStatCost.findEntry("Stat", stat);
	return entry ? ToInt(entry->getString("ID"), -1) : -1;
}

bool TotalFor(const std::vector<StatTotal>& totals, const std::string& stat,
		int& low, int& high) {
	low = high = 0;
	bool written = false;
	for (unsigned int i = 0; i < totals.size(); i++) {
		if (totals[i].stat.compare(stat) != 0)
			continue;
		low += totals[i].low;
		high += totals[i].high;
		written = true;
	}
	return written;
}

void MergeStats(std::vector<Stat>& stats) {
	std::vector<Stat> merged;
	for (unsigned int i = 0; i < stats.size(); i++) {
		bool found = false;
		for (unsigned int j = 0; j < merged.size() && !found; j++) {
			if (merged[j].id.compare(stats[i].id) != 0)
				continue;
			merged[j].low += stats[i].low;
			merged[j].high += stats[i].high;
			found = true;
		}
		if (!found)
			merged.push_back(stats[i]);
	}
	stats.swap(merged);
}

// A stat only folds into a group when it is an ordinary stat granted flat or per
// level, since the group line has nowhere to say what a parameter meant.
static bool CanGroup(const Stat& stat) {
	return stat.kind == StatKindNormal && stat.text.length() == 0 &&
		stat.param.length() == 0;
}

// Two stats fold together only if they were granted identically, so an item with
// +20 to three resistances and +30 to the fourth still lists all four.
static bool GroupsWith(const Stat& a, const Stat& b) {
	return a.low == b.low && a.high == b.high && a.perLevel == b.perLevel;
}

void GroupStats(std::vector<Stat>& stats) {
	LoadStatGroups();

	for (std::map<int, StatGroup>::iterator group = statGroups.begin();
			group != statGroups.end(); group++) {
		const std::vector<std::string>& members = group->second.members;
		if (members.size() < 2)
			continue;

		// Every member has to be present, and all of them granted the same way,
		// or the group does not apply and the stats stay as they are.
		std::vector<int> found(members.size(), -1);
		bool complete = true;
		for (unsigned int m = 0; m < members.size() && complete; m++) {
			for (unsigned int i = 0; i < stats.size() && found[m] < 0; i++) {
				if (CanGroup(stats[i]) && stats[i].stat.compare(members[m]) == 0)
					found[m] = (int)i;
			}
			complete = (found[m] >= 0) &&
				GroupsWith(stats[found[m]], stats[found[0]]);
		}
		if (!complete)
			continue;

		// The combined line takes the place of the first of them, so the group
		// sits where the stats it replaces would have been.
		const Stat& first = stats[found[0]];
		Stat combined;
		combined.kind = StatKindText;
		combined.priority = group->second.priority;
		combined.id = "dgrp|" + ToText(group->first);

		bool percent = false, plus = false;
		FormatFlags(group->second.desc.func, percent, plus);
		std::string value = first.perLevel ? Eighths(first.low) :
			Range(first.low, first.high);
		combined.text = Describe(group->second.desc, value, "", percent, plus,
			ToText(first.low), ToText(first.high));
		if (combined.text.length() == 0)
			continue;

		// Erased back to front, so the indices ahead of each one stay valid.
		std::vector<int> remove(found);
		std::sort(remove.begin(), remove.end());
		int at = remove.front();
		for (int i = (int)remove.size() - 1; i >= 0; i--)
			stats.erase(stats.begin() + remove[i]);
		stats.insert(stats.begin() + at, combined);
	}
}

void SortStats(std::vector<Stat>& stats) {
	// Stable, so stats the tables give equal priority keep the order they were
	// collected in rather than being shuffled against each other.
	std::stable_sort(stats.begin(), stats.end(), [](const Stat& a, const Stat& b) {
		return a.priority > b.priority;
	});
}

std::vector<std::string> BuildLines(std::vector<Stat> stats) {
	MergeStats(stats);
	GroupStats(stats);
	SortStats(stats);

	std::vector<std::string> lines;
	for (unsigned int i = 0; i < stats.size(); i++) {
		std::string line = Render(stats[i]);
		if (line.length() > 0)
			lines.push_back(line);
	}
	return lines;
}

std::string Render(const Stat& stat) {
	if (stat.kind == StatKindPoison) {
		int frames = ToInt(stat.param, 1);
		if (frames <= 0)
			frames = 1;
		int total = (int)(((double)stat.low * frames / 256.0) + 0.5);
		int seconds = (int)(((double)frames / 25.0) + 0.5);
		if (seconds < 1)
			seconds = 1;
		char line[128];
		sprintf_s(line, "+%d poison damage over %d seconds", total, seconds);
		return line;
	}

	if (stat.kind == StatKindText)
		return stat.text;

	std::string range = Range(stat.low, stat.high);

	if (stat.kind == StatKindDamage) {
		if (HasPlaceholder(stat.text))
			return Collapse(Substitute(stat.text, { range }));
		return Collapse("+" + range + (stat.percent ? "% " : " ") + stat.text);
	}

	StatDescription desc;
	if (!LookupStat(stat.stat, desc))
		return "";
	if (stat.text.length() > 0)
		desc.positive = stat.text;

	bool percent = false, plus = false;
	FormatFlags(desc.func, percent, plus);

	std::string value;
	if (stat.perLevel) {
		value = Eighths(stat.low);
	} else if (desc.func == 20 || desc.func == 21) {
		// Negated once, rather than at each end of the range.
		value = "-" + range;
	} else {
		value = range;
	}

	return Describe(desc, value, GetSkillName(stat.param), percent, plus,
		ToText(stat.low), ToText(stat.high));
}

}	// namespace StatDescriptions
