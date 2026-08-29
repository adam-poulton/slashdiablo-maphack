#include "ItemDisplay.h"
#include "ItemFactsLive.h"
#include "Item.h"
#include "../../D2Helpers.h"



std::map<std::string, int> UnknownItemCodes;
vector<pair<string, string>> rules;
map<string, string> condition_group;
bool OrderedFiltering = false;
vector<Rule*> RuleList;
vector<Rule*> NameRuleList;
vector<Rule*> DescRuleList;
vector<Rule*> MapRuleList;
vector<Rule*> DoNotBlockRuleList;
vector<Rule*> IgnoreRuleList;



// Find the item description. This code is called only when there's a cache miss
string ItemDescLookupCache::make_cached_T(UnitItemInfo *uInfo) {
	LiveContext context;
	string new_name;
	for (vector<Rule*>::const_iterator it = this->RuleList.begin(); it != this->RuleList.end(); it++) {
		if ((*it)->Evaluate(*uInfo->facts, context.Context())) {
			SubstituteNameVariables(uInfo, new_name, (*it)->action.description);
			if ((*it)->action.stopProcessing) {
				break;
			}
		}
	}
	return new_name;
}

string ItemDescLookupCache::to_str(const string &name) {
	size_t start_pos = 0;
	std::string itemName(name);
	while ((start_pos = itemName.find('\n', start_pos)) != std::string::npos) {
		itemName.replace(start_pos, 1, " - ");
		start_pos += 3;
	}
	return itemName;
}

// Find the item name. This code is called only when there's a cache miss
string ItemNameLookupCache::make_cached_T(UnitItemInfo *uInfo, const string &name) {
	LiveContext context;
	string new_name(name);
	for (vector<Rule*>::const_iterator it = this->RuleList.begin(); it != this->RuleList.end(); it++) {
		if ((*it)->Evaluate(*uInfo->facts, context.Context())) {
			SubstituteNameVariables(uInfo, new_name, (*it)->action.name);
			if ((*it)->action.stopProcessing) {
				break;
			}
		}
	}
	// if the item is on the ignore list and nothing outranks it, warn the user that this item is normally blocked
	unsigned int ignore_index = ignore_cache.Get(uInfo);
	if (ignore_index != NO_RULE_MATCH) {
		unsigned int keep_index = do_not_block_cache.Get(uInfo);
		// actions come back in config order, so the first one with a map action is the earliest
		for (auto &action : map_action_cache.Get(uInfo)) {
			if (action.colorOnMap != UNDEFINED_COLOR ||
				action.borderColor != UNDEFINED_COLOR ||
				action.dotColor != UNDEFINED_COLOR ||
				action.pxColor != UNDEFINED_COLOR ||
				action.lineColor != UNDEFINED_COLOR) {
				if (action.index < keep_index)
					keep_index = action.index;
				break;
			}

		}
		if (IsItemBlocked(ignore_index, keep_index)) return new_name + " [blocked]";
	}
	return new_name;
}

string ItemNameLookupCache::to_str(const string &name) {
	size_t start_pos = 0;
	std::string itemName(name);
	while ((start_pos = itemName.find('\n', start_pos)) != std::string::npos) {
		itemName.replace(start_pos, 1, " - ");
		start_pos += 3;
	}
	return itemName;
}

vector<Action> MapActionLookupCache::make_cached_T(UnitItemInfo *uInfo) {
	LiveContext context;
	vector<Action> actions;
	for (vector<Rule*>::const_iterator it = this->RuleList.begin(); it != this->RuleList.end(); it++) {
		if ((*it)->Evaluate(*uInfo->facts, context.Context())) {
			actions.push_back((*it)->action);
		}
	}
	return actions;
}

string MapActionLookupCache::to_str(const vector<Action> &actions) {
	string name;
	for (auto &action : actions) {
		name += action.name + " ";
	}
	return name;
}

