#pragma once
#include "../../Constants.h"
#include "../../D2Ptrs.h"
#include "../../Config.h"
#include "../../MPQInit.h"
#include "../../BH.h"
#include <cstdlib>
#include <regex>
#include "../../RuleLookupCache.h"
#include "ItemFacts.h"

#include "ItemFilter.h"

extern std::map<std::string, int> UnknownItemCodes;


class ItemDescLookupCache : public RuleLookupCache<string> {
	string make_cached_T(UnitItemInfo *uInfo) override;
	string to_str(const string &name) override;

		public:
		ItemDescLookupCache(const std::vector<Rule*> &RuleList) :
			RuleLookupCache<string>(RuleList) {}
};

class ItemNameLookupCache : public RuleLookupCache<string, const string &> {
	string make_cached_T(UnitItemInfo *uInfo, const string &name) override;
	string to_str(const string &name) override;

		public:
		ItemNameLookupCache(const std::vector<Rule*> &RuleList) :
			RuleLookupCache<string, const string&>(RuleList) {}
};

class MapActionLookupCache : public RuleLookupCache<vector<Action>> {
	vector<Action> make_cached_T(UnitItemInfo *uInfo) override;
	string to_str(const vector<Action> &actions);

		public:
		MapActionLookupCache(const std::vector<Rule*> &RuleList) :
			RuleLookupCache<vector<Action>>(RuleList) {}
};

// Returns the index of the first matching rule, or NO_RULE_MATCH if none matched.
class IgnoreLookupCache : public RuleLookupCache<unsigned int> {
	unsigned int make_cached_T(UnitItemInfo *uInfo) override;
	string to_str(const unsigned int &index);

		public:
		IgnoreLookupCache(const std::vector<Rule*> &RuleList) :
			RuleLookupCache<unsigned int>(RuleList) {}
};

extern vector<Rule*> RuleList;
extern vector<Rule*> NameRuleList;
extern vector<Rule*> DescRuleList;
extern vector<Rule*> MapRuleList;
extern vector<Rule*> DoNotBlockRuleList;
extern vector<Rule*> IgnoreRuleList;
extern vector<pair<string, string>> rules;

extern ItemDescLookupCache item_desc_cache;
extern ItemNameLookupCache item_name_cache;
extern MapActionLookupCache map_action_cache;
extern IgnoreLookupCache do_not_block_cache;
extern IgnoreLookupCache ignore_cache;
extern map<string, string> condition_group;
extern bool OrderedFiltering;

namespace ItemDisplay {
	void InitializeItemRules();
	void UninitializeItemRules();
	bool UntestedSettingsUsed();
}
void HandleUnknownItemCode(char *code, char *tag);
void GetItemName(UnitItemInfo *uInfo, string &name);
void SubstituteNameVariables(UnitItemInfo *uInfo, string &name, const string &action_name);
BYTE GetRequiredLevel(UnitAny* item);
