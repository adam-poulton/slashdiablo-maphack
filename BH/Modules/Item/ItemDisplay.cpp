#include "ItemDisplay.h"
#include "ItemFactsLive.h"
#include "Item.h"
#include "../../D2Helpers.h"
#include "../../StampedCache.h"
#include <optional>
#include <utility>



std::map<std::string, int> UnknownItemCodes;
vector<pair<string, string>> rules;
map<string, string> condition_group;
vector<Rule*> RuleList;
vector<Rule*> NameRuleList;
vector<Rule*> DescRuleList;
vector<Rule*> MapRuleList;
vector<Rule*> DoNotBlockRuleList;
vector<Rule*> IgnoreRuleList;



/*
 * What the rules make of one item, worked out in the halves it is asked for.
 *
 * The map, do-not-block and ignore lists share a walk and are answered
 * together. The name and the description are each held back until something
 * asks, because working one out means going through the game for a price, a
 * required level and a stat, and most of the items looked at here are only ever
 * asked about the automap.
 */
struct ItemVerdict {
	RuleMatch match;

	bool named;
	string name;

	bool described;
	string description;

	ItemVerdict() : named(false), described(false) {}
};

/*
 * What is kept, for as many items as a busy room holds.
 *
 * Wide because two callers want different things at once: every item on the
 * ground is named on every frame, while the automap asks about every item in
 * every room. Sharing one cache between them means one has to be able to run
 * over the other's items without emptying it.
 */
static StampedCache<ItemVerdict> item_verdicts(512);

/*
 * The world, read at most once however much of a verdict is worked out.
 *
 * Read from the running game rather than passed in, so it is only worth reading
 * when something is actually going to be worked out. An item already judged is
 * answered without the game being asked anything.
 */
namespace {
class World {
public:
	const FilterContext &Now() {
		if (!live)
			live.emplace();
		return live->Context();
	}

private:
	std::optional<LiveContext> live;
};
}  // namespace

// An item is the same item to the cache while its unit id and its flags are
// what they were: identifying it, socketing it or making a runeword changes the
// flags, and everything worked out about it is worked out again.
static ItemVerdict &VerdictFor(UnitItemInfo *uInfo, World &world) {
	DWORD id = uInfo->item->dwUnitId;
	DWORD flags = uInfo->item->pItemData->dwFlags;

	ItemVerdict *held = item_verdicts.Find(id, flags);
	if (held)
		return *held;

	RuleLists lists = { &MapRuleList, &DoNotBlockRuleList, &IgnoreRuleList };
	ItemVerdict verdict;
	verdict.match = MatchRules(lists, *uInfo->facts, world.Now(),
		Item::GetPingLevel());
	return item_verdicts.Hold(id, flags, std::move(verdict));
}

void ResetItemVerdicts() {
	item_verdicts.Clear();
}

/*
 * Forgets what was worked out under a world that has since changed.
 *
 * A verdict is kept against the item it was reached about, and an item is not
 * the only thing a rule reads. CLVL and CRAFTALVL ask how far the character has
 * got and AREAID and AREALVL ask where they are standing, so an item judged
 * before a level-up or in the last area was judged against something that is no
 * longer true.
 *
 * Only those two are looked at because they are the only ones that can change
 * while a game is being played. Which class is playing, the character flags and
 * the difficulty are fixed for a game, and the filter level is a setting, which
 * already forgets everything when it is changed.
 *
 * Once a frame rather than once an item: reading the world for every item would
 * cost more than the walk this saves. A rule reading an arbitrary character
 * stat through CHARSTAT is not covered, since watching every stat a rule might
 * name would be reading the world constantly to catch a case no rule in the
 * shipped config asks for.
 */
void ForgetVerdictsIfWorldChanged() {
	static DWORD judgedAtLevel = 0;
	static DWORD judgedInArea = 0;

	UnitAny *player = D2CLIENT_GetPlayerUnit();
	if (!player)
		return;

	DWORD level = D2COMMON_GetUnitStat(player, STAT_LEVEL, 0);
	DWORD area = GetPlayerArea();
	if (level == judgedAtLevel && area == judgedInArea)
		return;

	judgedAtLevel = level;
	judgedInArea = area;
	item_verdicts.Clear();
}

std::vector<const Action*> GetItemMapActions(UnitItemInfo *uInfo) {
	World world;
	return VerdictFor(uInfo, world).match.mapActions;
}

