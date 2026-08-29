#include "doctest.h"
#include <fstream>
#include <map>
#include <string>
#include <vector>
#include "../BH/Constants.h"
#include "../BH/ItemTables.h"
#include "CaptureFormat.h"
#include "FilterContext.h"
#include "ItemFacts.h"
#include "ItemFactsPacket.h"
#include "ItemFilter.h"

/*
 * Judging the items a session recorded, and checking the answers against the
 * ones it recorded.
 *
 * Each case is a whole decision put back together: the bytes that arrived, the
 * tables they were read against, the rules in force, the world the character
 * was in, and what the filter concluded. Given the first four, the fifth has to
 * come out again.
 *
 * This is the check the rest of the item filter's rework was for. Every step of
 * it was made to keep behaviour and verified by nothing but the compiler and
 * reading; these are the recorded decisions of a filter that had not been
 * touched yet.
 */

using CaptureFormat::Fields;
using CaptureFormat::ParseLine;

namespace {

class FixtureTables : public ItemFactsPacket::Tables {
public:
	void AddStat(unsigned int at, const StatProperties& stat) {
		if (widths.size() <= at) {
			widths.resize(at + 1);
			present.resize(at + 1, false);
		}
		widths[at] = stat;
		present[at] = true;
	}

	void AddItem(const std::string& code, const ItemAttributes& item) {
		items[code] = item;
	}

	ItemAttributes* Attributes(const char* code) const override {
		std::map<std::string, ItemAttributes>::const_iterator found = items.find(code);
		return (found == items.end()) ? NULL
			: const_cast<ItemAttributes*>(&found->second);
	}

	StatProperties* Stat(unsigned int stat) const override {
		if (stat >= widths.size() || !present[stat])
			return NULL;
		return const_cast<StatProperties*>(&widths[stat]);
	}

private:
	std::vector<StatProperties> widths;
	std::vector<bool> present;
	std::map<std::string, ItemAttributes> items;
};

class SilentPacketDiagnostics : public ItemFactsPacket::Diagnostics {};

// The character's own stats. Only the level is recorded, and no rule kept in
// the fixtures asks for anything else: CHARSTAT appears in none of them.
class RecordedCharStats : public StatSource {
public:
	explicit RecordedCharStats(unsigned int level) : level(level) {}

	int Stat(unsigned int stat, unsigned int sub) const override {
		return (stat == STAT_LEVEL) ? (int)level : 0;
	}

	const std::vector<StatEntry>& Stats() const override { return none; }

private:
	unsigned int level;
	std::vector<StatEntry> none;
};

// A rule set as it was read, with the lists a rule is sorted into.
struct RuleSet {
	std::vector<Rule*> all;
	std::vector<Rule*> map;
	std::vector<Rule*> doNotBlock;
	std::vector<Rule*> ignore;
	int ignoredTokens;

	RuleSet() : ignoredTokens(0) {}

	~RuleSet() {
		for (unsigned int i = 0; i < all.size(); i++) {
			for (unsigned int c = 0; c < all[i]->conditions.size(); c++)
				delete all[i]->conditions[c];
			delete all[i];
		}
	}
};

class CountingFilterDiagnostics : public ItemFilterDiagnostics {
public:
	CountingFilterDiagnostics() : ignored(0) {}
	void IgnoredToken(const std::string& token) override { ignored++; }
	void UnreadableValue(const std::string& token) override { ignored++; }
	int ignored;
};

std::vector<Fields> ReadFixture(const std::string& name) {
	std::vector<Fields> records;
	std::ifstream file("BHTests/fixtures/" + name);
	REQUIRE_MESSAGE(file.is_open(),
		"fixture not found, run the tests from the repository root: " << name);
	std::string line;
	while (std::getline(file, line)) {
		if (!line.empty() && line[line.length() - 1] == '\r')
			line.erase(line.length() - 1);
		if (!line.empty())
			records.push_back(ParseLine(line));
	}
	return records;
}

void LoadTables(FixtureTables& tables) {
	std::vector<Fields> records = ReadFixture("tables.txt");
	for (unsigned int i = 0; i < records.size(); i++) {
		const Fields& r = records[i];
		if (r.type == "statwidths") {
			StatProperties stat;
			stat.name = r.Text("name");
			stat.saveBits = (unsigned char)r.Number("saveBits");
			stat.saveParamBits = (unsigned char)r.Number("saveParamBits");
			stat.saveAdd = (unsigned char)r.Number("saveAdd");
			stat.op = (unsigned char)r.Number("op");
			stat.sendParamBits = (unsigned char)r.Number("sendParamBits");
			stat.ID = (unsigned short)r.Number("at");
			tables.AddStat((unsigned int)r.Number("at"), stat);
		} else if (r.type == "itemattrs") {
			ItemAttributes item;
			std::string code = r.Text("code");
			item.name = r.Text("name");
			item.category = r.Text("category");
			for (int c = 0; c < 4; c++)
				item.code[c] = (c < (int)code.length()) ? code[c] : 0;
			item.width = (unsigned char)r.Number("width");
			item.height = (unsigned char)r.Number("height");
			item.stackable = (unsigned char)r.Number("stackable");
			item.useable = (unsigned char)r.Number("useable");
			item.throwable = (unsigned char)r.Number("throwable");
			item.itemLevel = (unsigned char)r.Number("itemLevel");
			item.unusedFlags = 0;
			item.flags = (unsigned int)r.Number("flags");
			item.flags2 = (unsigned int)r.Number("flags2");
			item.qualityLevel = (unsigned char)r.Number("qualityLevel");
			item.magicLevel = (unsigned char)r.Number("magicLevel");
			tables.AddItem(code, item);
		}
	}
}

// Reads a rule the way the game does, and sorts it into the same lists.
void AddRule(RuleSet& set, const std::string& condition, const std::string& action,
		unsigned int index, const ItemFilterSettings& settings) {
	// Reading a rule is not reentrant: the parser tracks whether the last thing
	// it saw was an operand in a global, so that it can put in the AND between
	// two conditions written side by side. Every rule starts afresh.
	LastConditionType = CT_None;

	std::vector<Condition*> raw;
	std::string token;
	std::string text = condition;
	std::size_t at = 0;
	while (at <= text.length()) {
		std::size_t space = text.find(' ', at);
		if (space == std::string::npos)
			space = text.length();
		token = text.substr(at, space - at);
		at = space + 1;
		if (!token.empty())
			Condition::BuildConditions(raw, token, settings);
	}

	std::string actionText = action;
	Rule* rule = new Rule(raw, &actionText);
	rule->action.index = index;
	set.all.push_back(rule);

	// Sorted by the same decision the game sorts by, rather than by a second
	// reading of what the action does.
	RulePlacement placement = PlaceRule(*rule);
	if (placement.map)
		set.map.push_back(rule);
	if (placement.doNotBlock)
		set.doNotBlock.push_back(rule);
	if (placement.ignore)
		set.ignore.push_back(rule);
}

}  // namespace

