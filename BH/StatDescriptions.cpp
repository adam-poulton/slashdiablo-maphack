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
	case 24:	// charges: level, skill
		return Collapse(Substitute(text, { value, skill }));
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

// Emits the single line the game uses for a damage property, if this is one.
static bool DescribeDamage(JSONObject* property, const std::string& param,
		int min, int max, std::vector<std::string>& lines) {
	std::string first = property->getString("stat1");
	std::string second = property->getString("stat2");

	// Poison damage is held per frame in 256ths, and shown as a total spread
	// over the duration the parameter gives in frames.
	if (first.compare("poisonmindam") == 0) {
		int frames = ToInt(param, 1);
		if (frames <= 0)
			frames = 1;
		int total = (int)(((double)min * frames / 256.0) + 0.5);
		int seconds = (int)(((double)frames / 25.0) + 0.5);
		if (seconds < 1)
			seconds = 1;
		char line[128];
		sprintf_s(line, "+%d poison damage over %d seconds", total, seconds);
		lines.push_back(line);
		return true;
	}

	for (unsigned int i = 0; i < (sizeof(kDamagePairs) / sizeof(kDamagePairs[0])); i++) {
		if (first.compare(kDamagePairs[i].min) != 0 || second.compare(kDamagePairs[i].max) != 0)
			continue;
		StatDescription desc;
		if (!LookupStat(second, desc))
			return false;
		std::string range = Range(min, max);
		if (HasPlaceholder(desc.positive)) {
			lines.push_back(Collapse(Substitute(desc.positive, { range })));
			return true;
		}
		// "to Maximum Cold Damage" reads as "+3-14 Cold Damage" once combined.
		std::string label = desc.positive;
		size_t maximum = label.find("Maximum ");
		if (maximum != std::string::npos)
			label.erase(maximum, 8);
		if (label.compare(0, 3, "to ") == 0)
			label.erase(0, 3);
		lines.push_back(Collapse("+" + range + " " + label));
		return true;
	}
	return false;
}

void DescribeProperty(const std::string& code, const std::string& param,
		int min, int max, std::vector<std::string>& lines) {
	JSONObject* property = Tables::Properties.findEntry("code", code);
	if (!property)
		return;

	if (DescribeDamage(property, param, min, max, lines))
		return;

	std::string skill = GetSkillName(param);

	for (int n = 1; n <= 7; n++) {
		std::string index = ToText(n);
		std::string stat = property->getString("stat" + index);
		if (stat.length() == 0)
			continue;

		int func = ToInt(property->getString("func" + index));
		StatDescription desc;
		if (!LookupStat(stat, desc)) {
			// Stats such as the poison and cold durations have no description of
			// their own; the game folds them into the damage line.
			continue;
		}

		std::string value;
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

		if (func == 17 || func == 18) {
			// Granted per character level, stored in eighths of a point.
			value = Eighths(ToInt(param));
		} else if (desc.func == 20 || desc.func == 21) {
			// Negated once, rather than at each end of the range.
			value = "-" + Range(min, max);
		} else {
			value = Range(min, max);
		}

		if (func == 10) {
			int tab = ToInt(param, -1);
			if (tab >= 0 && tab < (int)(sizeof(kSkillTabStrings) / sizeof(kSkillTabStrings[0]))) {
				std::string key = "StrSklTabItem" + ToText(kSkillTabStrings[tab]);
				std::string text = StatDescriptions::GetString(key);
				if (text.length() > 0)
					desc.positive = text;
			}
		}

		std::string line = Describe(desc, value, skill, percent, plus,
			ToText(min), ToText(max));
		if (line.length() > 0)
			lines.push_back(line);

		// These describe the whole property in one line; the remaining stats are
		// parameters to it rather than separate bonuses.
		if (func == 10 || func == 19 || func == 21 || func == 22)
			break;
	}
}

}	// namespace StatDescriptions