void GetItemName(UnitItemInfo *uInfo, string &name) {
	World world;
	ItemVerdict &verdict = VerdictFor(uInfo, world);
	if (!verdict.named) {
		// Each rule that has a say writes over what the ones before it made of
		// the name, starting from the name the game gave the item.
		string built(name);
		std::vector<const Action*> actions = MatchingActions(NameRuleList,
			*uInfo->facts, world.Now(), PING_LEVEL_ALL);
		for (unsigned int i = 0; i < actions.size(); i++)
			SubstituteNameVariables(uInfo, built, actions[i]->name);

		// An item a rule would hide is still named while it is in the world,
		// since only a packet can be stopped. Saying so is how a rule set that
		// hides something can be seen to be doing it.
		if (verdict.match.blocked)
			built += " [blocked]";

		verdict.name = built;
		verdict.named = true;
		// Dear enough that the automap's items must not push it out: there are
		// far more of those than there are names on screen.
		item_verdicts.Protect(uInfo->item->dwUnitId);
	}
	name.assign(verdict.name);
}

std::string GetItemDescription(UnitItemInfo *uInfo) {
	World world;
	ItemVerdict &verdict = VerdictFor(uInfo, world);
	if (!verdict.described) {
		string built;
		std::vector<const Action*> actions = MatchingActions(DescRuleList,
			*uInfo->facts, world.Now(), PING_LEVEL_ALL);
		for (unsigned int i = 0; i < actions.size(); i++)
			SubstituteNameVariables(uInfo, built, actions[i]->description);

		verdict.description = built;
		verdict.described = true;
		item_verdicts.Protect(uInfo->item->dwUnitId);
	}
	return verdict.description;
}

void SubstituteNameVariables(UnitItemInfo *uInfo, string &name, const string &action_name) {
	char origName[128], sockets[4], usedsockets[4], code[4], ilvl[4], alvl[4], craft_alvl[4], runename[16] = "", runenum[4] = "0";
	char gemtype[16] = "", gemlevel[16] = "", sellValue[16] = "", statVal[16] = "";
	char lvlreq[4], wpnspd[4], rangeadder[4];

	UnitAny *item = uInfo->item;
	ItemText *txt = D2COMMON_GetItemText(item->dwTxtFileNo);
	char *szCode = txt->szCode;
	code[0] = szCode[0];
	code[1] = szCode[1];
	code[2] = szCode[2];
	code[3] = '\0';
	auto ilvl_int = item->pItemData->dwItemLevel;
	auto alvl_int = GetAffixLevel((BYTE)item->pItemData->dwItemLevel, (BYTE)uInfo->attrs->qualityLevel, uInfo->attrs->magicLevel);
	auto clvl_int = D2COMMON_GetUnitStat(D2CLIENT_GetPlayerUnit(), STAT_LEVEL, 0); 
	sprintf_s(sockets, "%d", D2COMMON_GetUnitStat(item, STAT_SOCKETS, 0));
	sprintf_s(usedsockets, "%d", GetUsedSockets(item));
	sprintf_s(ilvl, "%d", ilvl_int);
	sprintf_s(alvl, "%d", alvl_int);
	sprintf_s(craft_alvl, "%d", GetAffixLevel((BYTE)(ilvl_int/2+clvl_int/2), (BYTE)uInfo->attrs->qualityLevel, uInfo->attrs->magicLevel));
	sprintf_s(origName, "%s", name.c_str());

	sprintf_s(lvlreq, "%d", GetRequiredLevel(uInfo->item));
	sprintf_s(wpnspd, "%d", txt->speed); //Add these as matchable stats too, maybe?
	sprintf_s(rangeadder, "%d", txt->rangeadder);

	UnitAny* pUnit = D2CLIENT_GetPlayerUnit();
	if (pUnit && txt->fQuest == 0) {
		sprintf_s(sellValue, "%d", D2COMMON_GetItemPrice(pUnit, item, D2CLIENT_GetDifficulty(), (DWORD)D2CLIENT_GetQuestInfo(), 0x201, 1));
	}

	if (IsRune(uInfo->attrs)) {
		sprintf_s(runenum, "%d", RuneNumberFromItemCode(code));
		sprintf_s(runename, name.substr(0, name.find(' ')).c_str());
	} else if (IsGem(uInfo->attrs)) {
		sprintf_s(gemlevel, "%s", GetGemLevelString(GetGemLevel(uInfo->attrs)));
		sprintf_s(gemtype, "%s", GetGemTypeString(GetGemType(uInfo->attrs)));
	}

	string baseName = UnicodeToAnsi(D2LANG_GetLocaleText(txt->nLocaleTxtNo));

	ActionReplace replacements[] = {
		{"NAME", origName},
		{"BASENAME", baseName},
		{"SOCKETS", sockets},
		{"USEDSOCKETS", usedsockets},
		{"RUNENUM", runenum},
		{"RUNENAME", runename},
		{"GEMLEVEL", gemlevel},
		{"GEMTYPE", gemtype},
		{"ILVL", ilvl},
		{"ALVL", alvl},
		{"CRAFTALVL", craft_alvl},
		{"LVLREQ", lvlreq},
		{"WPNSPD", wpnspd},
		{"RANGE", rangeadder},
		{"CODE", code},
		{"NL", "\n"},
		{"PRICE", sellValue},
		COLOR_REPLACEMENTS
	};
	name.assign(action_name);
	for (int n = 0; n < sizeof(replacements) / sizeof(replacements[0]); n++) {

		// Revert to non-glide colors here
		if (*p_D2GFX_VideoMode != VIDEO_MODE_GLIDE) {
			if (replacements[n].key == "CORAL") {
				replacements[n].value = "\377c1"; // red
			} else if (replacements[n].key == "SAGE") {
				replacements[n].value = "\377c2"; // green
			} else if (replacements[n].key == "TEAL") {
				replacements[n].value = "\377c3"; // blue
			} else if (replacements[n].key == "LIGHT_GRAY") {
				replacements[n].value = "\377c5"; // gray
			}
		}
		
		while (name.find("%" + replacements[n].key + "%") != string::npos) {
			name.replace(name.find("%" + replacements[n].key + "%"), replacements[n].key.length() + 2, replacements[n].value);
		}
	}

	// stat replacements
	if (name.find("%STAT-") != string::npos) {
		std::regex stat_reg("%STAT-([0-9]{1,4})%", std::regex_constants::ECMAScript);
		std::smatch stat_match;

		while (std::regex_search(name, stat_match, stat_reg)) {
			int stat = stoi(stat_match[1].str(), nullptr, 10);
			statVal[0] = '\0';
			if (stat <= (int)STAT_MAX) {
				auto value = D2COMMON_GetUnitStat(item, stat, 0);
				// Hp and mana need adjusting
				if (stat == 7 || stat == 9)
					value /= 256;
				sprintf_s(statVal, "%d", value);
			}
			name.replace(
					stat_match.prefix().length(),
					stat_match[0].length(), statVal);
		}
	}
}


