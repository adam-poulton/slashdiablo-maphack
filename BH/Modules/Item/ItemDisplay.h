#pragma once
#include "../../Constants.h"
#include "../../D2Ptrs.h"
#include "../../Config.h"
#include "../../MPQInit.h"
#include "../../BH.h"
#include <cstdlib>
#include <regex>
#include "ItemFacts.h"

#include "ItemFilter.h"

extern std::map<std::string, int> UnknownItemCodes;

extern vector<Rule*> RuleList;
extern vector<Rule*> NameRuleList;
extern vector<Rule*> DescRuleList;
extern vector<Rule*> MapRuleList;
extern vector<Rule*> DoNotBlockRuleList;
extern vector<Rule*> IgnoreRuleList;
extern vector<pair<string, string>> rules;

extern map<string, string> condition_group;

namespace ItemDisplay {
	void InitializeItemRules();
	void UninitializeItemRules();
	bool UntestedSettingsUsed();
}
void HandleUnknownItemCode(char *code, char *tag);

/*
 * What the rules make of an item in the world.
 *
 * Each is worked out once for an item and kept until the item changes. They are
 * three questions rather than one because they are wanted in different places
 * and at different rates: every item on the ground is named on every frame,
 * every item in every room is asked about the automap on every frame, and a
 * description is wanted only for the one item being looked at.
 *
 * The map actions come back as a copy of a short list of pointers rather than
 * as a reference into what is kept, because naming an item goes back through
 * the game and returns here, and what is kept may have moved by then.
 */
void GetItemName(UnitItemInfo *uInfo, string &name);
std::string GetItemDescription(UnitItemInfo *uInfo);
std::vector<const Action*> GetItemMapActions(UnitItemInfo *uInfo);

// Forgets what was worked out about every item, for when the rules or the
// settings they were worked out under have changed.
void ResetItemVerdicts();

// The same, for the parts of the world a rule reads that change while a game is
// being played: how far the character has got and where they are standing.
void ForgetVerdictsIfWorldChanged();
void SubstituteNameVariables(UnitItemInfo *uInfo, string &name, const string &action_name);
BYTE GetRequiredLevel(UnitAny* item);
