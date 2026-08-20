#include "StatDescriptions.h"
#include <algorithm>
#include <map>
#include <Windows.h>
#include "Common.h"
#include "MPQReader.h"
#include "TableReader.h"

namespace {

bool initialized = false;
std::map<std::string, std::string> strings;

// Offsets within a .tbl file, as described in TableReader.cpp.
enum TblOffsets {
	HeaderSize = 0x15,
	ElementSize = 0x02,
	NodeSize = 0x11,
	NumElementsOffset = 0x02,
	ActiveOffset = 0x00,
	KeyStringOffset = 0x07,
	ValueStringOffset = 0x0B
};

std::string ReadTblString(const char* buffer, size_t size, int offset) {
	if (offset < 0 || (size_t)offset >= size)
		return "";
	size_t end = offset;
	while (end < size && buffer[end] != 0)
		end++;
	return std::string(&buffer[offset], end - offset);
}

// The .tbl format is a hash table of key/value string pairs. We only need the
// key to value mapping, so the hashing side of it is ignored.
void ParseTbl(const char* buffer, size_t size) {
	if (size < HeaderSize)
		return;
	unsigned short count = *(unsigned short*)&buffer[NumElementsOffset];
	size_t firstNode = HeaderSize + (ElementSize * (size_t)count);
	for (unsigned short i = 0; i < count; i++) {
		size_t elementPos = HeaderSize + (ElementSize * (size_t)i);
		if (elementPos + ElementSize > size)
			break;
		unsigned short node = *(unsigned short*)&buffer[elementPos];
		size_t nodePos = firstNode + (NodeSize * (size_t)node);
		if (nodePos + NodeSize > size)
			continue;
		if (buffer[nodePos + ActiveOffset] == 0)
			continue;
		std::string key = ReadTblString(buffer, size, *(int*)&buffer[nodePos + KeyStringOffset]);
		std::string value = ReadTblString(buffer, size, *(int*)&buffer[nodePos + ValueStringOffset]);
		if (key.length() > 0)
			strings[key] = value;
	}
}

bool LoadTbl(const std::string& name) {
	// The tables live under the locale the client was installed with, and the
	// game's MPQ layer searches every loaded archive for us.
	const char* locales[] = { "eng", "esp", "deu", "fra", "ita", "por", "pol",
			"rus", "jpn", "kor", "chi", "sin", "tw" };
	for (int i = 0; i < (sizeof(locales) / sizeof(locales[0])); i++) {
		std::string path = std::string("data\\local\\lng\\") + locales[i] + "\\" + name + ".tbl";
		BufferData file = loadFile(path);
		if (!file.data)
			continue;
		ParseTbl((const char*)file.data, file.size);
		delete[] file.data;
		return true;
	}
	return false;
}

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
struct BuiltInProperty {
	const char* code;
	const char* stat;		// stand in stat to describe it, if there is one
	const char* stringKey;	// otherwise the string table key for the label
	bool percent;
	bool valueless;
};

static const BuiltInProperty kBuiltIns[] = {
	{ "dmg%",       "",                     "strModEnhancedDamage", true,  false },
	{ "dmg-min",    "mindamage",            "",                     false, false },
	{ "dmg-max",    "maxdamage",            "",                     false, false },
	{ "indestruct", "item_indesctructible", "",                     false, true  },
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

// The game's own word for "Level", used when building a charges line.
std::string GetLevelWord() {
	std::string word = StatDescriptions::GetString("ModStre10b");
	return (word.length() > 0) ? word : "Level";
}

struct StatDescription {
	int func;
	int val;
	std::string positive;
	std::string second;
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
	out.positive = StatDescriptions::GetString(entry->getString("descstrpos"));
	out.second = StatDescriptions::GetString(entry->getString("descstr2"));
	return true;
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
	return initialized;
}

bool Initialize() {
	if (initialized)
		return true;
	bool loaded = LoadTbl("string");
	loaded |= LoadTbl("expansionstring");
	loaded |= LoadTbl("patchstring");
	initialized = loaded;
	return initialized;
}

std::string GetString(const std::string& key) {
	if (key.length() == 0)
		return "";
	std::map<std::string, std::string>::iterator it = strings.find(key);
	return (it == strings.end()) ? "" : it->second;
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

		stat.id = name + "|" + param;
		stats.push_back(stat);

		// These describe the whole property in one line; the remaining stats are
		// parameters to it rather than separate bonuses.
		if (func == 10 || func == 19 || func == 21 || func == 22)
			break;
	}
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
	switch (desc.func) {
	case 2: case 5: case 7: case 10: case 20: case 22:
		percent = true;
		break;
	case 4: case 8:
		percent = true;
		plus = true;
		break;
	case 1: case 6: case 12:
		plus = true;
		break;
	default:
		break;
	}

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