// Returns the (lowest) level requirement (for any class) of an item
BYTE GetRequiredLevel(UnitAny* item) {
	// Some crafted items can supposedly go above 100, but it's practically the same as 100
	BYTE rlvl = 100;

	// The unit for which the required level is calculated
	UnitAny* character = D2CLIENT_GetPlayerUnit();

	// Extra checks for these as they can have charges
	if (item->pItemData->dwQuality == ITEM_QUALITY_RARE || item->pItemData->dwQuality == ITEM_QUALITY_MAGIC) {

		// Save the original class of the character (0-6)
		DWORD temp = character->dwTxtFileNo;

		// Pretend to be every class once, use the lowest req lvl (for charged items)
		for (DWORD i = 0; i < 7; i++) {

			character->dwTxtFileNo = i;
			BYTE temprlvl = (BYTE)D2COMMON_GetItemLevelRequirement(item, character);

			if (temprlvl < rlvl) {

				rlvl = temprlvl;
				//Only one class will have a lower req than the others, so if a lower one is found we can stop
				if (i > 0) { break; }
			}
		}
		// Go back to being original class
		character->dwTxtFileNo = temp;
	} else {
		rlvl = (BYTE)D2COMMON_GetItemLevelRequirement(item, character);
	}

	return rlvl;
}