TEST_CASE("every recorded decision is reached again") {
	FixtureTables tables;
	LoadTables(tables);
	SilentPacketDiagnostics packetDiagnostics;
	ItemFactsPacket::Reader reader(tables, packetDiagnostics, true);

	CountingFilterDiagnostics filterDiagnostics;
	ItemFilterSettings settings;
	settings.statMax = 512;
	settings.skillMax = 512;
	settings.diagnostics = &filterDiagnostics;

	std::vector<Fields> records = ReadFixture("filter-cases.txt");

	std::map<std::string, RuleSet*> ruleSets;
	std::map<std::string, unsigned int> ruleCounts;
	RuleSet* current = NULL;
	Fields header;
	int replayed = 0, blockedSeen = 0, shownSeen = 0;

	for (unsigned int i = 0; i < records.size(); i++) {
		const Fields& r = records[i];

		if (r.type == "rule") {
			std::string set = r.Text("set");
			if (!ruleSets.count(set)) {
				ruleSets[set] = new RuleSet();
				ruleCounts[set] = 0;
			}
			AddRule(*ruleSets[set], r.Text("condition"), r.Text("action"),
				ruleCounts[set]++, settings);
			continue;
		}

		if (r.type == "header") {
			REQUIRE(ruleSets.count(r.Text("ruleSet")) == 1);
			current = ruleSets[r.Text("ruleSet")];
			header = r;
			continue;
		}

		if (r.type != "drop")
			continue;
		REQUIRE(current != NULL);

		// The item, read back out of the bytes that carried it.
		std::string packet = r.Text("packet");
		std::vector<unsigned char> buffer(packet.length() + 256, 0);
		for (std::size_t b = 0; b < packet.length(); b++)
			buffer[b] = (unsigned char)packet[b];

		ItemFacts facts = {};
		ItemFactsPacket::PacketStats stats(facts);
		facts.stats = &stats;
		if (!reader.Read(&buffer[0], &facts))
			continue;	// the game did not judge what it could not read either

		// The world it landed in.
		RecordedCharStats charStats((unsigned int)r.Number("charLevel"));
		FilterContext context = {};
		context.charClass = (unsigned int)r.Number("charClass");
		context.charLevel = (unsigned int)r.Number("charLevel");
		context.charFlags = (unsigned int)r.Number("charFlags");
		context.difficulty = (unsigned int)r.Number("difficulty");
		context.areaId = (unsigned int)r.Number("areaId");
		context.areaLevel = (unsigned int)r.Number("areaLevel");
		context.filterLevel = (unsigned int)header.Number("filterLevel");
		context.charStats = &charStats;

		RuleLists lists = { &current->map, &current->doNotBlock, &current->ignore };
		RuleMatch match = MatchRules(lists, facts, context,
			(unsigned int)header.Number("pingLevel"),
			header.Boolean("orderedFiltering"));

		INFO("item " << r.Text("code") << " at drop " << i
			<< ", area " << r.Number("areaId"));
		CHECK((long long)match.keepIndex == r.Number("keepIndex"));
		CHECK((long long)match.ignoreIndex == r.Number("ignoreIndex"));
		CHECK(match.blocked == r.Boolean("blocked"));
		CHECK(match.showOnMap == r.Boolean("showOnMap"));
		CHECK(match.noTracking == r.Boolean("noTracking"));
		CHECK((long long)match.color == r.Number("color"));
		CHECK((long long)match.pingLevel == r.Number("pingLevel"));

		replayed++;
		if (r.Boolean("blocked"))
			blockedSeen++;
		if (r.Boolean("showOnMap"))
			shownSeen++;
	}

	// A rule the parser did not understand would judge items differently from
	// the filter that recorded them, and quietly.
	CHECK(filterDiagnostics.ignored == 0);

	// Worth asserting rather than assuming: a replay that silently stopped
	// reading items would pass every check above by doing nothing.
	CHECK(replayed > 800);
	CHECK(blockedSeen > 100);
	CHECK(shownSeen > 100);

	for (std::map<std::string, RuleSet*>::iterator it = ruleSets.begin();
			it != ruleSets.end(); ++it)
		delete it->second;
}