unsigned int IgnoreLookupCache::make_cached_T(UnitItemInfo *uInfo) {
	LiveContext context;
	for (vector<Rule*>::const_iterator it = this->RuleList.begin(); it != this->RuleList.end(); it++) {
		if ((*it)->Evaluate(*uInfo->facts, context.Context())) {
			return (*it)->action.index;
		}
	}
	return NO_RULE_MATCH;
}

string IgnoreLookupCache::to_str(const unsigned int &index) {
	return index == NO_RULE_MATCH ? "no match" : ("matched rule " + std::to_string(index));
}

// Decide whether an item is hidden, given the index of the first rule that wants
// it hidden and the index of the first rule that wants it kept (either a map
// action or a whitelisted name). With ordered filtering off, any keeper wins
// regardless of where it sits in the file; with it on, the earlier rule wins.
bool IsItemBlocked(unsigned int ignore_index, unsigned int keep_index) {
	if (ignore_index == NO_RULE_MATCH)
		return false;
	if (OrderedFiltering)
		return ignore_index < keep_index;
	return keep_index == NO_RULE_MATCH;
}

// least recently used cache for storing a limited number of item names
ItemDescLookupCache item_desc_cache(DescRuleList);
ItemNameLookupCache item_name_cache(NameRuleList);
MapActionLookupCache map_action_cache(MapRuleList);
IgnoreLookupCache do_not_block_cache(DoNotBlockRuleList);
IgnoreLookupCache ignore_cache(IgnoreRuleList);

void GetItemName(UnitItemInfo *uInfo, string &name) {
	string new_name = item_name_cache.Get(uInfo, name);
	name.assign(new_name);
}

// Number of sockets that are filled (gems, runes or jewels inserted into the item).
// Socketed items are held in the parent item's own inventory.
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


void removeSubstrs(string& s, const string& p) {
	string::size_type n = p.length();
	for (string::size_type i = s.find(p); i != string::npos; i = s.find(p))
		s.erase(i, n);
}

std::string without_invis_chars(const std::string &name) {
	string wo_invis_chars(name);
	ColorReplace colors[] = {
		MAP_COLOR_REPLACEMENTS
	};
	for (int n = 0; n < sizeof(colors) / sizeof(colors[0]); n++) {
		removeSubstrs(wo_invis_chars, "%" + colors[n].key + "%");
	}
	removeSubstrs(wo_invis_chars, " ");
	return wo_invis_chars;
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
static ItemFilterSettings ReadFilterSettings() {
	static std::map<std::string, std::string> classSkills;
	static std::map<std::string, std::string> tabSkills;
	BH::itemConfig->ReadAssoc("ClassSkillsList", classSkills);
	BH::itemConfig->ReadAssoc("TabSkillsList", tabSkills);

	ItemFilterSettings settings;
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
			bool has_map_action = false;
			bool has_desc = false;
			bool has_name = false;
			if (without_invis_chars(r->action.description).length() > 0) {
				DescRuleList.push_back(r);
				has_desc = true;
			}
			if (r->action.colorOnMap != UNDEFINED_COLOR ||
					r->action.borderColor != UNDEFINED_COLOR ||
					r->action.dotColor != UNDEFINED_COLOR ||
					r->action.pxColor != UNDEFINED_COLOR ||
					r->action.lineColor != UNDEFINED_COLOR) {
				MapRuleList.push_back(r);
				has_map_action = true;
			}
			if (without_invis_chars(r->action.name).length() > 0) {
				NameRuleList.push_back(r);
				// this is a bit of a hack. the idea is not to block items that have a name specified. Items with a map action are
				// already not blocked, so we make another rule list for those with a name and not a map action. Note the name must
				// not use CONTINUE. If item display line uses continue, then the item can still be blocked by a matching ignore
				// item display line.
				if (r->action.stopProcessing && !has_map_action)
					DoNotBlockRuleList.push_back(r); // if we have a non-blank name and no continue, we don't want to block
				has_name = true;
			}
			if (!has_map_action && !has_name && !has_desc && r->action.stopProcessing) {
				IgnoreRuleList.push_back(r);
			}
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