namespace ItemDisplay {
	bool item_display_initialized = false;

/*
 * The settings a rule may be built against, read once for a whole rule set
 * rather than by each condition that wants them.
 *
 * The maps are held across calls because Config keeps the address it is given
 * and writes through it when settings are saved.
 */
// What reading a rule could not make sense of, said in game.
class FilterDiagnostics : public ItemFilterDiagnostics {
	void IgnoredToken(const std::string &token) override {
		PrintText(1, "Ignored ItemDisplay token: %s", token.c_str());
	}
	void UnreadableValue(const std::string &token) override {
		PrintText(1, "Error processing value for token: %s", token.c_str());
	}
};
static FilterDiagnostics filterDiagnostics;

static ItemFilterSettings ReadFilterSettings() {
	static std::map<std::string, std::string> classSkills;
	static std::map<std::string, std::string> tabSkills;
	BH::itemConfig->ReadAssoc("ClassSkillsList", classSkills);
	BH::itemConfig->ReadAssoc("TabSkillsList", tabSkills);

	ItemFilterSettings settings;
	// What the tables describe is what a rule is allowed to name.
	settings.statMax = STAT_MAX;
	settings.skillMax = SKILL_MAX;
	settings.diagnostics = &filterDiagnostics;
	for (auto it = classSkills.cbegin(); it != classSkills.cend(); ++it) {
		if (StringToBool(it->second))
			settings.goodClassSkills.push_back(stoi(it->first));
	}
	for (auto it = tabSkills.cbegin(); it != tabSkills.cend(); ++it) {
		if (StringToBool(it->second))
			settings.goodTabSkills.push_back(stoi(it->first));
	}
	return settings;
}

	bool UntestedSettingsUsed() {
		return condition_group.size() > 0;
	}

	void InitializeItemRules() {
		if (item_display_initialized) return;
		if (!IsInitialized()){
			return;
		}

		item_display_initialized = true;
		rules.clear();
		ResetCaches();

		condition_group.clear();
		BH::itemConfig->ReadAssoc("ConditionGroup", condition_group);

		ItemFilterSettings settings = ReadFilterSettings();

		BH::itemConfig->ReadMapList("ItemDisplay", rules);
		for (unsigned int i = 0; i < rules.size(); i++) {
			string buf;
			stringstream ss(rules[i].first);
			vector<string> tokens;
			while (ss >> buf) {
				// check if buf matches any user idendified strings, and replace it if so
				// todo: make config groups nestable?
				// the group token has to be surrounded by whitespace
				// e.g. `the_group && other_group` works but not `(the_group)`
				if (condition_group.count(buf)) {

					string buf2;
					stringstream ssg(condition_group[buf]);

					// enclose group with parens
					tokens.push_back("(");

					while (ssg >> buf2) {
						tokens.push_back(buf2);
					}
					tokens.push_back(")");
				}
				else {
					tokens.push_back(buf);
				}
			}

			LastConditionType = CT_None;
			vector<Condition*> RawConditions;
			for (vector<string>::iterator tok = tokens.begin(); tok < tokens.end(); tok++) {
				Condition::BuildConditions(RawConditions, (*tok), settings);
			}
			Rule *r = new Rule(RawConditions, &(rules[i].second));
			r->action.index = i;

			RuleList.push_back(r);
			RulePlacement placement = PlaceRule(*r);
			if (placement.description) DescRuleList.push_back(r);
			if (placement.map) MapRuleList.push_back(r);
			if (placement.name) NameRuleList.push_back(r);
			if (placement.doNotBlock) DoNotBlockRuleList.push_back(r);
			if (placement.ignore) IgnoreRuleList.push_back(r);
		}
		cout << "Finished initializing item rules" << endl << endl;
	}

	void UninitializeItemRules() {
		// RuleList contains every created rule. MapRuleList and IgnoreRuleList have a subset of rules.
		// Deleting objects in RuleList is sufficient.
		if (item_display_initialized) {
			for (Rule *r : RuleList) {
				for (Condition *condition : r->conditions) {
					delete condition;
				}
				delete r;
			}
		}
		item_display_initialized = false;
		ResetCaches();
		RuleList.clear();
		NameRuleList.clear();
		DescRuleList.clear();
		MapRuleList.clear();
		DoNotBlockRuleList.clear();
		IgnoreRuleList.clear();
	}
}

void HandleUnknownItemCode(char *code, char *tag) {
	// If the MPQ files arent loaded yet then this is expected
	if (!IsInitialized()){
		return;
	}

	// Avoid spamming endlessly
	if (UnknownItemCodes.size() > 10 || (*BH::MiscToggles2)["Allow Unknown Items"].state) {
		return;
	}
	if (UnknownItemCodes.find(code) == UnknownItemCodes.end()) {
		PrintText(1, "Unknown item code %s: %c%c%c\n", tag, code[0], code[1], code[2]);
		UnknownItemCodes[code] = 1;
	}
}
