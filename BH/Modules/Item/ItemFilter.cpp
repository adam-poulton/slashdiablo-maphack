#include "ItemFilter.h"
#include "../../ItemTables.h"
#include <regex>
#include <sstream>
#include "../../StringUtil.h"

#include "ItemFactsPacket.h"

// The code moved here was written under a using directive it inherited from
// what it used to include, and is left reading as it did.
using namespace std;

/*
 * Judging an item, and reading the rules an item is judged by.
 *
 * Nothing here reaches into the game. What a condition needs that is not in the
 * rule's own text arrives as facts about the item and about the world, and the
 * two questions that can only be put to an item that exists are asked through
 * an interface the game answers behind. That is what lets a rule be read and an
 * item be judged with no client running, which is what the recorded decisions
 * in the fixtures are replayed against.
 */

SkillReplace skills[] = {
	COMBO_STATS
};

BYTE LastConditionType;


TrueCondition *trueCondition = new TrueCondition();
FalseCondition *falseCondition = new FalseCondition();

// Helper function to get a list of strings
vector<string> split(const string &s, char delim) {
	vector<string> result;
	stringstream ss(s);
	string item;
	while (getline(ss, item, delim)) {
		result.push_back(item);
	}
	return result;
}

char* GemLevels[] = {
	"NONE",
	"Chipped",
	"Flawed",
	"Normal",
	"Flawless",
	"Perfect"
};

char* GemTypes[] = {
	"NONE",
	"Amethyst",
	"Diamond",
	"Emerald",
	"Ruby",
	"Sapphire",
	"Topaz",
	"Skull"
};

bool IsGem(ItemAttributes *attrs) {
	return (attrs->flags2 & ITEM_GROUP_GEM) > 0;
}

BYTE GetGemLevel(ItemAttributes *attrs) {
	if (attrs->flags2 & ITEM_GROUP_CHIPPED) {
		return 1;
	} else if (attrs->flags2 & ITEM_GROUP_FLAWED) {
		return 2;
	} else if (attrs->flags2 & ITEM_GROUP_REGULAR) {
		return 3;
	} else if (attrs->flags2 & ITEM_GROUP_FLAWLESS) {
		return 4;
	} else if (attrs->flags2 & ITEM_GROUP_PERFECT) {
		return 5;
	}
	return 0;
}

char *GetGemLevelString(BYTE level) {
	return GemLevels[level];
}

BYTE GetGemType(ItemAttributes *attrs) {
	if (attrs->flags2 & ITEM_GROUP_AMETHYST) {
		return 1;
	} else if (attrs->flags2 & ITEM_GROUP_DIAMOND) {
		return 2;
	} else if (attrs->flags2 & ITEM_GROUP_EMERALD) {
		return 3;
	} else if (attrs->flags2 & ITEM_GROUP_RUBY) {
		return 4;
	} else if (attrs->flags2 & ITEM_GROUP_SAPPHIRE) {
		return 5;
	} else if (attrs->flags2 & ITEM_GROUP_TOPAZ) {
		return 6;
	} else if (attrs->flags2 & ITEM_GROUP_SKULL) {
		return 7;
	}
	return 0;
}

char *GetGemTypeString(BYTE type) {
	return GemTypes[type];
}

bool IsRune(ItemAttributes *attrs) {
	return (attrs->flags2 & ITEM_GROUP_RUNE) > 0;
}

BYTE RuneNumberFromItemCode(char *code){
	return (BYTE)(((code[1] - '0') * 10) + code[2] - '0');
}
BYTE GetAffixLevel(BYTE ilvl, BYTE qlvl, BYTE mlvl) {
	if (ilvl > 99) {
		ilvl = 99;
	}
	if (qlvl > ilvl) {
		ilvl = qlvl;
	}
	if (mlvl > 0) {
		return ilvl + mlvl > 99 ? 99 : ilvl + mlvl;
	}
	return ((ilvl) < (99 - ((qlvl)/2)) ? (ilvl) - ((qlvl)/2) : (ilvl) * 2 - 99);
}
BYTE GetOperation(string *op) {
	if (op->length() < 1) {
		return NONE;
	} else if ((*op)[0] == '=') {
		return EQUAL;
	} else if ((*op)[0] == '<') {
		return LESS_THAN;
	} else if ((*op)[0] == '>') {
		return GREATER_THAN;
	}
	return NONE;
}

unsigned int GetItemCodeIndex(char codeChar) {
	// Characters '0'-'9' map to 0-9, and a-z map to 10-35
	return codeChar - (codeChar < 90 ? 48 : 87);
}

bool IntegerCompare(unsigned int Lvalue, BYTE operation, unsigned int Rvalue) {
	switch (operation) {
	case EQUAL:
		return Lvalue == Rvalue;
	case GREATER_THAN:
		return Lvalue > Rvalue;
	case LESS_THAN:
		return Lvalue < Rvalue;
	default:
		return false;
	}
}
Rule::Rule(vector<Condition*> &inputConditions, string *str) {
	needsLiveItem = false;
	for (unsigned int i = 0; i < inputConditions.size(); i++) {
		if (inputConditions[i]->NeedsLiveItem())
			needsLiveItem = true;
	}
	Condition::ProcessConditions(inputConditions, conditions);
	if (str != NULL) BuildAction(str, &action);
	conditionStack.reserve(conditions.size()); // TODO: too large?
}

void BuildAction(string *str, Action *act) {
	act->name = string(str->c_str());

	// upcase all text in a %replacement_string%
	// for some reason \w wasn't catching _, so I added it to the groups
	std::regex replace_reg(
			R"(^(?:(?:%[^%]*%)|[^%])*%((?:\w|_|-)*?[a-z]+?(?:\w|_|-)*?)%)",
			std::regex_constants::ECMAScript);
	std::smatch replace_match;
	while (std::regex_search(act->name, replace_match, replace_reg)) {
		auto offset = replace_match[1].first - act->name.begin();
		std::transform(
				replace_match[1].first,
				replace_match[1].second,
				act->name.begin() + offset,
				toupper
				);
	}

	// new stuff:
	act->borderColor = ParseMapColor(act, "BORDER");
	act->colorOnMap = ParseMapColor(act, "MAP");
	act->dotColor = ParseMapColor(act, "DOT");
	act->pxColor = ParseMapColor(act, "PX");
	act->lineColor = ParseMapColor(act, "LINE");
	act->notifyColor = ParseMapColor(act, "NOTIFY");
	act->pingLevel = ParsePingLevel(act, "TIER");
	act->description = ParseDescription(act);

	size_t noTracking = act->name.find("%NOTRACK%");
	if (noTracking != string::npos) {
		act->name.replace(noTracking, 9, "");
		act->noTracking = true;
	}

	// legacy support:
	size_t map = act->name.find("%MAP%");
	if (map != string::npos) {
		int mapColor = MAP_COLOR_WHITE;
		size_t lastColorPos = 0;
		ColorReplace colors[] = {
			MAP_COLOR_REPLACEMENTS
		};
		for (int n = 0; n < sizeof(colors) / sizeof(colors[0]); n++) {
			size_t pos = act->name.find("%" + colors[n].key + "%");
			if (pos != string::npos && pos < map && pos >= lastColorPos) {
				mapColor = colors[n].value;
				lastColorPos = pos;
			}
		}

		act->name.replace(map, 5, "");
		act->colorOnMap = mapColor;
		if (act->borderColor == UNDEFINED_COLOR)
			act->borderColor = act->colorOnMap;
	}

	size_t done = act->name.find("%CONTINUE%");
	if (done != string::npos) {
		act->name.replace(done, 10, "");
		act->stopProcessing = false;
	}
}

string ParseDescription(Action *act) {
	size_t l_idx = act->name.find("{");
	size_t r_idx = act->name.find("}");
	if (l_idx == string::npos || r_idx == string::npos || l_idx > r_idx) return "";
	size_t start_idx = l_idx + 1;
	size_t len = r_idx - start_idx;
	string desc_string = act->name.substr(start_idx, len);
	act->name.replace(l_idx, len+2, "");
	return desc_string;
}

int ParseMapColor(Action *act, const string& key_string) {
	std::regex pattern("%" + key_string + "-([a-f0-9]{1,4})%",
		std::regex_constants::ECMAScript | std::regex_constants::icase);
	int color = UNDEFINED_COLOR;
	std::smatch the_match;

	if (std::regex_search(act->name, the_match, pattern)) {
		color = stoi(the_match[1].str(), nullptr, 16);
		act->name.replace(
				the_match.prefix().length(),
				the_match[0].length(), "");
	}
	return color;
}

int ParsePingLevel(Action *act, const string& key_string) {
	std::regex pattern("%" + key_string + "-([0-9])%",
		std::regex_constants::ECMAScript | std::regex_constants::icase);
	int ping_level = 0;
	std::smatch the_match;

	if (std::regex_search(act->name, the_match, pattern)) {
		ping_level = stoi(the_match[1].str());
		act->name.replace(
				the_match.prefix().length(),
				the_match[0].length(), "");
	}
	return ping_level;
}

const string Condition::tokenDelims = "<=>";

// Implements the shunting-yard algorithm to evaluate condition expressions
// Returns a vector of conditions in Reverse Polish Notation
void Condition::ProcessConditions(vector<Condition*> &inputConditions, vector<Condition*> &processedConditions) {
	vector<Condition*> conditionStack;
	unsigned int size = inputConditions.size();
	for (unsigned int c = 0; c < size; c++) {
		Condition *input = inputConditions[c];
		if (input->conditionType == CT_Operand) {
			processedConditions.push_back(input);
		} else if (input->conditionType == CT_BinaryOperator || input->conditionType == CT_NegationOperator) {
			bool go = true;
			while (go) {
				if (conditionStack.size() > 0) {
					Condition *stack = conditionStack.back();
					if ((stack->conditionType == CT_NegationOperator || stack->conditionType == CT_BinaryOperator) &&
						input->conditionType == CT_BinaryOperator) {
						conditionStack.pop_back();
						processedConditions.push_back(stack);
					} else {
						go = false;
					}
				} else {
					go = false;
				}
			}
			conditionStack.push_back(input);
		} else if (input->conditionType == CT_LeftParen) {
			conditionStack.push_back(input);
		} else if (input->conditionType == CT_RightParen) {
			bool foundLeftParen = false;
			while (conditionStack.size() > 0 && !foundLeftParen) {
				Condition *stack = conditionStack.back();
				conditionStack.pop_back();
				if (stack->conditionType == CT_LeftParen) {
					foundLeftParen = true;
					break;
				} else {
					processedConditions.push_back(stack);
				}
			}
			if (!foundLeftParen) {
				// TODO: find a way to report error
				return;
			}
		}
	}
	while (conditionStack.size() > 0) {
		Condition *next = conditionStack.back();
		conditionStack.pop_back();
		if (next->conditionType == CT_LeftParen || next->conditionType == CT_RightParen) {
			// TODO: find a way to report error
			break;
		} else {
			processedConditions.push_back(next);
		}
	}
}

// make_count_subrule calls BuildConditon, which creates new Conditions. We free these here.
PartialCondition::~PartialCondition() {
	for (auto rule : rules) {
		for (Condition *condition : rule.conditions) {
			delete condition;
		}
	}
}

void PartialCondition::make_count_subrule(string rule_str,
		const ItemFilterSettings &settings) {
	BYTE LastConditionTypeOld = LastConditionType;
	LastConditionType = CT_None;
	vector<Condition*> RawConditions;
	string buf;
	vector<string> tokens;
	stringstream ss(rule_str);
	while (ss >> buf) {
		tokens.push_back(buf);
	}
	for (auto &token : tokens) {
		//string token(s.substr(last, next-last));
		//PrintText(1, "In BuildConditions. token=%s", token.c_str());
		Condition::BuildConditions(RawConditions, token, settings);
		//PrintText(1, "In BuildConditions. RawConditions.size=%d", RawConditions.size());
	}
	for (auto condition : RawConditions) {
		//PrintText(1, "\t condition type=%d", condition->conditionType);
	}
	Rule rule(RawConditions, NULL);
	rules.push_back(rule);
	LastConditionType = LastConditionTypeOld;
}

void Condition::BuildConditions(vector<Condition*> &conditions, string token,
		const ItemFilterSettings &settings) {
	vector<Condition*> endConditions;
	int i;

	// Since we don't have a real parser, things will break if [!()] appear in
	// the middle of a token (e.g. "(X AND Y)(A AND B)")

	// Look for syntax characters at the beginning of the token
	for (i = 0; i < (int)token.length(); i++) {
		if (token[i] == '!') {
			Condition::AddNonOperand(conditions, new NegationOperator());
		} else if (token[i] == '(') {
			Condition::AddNonOperand(conditions, new LeftParen());
		} else if (token[i] == ')') {
			Condition::AddNonOperand(conditions, new RightParen());
		} else {
			break;
		}
	}
	token.erase(0, i);

	// Look for syntax characters at the end of the token
	for (i = token.length() - 1; i >= 0; i--) {
		if (token[i] == '!') {
			endConditions.insert(endConditions.begin(), new NegationOperator());
		} else if (token[i] == '(') {
			endConditions.insert(endConditions.begin(), new LeftParen());
		} else if (token[i] == ')') {
			endConditions.insert(endConditions.begin(), new RightParen());
		} else {
			break;
		}
	}
	if (i < (int)(token.length() - 1)) {
		token.erase(i + 1, string::npos);
	}

	size_t delPos = token.find_first_of(tokenDelims);
	string key;
	string delim = "";
	int value = 0;
	string valueStr;
	if (delPos != string::npos) {
		key = Trim(token.substr(0, delPos));
		delim = token.substr(delPos, 1);
		valueStr = Trim(token.substr(delPos + 1));
		if (valueStr.length() > 0) {
			stringstream ss(valueStr);
			if ((ss >> value).fail()) {
				// Reported rather than written to a console this has never
				// had: the whole token is abandoned, as it always was.
				if (settings.diagnostics)
					settings.diagnostics->UnreadableValue(token);
				return;
			}
		}
	} else {
		key = token;
	}
	//if (key.compare(0, 5, "COUNT") == 0) PrintText(1, "Matched COUNT, valueStr=%s, value=%d, delim=%s", valueStr.c_str(), value, delim.c_str());
	BYTE operation = GetOperation(&delim);

	unsigned int keylen = key.length();
	if (key == "AND" || key == "&&") {
		Condition::AddNonOperand(conditions, new AndOperator());
	} else if (key == "OR" || key == "||") {
		Condition::AddNonOperand(conditions, new OrOperator());
	} else if (keylen == 3 && !(isupper(key[0]) || isupper(key[1]) || isupper(key[2]))) {
		Condition::AddOperand(conditions, new ItemCodeCondition(key.substr(0, 3).c_str()));
	} else if (key.find('+') != std::string::npos) {
		Condition::AddOperand(conditions, new AddCondition(key, operation, value));
	} else if (key == "ETH") {
		Condition::AddOperand(conditions, new FlagsCondition(ITEM_ETHEREAL));
	} else if (key == "SOCK") {
		Condition::AddOperand(conditions, new ItemStatCondition(STAT_SOCKETS, 0, operation, value));
	} else if (key == "USEDSOCK") {
		Condition::AddOperand(conditions, new UsedSocketsCondition(operation, value));
	} else if (key.compare(0, 3, "SET") == 0) {
		std::smatch match;
		if (regex_search(key, match, regex("^SET([0-9]{1,4})$")) && match.size() == 2) {
			int id = stoi(match[1], nullptr, 10);
			Condition::AddOperand(conditions, new QualityIdCondition(ITEM_QUALITY_SET, id));
		} else {
			Condition::AddOperand(conditions, new QualityCondition(ITEM_QUALITY_SET));
		}
	} else if (key == "MAG") {
		Condition::AddOperand(conditions, new QualityCondition(ITEM_QUALITY_MAGIC));
	} else if (key == "RARE") {
		Condition::AddOperand(conditions, new QualityCondition(ITEM_QUALITY_RARE));
	} else if (key.compare(0, 3, "UNI") == 0) {
		std::smatch match;
		if (regex_search(key, match, regex("^UNI([0-9]{1,4})$")) && match.size() == 2) {
			int id = stoi(match[1], nullptr, 10);
			Condition::AddOperand(conditions, new QualityIdCondition(ITEM_QUALITY_UNIQUE, id));
		} else {
			Condition::AddOperand(conditions, new QualityCondition(ITEM_QUALITY_UNIQUE));
		}
	} else if (key == "CRAFTALVL") {
		Condition::AddOperand(conditions, new CraftAffixLevelCondition(operation, value));
	} else if (key == "CRAFT") {
		Condition::AddOperand(conditions, new QualityCondition(ITEM_QUALITY_CRAFT));
	} else if (key == "RW") {
		Condition::AddOperand(conditions, new FlagsCondition(ITEM_RUNEWORD));
	} else if (key == "NMAG") {
		Condition::AddOperand(conditions, new NonMagicalCondition());
	} else if (key == "SUP") {
		Condition::AddOperand(conditions, new QualityCondition(ITEM_QUALITY_SUPERIOR));
	} else if (key == "INF") {
		Condition::AddOperand(conditions, new QualityCondition(ITEM_QUALITY_INFERIOR));
	} else if (key == "NORM") {
		Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_NORMAL));
	} else if (key == "EXC") {
		Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_EXCEPTIONAL));
	} else if (key == "ELT") {
		Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_ELITE));
	} else if (key == "ID") {
		Condition::AddOperand(conditions, new FlagsCondition(ITEM_IDENTIFIED));
	} else if (key == "ILVL") {
		Condition::AddOperand(conditions, new ItemLevelCondition(operation, value));
	} else if (key == "QLVL") {
		Condition::AddOperand(conditions, new QualityLevelCondition(operation, value));
	} else if (key == "ALVL") {
		Condition::AddOperand(conditions, new AffixLevelCondition(operation, value));
	} else if (key == "CLVL") {
		Condition::AddOperand(conditions, new CharStatCondition(STAT_LEVEL, 0, operation, value));
	} else if (key == "AREALVL") {
		Condition::AddOperand(conditions, new AreaLevelCondition(operation, value));
	} else if (key == "AREAID") {
		Condition::AddOperand(conditions, new AreaIdCondition(operation, value));
	} else if (key == "FILTLVL") {
		Condition::AddOperand(conditions, new FilterLevelCondition(operation, value));
	} else if (key == "DIFF") {
		Condition::AddOperand(conditions, new DifficultyCondition(operation, value));
	} else if (key == "RUNE") {
		Condition::AddOperand(conditions, new RuneCondition(operation, value));
	} else if (key == "GOLD") {
		Condition::AddOperand(conditions, new GoldCondition(operation, value));
	} else if (key == "GEMTYPE") {
		Condition::AddOperand(conditions, new GemTypeCondition(operation, value));
	} else if (key == "GEM") {
		Condition::AddOperand(conditions, new GemLevelCondition(operation, value));
	} else if (key == "ED") {
		Condition::AddOperand(conditions, new EDCondition(operation, value));
	} else if (key == "DEF") {
		Condition::AddOperand(conditions, new ItemStatCondition(STAT_DEFENSE, 0, operation, value));
	} else if (key == "MAXDUR") {
		Condition::AddOperand(conditions, new DurabilityCondition(operation, value));
	} else if (key == "RES") {
		Condition::AddOperand(conditions, new ResistAllCondition(operation, value));
	} else if (key == "FRES") {
		Condition::AddOperand(conditions, new ItemStatCondition(STAT_FIRERESIST, 0, operation, value));
	} else if (key == "CRES") {
		Condition::AddOperand(conditions, new ItemStatCondition(STAT_COLDRESIST, 0, operation, value));
	} else if (key == "LRES") {
		Condition::AddOperand(conditions, new ItemStatCondition(STAT_LIGHTNINGRESIST, 0, operation, value));
	} else if (key == "PRES") {
		Condition::AddOperand(conditions, new ItemStatCondition(STAT_POISONRESIST, 0, operation, value));
	} else if (key == "IAS") {
		Condition::AddOperand(conditions, new ItemStatCondition(STAT_IAS, 0, operation, value));
	} else if (key == "FCR") {
		Condition::AddOperand(conditions, new ItemStatCondition(STAT_FASTERCAST, 0, operation, value));
	} else if (key == "FHR") {
		Condition::AddOperand(conditions, new ItemStatCondition(STAT_FASTERHITRECOVERY, 0, operation, value));
	} else if (key == "FBR") {
		Condition::AddOperand(conditions, new ItemStatCondition(STAT_FASTERBLOCK, 0, operation, value));
	} else if (key == "LIFE") {
		// For unknown reasons, the game's internal HP stat is 256 for every 1 displayed on item
		Condition::AddOperand(conditions, new ItemStatCondition(STAT_MAXHP, 0, operation, value * 256));
	} else if (key == "MANA") {
		// For unknown reasons, the game's internal Mana stat is 256 for every 1 displayed on item
		Condition::AddOperand(conditions, new ItemStatCondition(STAT_MAXMANA, 0, operation, value * 256));
	} else if (key == "GOODSK") {
		Condition::AddOperand(conditions, new SkillListCondition(operation, CLASS_SKILLS, value, settings));
	}else if (key == "GOODTBSK") {
		Condition::AddOperand(conditions, new SkillListCondition(operation, CLASS_TAB_SKILLS, value, settings));
	} else if (key == "FOOLS") {
		Condition::AddOperand(conditions, new FoolsCondition());
	} else if (key == "LVLREQ") {
		Condition::AddOperand(conditions, new RequiredLevelCondition(operation, value));
	} else if (key == "ARPER") {
		Condition::AddOperand(conditions, new ItemStatCondition(STAT_TOHITPERCENT, 0, operation, value));
	} else if (key == "MFIND") {
		Condition::AddOperand(conditions, new ItemStatCondition(STAT_MAGICFIND, 0, operation, value));
	} else if (key == "GFIND") {
		Condition::AddOperand(conditions, new ItemStatCondition(STAT_GOLDFIND, 0, operation, value));
	} else if (key == "STR") {
		Condition::AddOperand(conditions, new ItemStatCondition(STAT_STRENGTH, 0, operation, value));
	} else if (key == "DEX") {
		//PrintText(1, "In BuildCondition. Creating DEX condition with value=%d", value);
		Condition::AddOperand(conditions, new ItemStatCondition(STAT_DEXTERITY, 0, operation, value));
	} else if (key == "FRW") {
		Condition::AddOperand(conditions, new ItemStatCondition(STAT_FASTERRUNWALK, 0, operation, value));
	} else if (key == "MINDMG") {
		Condition::AddOperand(conditions, new ItemStatCondition(STAT_MINIMUMDAMAGE, 0, operation, value));
	} else if (key == "MAXDMG") {
		Condition::AddOperand(conditions, new ItemStatCondition(STAT_MAXIMUMDAMAGE, 0, operation, value));
	} else if (key == "AR" && keylen == 2) {
		Condition::AddOperand(conditions, new ItemStatCondition(STAT_ATTACKRATING, 0, operation, value));
	} else if (key == "DTM") {
		Condition::AddOperand(conditions, new ItemStatCondition(STAT_DAMAGETOMANA, 0, operation, value));
	} else if (key == "MAEK") {
		Condition::AddOperand(conditions, new ItemStatCondition(STAT_MANAAFTEREACHKILL, 0, operation, value));
	} else if (key == "REPLIFE") {
		Condition::AddOperand(conditions, new ItemStatCondition(STAT_REPLENISHLIFE, 0, operation, value));
	} else if (key == "REPQUANT") {
		Condition::AddOperand(conditions, new ItemStatCondition(STAT_REPLENISHESQUANTITY, 0, operation, value));
	} else if (key == "REPAIR") {
		Condition::AddOperand(conditions, new ItemStatCondition(STAT_REPAIRSDURABILITY, 0, operation, value));
	} else if (key == "ARMOR") {
		Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_ALLARMOR));
	} else if (key == "BELT") {
		Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_BELT));
	} else if (key == "CHEST") {
		Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_ARMOR));
	} else if (key == "HELM") {
		Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_HELM));
	} else if (key == "SHIELD") {
		Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_SHIELD));
	} else if (key == "GLOVES") {
		Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_GLOVES));
	} else if (key == "BOOTS") {
		Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_BOOTS));
	} else if (key == "CIRC") {
		Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_CIRCLET));
	} else if (key == "DRU") {
		Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_DRUID_PELT));
	} else if (key == "BAR") {
		Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_BARBARIAN_HELM));
	} else if (key == "DIN") {
		Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_PALADIN_SHIELD));
	} else if (key == "NEC") {
		Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_NECROMANCER_SHIELD));
	} else if (key == "SIN") {
		Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_ASSASSIN_KATAR));
	} else if (key == "SOR") {
		Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_SORCERESS_ORB));
	} else if (key == "ZON") {
		Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_AMAZON_WEAPON));
	} else if (key == "AXE") {
		Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_AXE));
	} else if (key == "MACE") {
		Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_MACE));
	} else if (key == "SWORD") {
		Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_SWORD));
	} else if (key == "DAGGER") {
		Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_DAGGER));
	} else if (key == "THROWING") {
		Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_THROWING));
	} else if (key == "JAV") {
		// Javelins don't seem to have ITEM_GROUP_JAVELIN set
		// they are however, throwing spears
		Condition::AddOperand(conditions,
			new ItemGroupCondition(ITEM_GROUP_THROWING | ITEM_GROUP_SPEAR));
	} else if (key == "SPEAR") {
		Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_SPEAR));
	} else if (key == "POLEARM") {
		Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_POLEARM));
	} else if (key == "BOW") {
		Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_BOW));
	} else if (key == "XBOW") {
		Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_CROSSBOW));
	} else if (key == "STAFF") {
		Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_STAFF));
	} else if (key == "WAND") {
		Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_WAND));
	} else if (key == "SCEPTER") {
		Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_SCEPTER));
	} else if (key.compare(0, 2, "EQ") == 0 && keylen == 3) {
		if (key[2] == '1') {
			Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_HELM));
		} else if (key[2] == '2') {
			Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_ARMOR));
		} else if (key[2] == '3') {
			Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_SHIELD));
		} else if (key[2] == '4') {
			Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_GLOVES));
		} else if (key[2] == '5') {
			Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_BOOTS));
		} else if (key[2] == '6') {
			Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_BELT));
		} else if (key[2] == '7') {
			Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_CIRCLET));
		}
	} else if (key.compare(0, 2, "CL") == 0 && keylen == 3) {
		if (key[2] == '1') {
			Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_DRUID_PELT));
		} else if (key[2] == '2') {
			Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_BARBARIAN_HELM));
		} else if (key[2] == '3') {
			Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_PALADIN_SHIELD));
		} else if (key[2] == '4') {
			Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_NECROMANCER_SHIELD));
		} else if (key[2] == '5') {
			Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_ASSASSIN_KATAR));
		} else if (key[2] == '6') {
			Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_SORCERESS_ORB));
		} else if (key[2] == '7') {
			Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_AMAZON_WEAPON));
		}
	} else if (key == "WEAPON") {
		Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_ALLWEAPON));
	} else if (key.compare(0, 2, "WP") == 0) {
		if (keylen >= 3) {
			if (key[2] == '1') {
				if (keylen >= 4 && key[3] == '0') {
					Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_CROSSBOW));
				} else if (keylen >= 4 && key[3] == '1') {
					Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_STAFF));
				} else if (keylen >= 4 && key[3] == '2') {
					Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_WAND));
				} else if (keylen >= 4 && key[3] == '3') {
					Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_SCEPTER));
				} else {
					Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_AXE));
				}
			} else if (key[2] == '2') {
				Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_MACE));
			} else if (key[2] == '3') {
				Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_SWORD));
			} else if (key[2] == '4') {
				Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_DAGGER));
			} else if (key[2] == '5') {
				Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_THROWING));
			} else if (key[2] == '6') {
				// Javelins don't seem to have ITEM_GROUP_JAVELIN set
				// they are however, throwing spears
				Condition::AddOperand(conditions,
					new ItemGroupCondition(ITEM_GROUP_THROWING | ITEM_GROUP_SPEAR));
			} else if (key[2] == '7') {
				Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_SPEAR));
			} else if (key[2] == '8') {
				Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_POLEARM));
			} else if (key[2] == '9') {
				Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_BOW));
			}
		}
	} else if (key.compare(0, 2, "SK") == 0) {
		int num = -1;
		stringstream ss(key.substr(2));
		if ((ss >> num).fail() || num < 0 || num > (int)settings.skillMax) {
			return;
		}
		Condition::AddOperand(conditions, new ItemStatCondition(STAT_SINGLESKILL, num, operation, value));
	} else if (key.compare(0, 2, "OS") == 0) {
		int num = -1;
		stringstream ss(key.substr(2));
		if ((ss >> num).fail() || num < 0 || num > (int)settings.skillMax) {
			return;
		}
		Condition::AddOperand(conditions, new ItemStatCondition(STAT_NONCLASSSKILL, num, operation, value));
	} else if (key.compare(0, 4, "CHSK") == 0) { // skills granted by charges
		int num = -1;
		stringstream ss(key.substr(4));
		if ((ss >> num).fail() || num < 0 || num > (int)settings.skillMax) {
			return;
		}
		Condition::AddOperand(conditions, new ChargedCondition(operation, num, value));
	} else if (key.compare(0, 4, "CLSK") == 0) {
		int num = -1;
		stringstream ss(key.substr(4));
		if ((ss >> num).fail() || num < 0 || num >= CLASS_NA) {
			return;
		}
		Condition::AddOperand(conditions, new ItemStatCondition(STAT_CLASSSKILLS, num, operation, value));
	} else if (key == "ALLSK") {
		Condition::AddOperand(conditions, new ItemStatCondition(STAT_ALLSKILLS, 0, operation, value));
	} else if (key.compare(0, 5, "TABSK") == 0) {
		int num = -1;
		stringstream ss(key.substr(5));
		if ((ss >> num).fail() || num < 0 || num > SKILLTAB_MAX) {
			return;
		}
		Condition::AddOperand(conditions, new ItemStatCondition(STAT_SKILLTAB, num, operation, value));
	} else if (key.compare(0, 4, "STAT") == 0) {
		int num = -1;
		stringstream ss(key.substr(4));
		if ((ss >> num).fail() || num < 0 || num > (int)settings.statMax) {
			return;
		}
		Condition::AddOperand(conditions, new ItemStatCondition(num, 0, operation, value));
	} else if (key.compare(0, 8, "CHARSTAT") == 0) {
		int num = -1;
		stringstream ss(key.substr(8));
		if ((ss >> num).fail() || num < 0 || num > (int)settings.statMax) {
			return;
		}
		Condition::AddOperand(conditions, new CharStatCondition(num, 0, operation, value));
	} else if (key.compare(0, 5, "MULTI") == 0) {

		std::regex multi_reg("([0-9]{1,10}),([0-9]{1,10})",
			std::regex_constants::ECMAScript | std::regex_constants::icase);
		std::smatch multi_match;
		if (std::regex_search(key, multi_match, multi_reg)) {
			int stat1, stat2;
			stat1 = stoi(multi_match[1].str(), nullptr, 10);
			stat2 = stoi(multi_match[2].str(), nullptr, 10);

			Condition::AddOperand(conditions, new ItemStatCondition(stat1, stat2, operation, value));
		}

	} else if (key == "PRICE") {
		Condition::AddOperand(conditions, new ItemPriceCondition(operation, value));
	} else if (key == "XP") {
		Condition::AddOperand(conditions, new PlayerTypeCondition(PLAYER_XP));
	} else if (key == "CLASSIC") {
	Condition::AddOperand(conditions, new PlayerTypeCondition(PLAYER_CLASSIC));
	} else if (key == "AMAZON") {
		Condition::AddOperand(conditions, new CharClassCondition(CLASS_AMA));
	} else if (key == "SORCERESS") {
		Condition::AddOperand(conditions, new CharClassCondition(CLASS_SOR));
	} else if (key == "NECROMANCER") {
		Condition::AddOperand(conditions, new CharClassCondition(CLASS_NEC));
	} else if (key == "PALADIN") {
		Condition::AddOperand(conditions, new CharClassCondition(CLASS_PAL));
	} else if (key == "BARBARIAN") {
		Condition::AddOperand(conditions, new CharClassCondition(CLASS_BAR));
	} else if (key == "DRUID") {
		Condition::AddOperand(conditions, new CharClassCondition(CLASS_DRU));
	} else if (key == "ASSASSIN") {
		Condition::AddOperand(conditions, new CharClassCondition(CLASS_ASN));
	} else if (key.compare(0, 5, "COUNT") == 0) {
		// backup the last condition type
		//PrintText(1, "COUNT match with valueStr=%s", valueStr.c_str());
		int i = 0; // Token index
		string s(valueStr);
		const string delimiter = ","; // Partial conditions are delimited by commas, e.g., COUNT=2,FRES>30,LRES>30,CRES>30
		size_t last = 0; 
		size_t next = 0;
		int min_conditions = 0; // minimum number of conditions required to match
		vector<Rule> rule_vec;
		vector<string> tokens;
		while ((next = s.find(delimiter, last)) != string::npos) {
			if (i==0) {
				stringstream ss(s.substr(last, next-last));
				if ((ss >> min_conditions).fail()) {
					// TODO: Error handling
					return;
				}
				if (min_conditions != value) return; // TODO: Error handling
			} else {
				tokens.push_back(s.substr(last, next-last));
			}
			last = next + 1;
			i++;
		}
		tokens.push_back(s.substr(last));
		// substitue | for a space. this is a workaround since we can't allow spaces in a single token
		for (auto &token : tokens) {
			replace(token.begin(), token.end(), '|', ' ');
		}
		//PrintText(1, "Created PartialCondition with min_conditions=%d and rules size=%d", min_conditions, tokens.size());
		Condition::AddOperand(conditions, new PartialCondition(operation, min_conditions, tokens, settings));
	} else if ( token.length() > 0 ){
		if (settings.diagnostics)
			settings.diagnostics->IgnoredToken(token);
	}
	for (vector<Condition*>::iterator it = endConditions.begin(); it != endConditions.end(); it++) {
		Condition::AddNonOperand(conditions, (*it));
	}
}

// Insert extra AND operators to stay backwards compatible with old config
// that implicitly ANDed all conditions
void Condition::AddOperand(vector<Condition*> &conditions, Condition *cond) {
	if (LastConditionType == CT_Operand || LastConditionType == CT_RightParen) {
		conditions.push_back(new AndOperator());
	}
	conditions.push_back(cond);
	LastConditionType = CT_Operand;
}

void Condition::AddNonOperand(vector<Condition*> &conditions, Condition *cond) {
	if ((cond->conditionType == CT_NegationOperator || cond->conditionType == CT_LeftParen) &&
		(LastConditionType == CT_Operand || LastConditionType == CT_RightParen)) {
		conditions.push_back(new AndOperator());
	}
	conditions.push_back(cond);
	LastConditionType = cond->conditionType;
}

/*
 * One body each, now that an item's stats can be asked for the same way
 * whichever side the item came from.
 *
 * Two of these could not be answered from a packet at all before: the stat a
 * rule counts and the skills it counts were only ever read from a live unit,
 * and the packet half returned false. An item on the ground was judged against
 * a rule that could not match it. They match now.
 */
bool ItemStatCondition::Match(const ItemFacts &facts, const FilterContext &context,
		Condition *arg1, Condition *arg2) const {
	return IntegerCompare(facts.stats->Stat(itemStat, itemStat2), operation, targetStat);
}

bool ResistAllCondition::Match(const ItemFacts &facts, const FilterContext &context,
		Condition *arg1, Condition *arg2) const {
	return (IntegerCompare(facts.stats->Stat(STAT_FIRERESIST, 0), operation, targetStat) &&
			IntegerCompare(facts.stats->Stat(STAT_LIGHTNINGRESIST, 0), operation, targetStat) &&
			IntegerCompare(facts.stats->Stat(STAT_COLDRESIST, 0), operation, targetStat) &&
			IntegerCompare(facts.stats->Stat(STAT_POISONRESIST, 0), operation, targetStat));
}

bool AddCondition::Match(const ItemFacts &facts, const FilterContext &context,
		Condition *arg1, Condition *arg2) const {
	int value = 0;
	for (unsigned int i = 0; i < stats.size(); i++) {
		int one = facts.stats->Stat(stats[i], 0);
		// Life and mana are held at 256 for every point the game shows.
		if (stats[i] == STAT_MAXHP || stats[i] == STAT_MAXMANA)
			one /= 256;
		value += one;
	}
	return IntegerCompare(value, operation, targetStat);
}

bool SkillListCondition::Match(const ItemFacts &facts, const FilterContext &context,
		Condition *arg1, Condition *arg2) const {
	int value = 0;
	if (type == CLASS_SKILLS) {
		for (unsigned int i = 0; i < goodClassSkills.size(); i++)
			value += facts.stats->Stat(STAT_CLASSSKILLS, goodClassSkills[i]);
	} else if (type == CLASS_TAB_SKILLS) {
		for (unsigned int i = 0; i < goodTabSkills.size(); i++)
			value += facts.stats->Stat(STAT_SKILLTAB, goodTabSkills[i]);
	}
	return IntegerCompare(value, operation, targetStat);
}

bool EDCondition::Match(const ItemFacts &facts, const FilterContext &context,
		Condition *arg1, Condition *arg2) const {
	// Enhanced defence on armour, enhanced damage on anything else. Normal
	// enhanced damage carries the same value on the minimum and the maximum, so
	// reading one of them is reading both.
	WORD stat = (facts.attrs->flags & ITEM_GROUP_ALLARMOR) ?
		STAT_ENHANCEDDEFENSE : STAT_ENHANCEDMAXIMUMDAMAGE;

	DWORD value = 0;
	const std::vector<StatEntry>& entries = facts.stats->Stats();
	for (unsigned int i = 0; i < entries.size(); i++) {
		if (entries[i].stat == stat && entries[i].sub == 0)
			value += entries[i].value;
	}
	return IntegerCompare(value, operation, targetED);
}

bool DurabilityCondition::Match(const ItemFacts &facts, const FilterContext &context,
		Condition *arg1, Condition *arg2) const {
	DWORD value = 0;
	const std::vector<StatEntry>& entries = facts.stats->Stats();
	for (unsigned int i = 0; i < entries.size(); i++) {
		if (entries[i].stat == STAT_ENHANCEDMAXDURABILITY && entries[i].sub == 0)
			value += entries[i].value;
	}
	return IntegerCompare(value, operation, targetDurability);
}

bool ChargedCondition::Match(const ItemFacts &facts, const FilterContext &context,
		Condition *arg1, Condition *arg2) const {
	DWORD value = 0;
	const std::vector<StatEntry>& entries = facts.stats->Stats();
	for (unsigned int i = 0; i < entries.size(); i++) {
		// The skill is held above the low six bits, the level it is charged at
		// in them.
		if (entries[i].stat == STAT_CHARGED && (entries[i].sub >> 6) == skill) {
			unsigned int level = entries[i].sub & 0x3F;
			value = (level > value) ? level : value;	// the highest one wins
		}
	}
	return IntegerCompare(value, operation, targetLevel);
}

bool FoolsCondition::Match(const ItemFacts &facts, const FilterContext &context,
		Condition *arg1, Condition *arg2) const {
	// 1 is damage per level, 2 is attack rating per level, 3 is both, which is
	// what a fool's item is. Rules write FOOLS rather than a number, so the
	// comparison is against having both rather than against what was asked for.
	unsigned int value = 0;
	const std::vector<StatEntry>& entries = facts.stats->Stats();
	for (unsigned int i = 0; i < entries.size(); i++) {
		if (entries[i].sub != 0)
			continue;
		if (entries[i].stat == STAT_MAXDAMAGEPERLEVEL)
			value += 1;
		if (entries[i].stat == STAT_ATTACKRATINGPERLEVEL)
			value += 2;
	}
	return IntegerCompare(value, (BYTE)EQUAL, 3);
}

bool TrueCondition::Match(const ItemFacts &facts, const FilterContext &context,
		Condition *arg1, Condition *arg2) const {
	return true;
}

bool FalseCondition::Match(const ItemFacts &facts, const FilterContext &context,
		Condition *arg1, Condition *arg2) const {
	return false;
}

bool ItemCodeCondition::Match(const ItemFacts &facts, const FilterContext &context,
		Condition *arg1, Condition *arg2) const {
	return (targetCode[0] == facts.code[0] && targetCode[1] == facts.code[1] &&
			targetCode[2] == facts.code[2]);
}

bool QualityCondition::Match(const ItemFacts &facts, const FilterContext &context,
		Condition *arg1, Condition *arg2) const {
	return facts.quality == quality;
}

bool NonMagicalCondition::Match(const ItemFacts &facts, const FilterContext &context,
		Condition *arg1, Condition *arg2) const {
	return (facts.quality == ITEM_QUALITY_INFERIOR ||
			facts.quality == ITEM_QUALITY_NORMAL ||
			facts.quality == ITEM_QUALITY_SUPERIOR);
}

bool GemLevelCondition::Match(const ItemFacts &facts, const FilterContext &context,
		Condition *arg1, Condition *arg2) const {
	if (!IsGem(facts.attrs))
		return false;
	return IntegerCompare(GetGemLevel(facts.attrs), operation, gemLevel);
}

bool GemTypeCondition::Match(const ItemFacts &facts, const FilterContext &context,
		Condition *arg1, Condition *arg2) const {
	if (!IsGem(facts.attrs))
		return false;
	return IntegerCompare(GetGemType(facts.attrs), operation, gemType);
}

bool RuneCondition::Match(const ItemFacts &facts, const FilterContext &context,
		Condition *arg1, Condition *arg2) const {
	if (!IsRune(facts.attrs))
		return false;
	return IntegerCompare(RuneNumberFromItemCode(const_cast<char*>(facts.code)),
		operation, runeNumber);
}

bool UsedSocketsCondition::Match(const ItemFacts &facts, const FilterContext &context,
		Condition *arg1, Condition *arg2) const {
	return IntegerCompare(facts.usedSockets, operation, targetUsedSockets);
}

bool ItemLevelCondition::Match(const ItemFacts &facts, const FilterContext &context,
		Condition *arg1, Condition *arg2) const {
	return IntegerCompare(facts.level, operation, itemLevel);
}

bool QualityLevelCondition::Match(const ItemFacts &facts, const FilterContext &context,
		Condition *arg1, Condition *arg2) const {
	return IntegerCompare(facts.attrs->qualityLevel, operation, qualityLevel);
}

bool AffixLevelCondition::Match(const ItemFacts &facts, const FilterContext &context,
		Condition *arg1, Condition *arg2) const {
	BYTE alvl = GetAffixLevel(facts.level, facts.attrs->qualityLevel,
		facts.attrs->magicLevel);
	return IntegerCompare(alvl, operation, affixLevel);
}

bool ItemGroupCondition::Match(const ItemFacts &facts, const FilterContext &context,
		Condition *arg1, Condition *arg2) const {
	return (facts.attrs->flags & itemGroup) > 0;
}

bool FlagsCondition::Match(const ItemFacts &facts, const FilterContext &context,
		Condition *arg1, Condition *arg2) const {
	switch (flag) {
	case ITEM_ETHEREAL:
		return facts.ethereal;
	case ITEM_IDENTIFIED:
		return facts.identified;
	case ITEM_RUNEWORD:
		return facts.runeword;
	}
	// No other flag can be asked for: these three are the only ones a rule's
	// text can name.
	return false;
}

bool QualityIdCondition::Match(const ItemFacts &facts, const FilterContext &context,
		Condition *arg1, Condition *arg2) const {
	switch (quality) {
	case ITEM_QUALITY_UNIQUE:
		return facts.uniqueCode == id;
	case ITEM_QUALITY_SET:
		return facts.setCode == id;
	default:
		return false;
	}
}

bool GoldCondition::Match(const ItemFacts &facts, const FilterContext &context,
		Condition *arg1, Condition *arg2) const {
	if (facts.code[0] != 'g' || facts.code[1] != 'l' || facts.code[2] != 'd')
		return false;
	return IntegerCompare(facts.amount, operation, goldAmount);
}

// The operators, which read the item only through what they are given.
bool NegationOperator::Match(const ItemFacts &facts, const FilterContext &context,
		Condition *arg1, Condition *arg2) const {
	return !arg1->Evaluate(facts, context, arg1, arg2);
}

bool AndOperator::Match(const ItemFacts &facts, const FilterContext &context,
		Condition *arg1, Condition *arg2) const {
	return arg1->Evaluate(facts, context, NULL, NULL) &&
		arg2->Evaluate(facts, context, NULL, NULL);
}

bool OrOperator::Match(const ItemFacts &facts, const FilterContext &context,
		Condition *arg1, Condition *arg2) const {
	return arg1->Evaluate(facts, context, NULL, NULL) ||
		arg2->Evaluate(facts, context, NULL, NULL);
}

// The parentheses never answer anything. They exist to carry their own kind
// through the shunting yard and are gone before a rule is ever judged.
bool LeftParen::Match(const ItemFacts &facts, const FilterContext &context,
		Condition *arg1, Condition *arg2) const {
	return false;
}

bool RightParen::Match(const ItemFacts &facts, const FilterContext &context,
		Condition *arg1, Condition *arg2) const {
	return false;
}

/*
 * The conditions that ask about the character or the world rather than the
 * item. Each one read the game itself, at the moment it was asked, which is
 * why they had the same body twice: the two halves were never different.
 */
bool PlayerTypeCondition::Match(const ItemFacts &facts, const FilterContext &context,
		Condition *arg1, Condition *arg2) const {
	return ((context.charFlags >> 5) & 0x1) == mode;
}

bool CharClassCondition::Match(const ItemFacts &facts, const FilterContext &context,
		Condition *arg1, Condition *arg2) const {
	return context.charClass == charClass;
}

bool CharStatCondition::Match(const ItemFacts &facts, const FilterContext &context,
		Condition *arg1, Condition *arg2) const {
	return IntegerCompare(context.charStats->Stat(stat1, stat2), operation, targetStat);
}

bool DifficultyCondition::Match(const ItemFacts &facts, const FilterContext &context,
		Condition *arg1, Condition *arg2) const {
	return IntegerCompare(context.difficulty, operation, targetDiff);
}

bool FilterLevelCondition::Match(const ItemFacts &facts, const FilterContext &context,
		Condition *arg1, Condition *arg2) const {
	return IntegerCompare(context.filterLevel, operation, filterLevel);
}











void AddCondition::Init() {
	codes.clear();
	codes = split(key, '+');
	for (auto code : codes) {
		for (int j = 0; j < sizeof(skills) / sizeof(skills[0]); j++) {
			if (code == skills[j].key)
				stats.push_back(skills[j].value);
		}
	}
}

// Only an item lying in the world has an area to speak of, and for those the
// character's area is the area the item is in.
bool AreaLevelCondition::Match(const ItemFacts &facts, const FilterContext &context,
		Condition *arg1, Condition *arg2) const {
	return facts.ground && IntegerCompare(context.areaLevel, operation, areaLevel);
}

bool AreaIdCondition::Match(const ItemFacts &facts, const FilterContext &context,
		Condition *arg1, Condition *arg2) const {
	return facts.ground && IntegerCompare(context.areaId, operation, areaId);
}

// The affix level a craft rolled at, worked out from the item's level and the
// character's together.
bool CraftAffixLevelCondition::Match(const ItemFacts &facts, const FilterContext &context,
		Condition *arg1, Condition *arg2) const {
	unsigned int craftLevel = facts.level / 2 + context.charLevel / 2;
	BYTE alvl = GetAffixLevel((BYTE)craftLevel, facts.attrs->qualityLevel,
		facts.attrs->magicLevel);
	return IntegerCompare(alvl, operation, affixLevel);
}

// A rule of rules: how many of them the item satisfies.
bool PartialCondition::Match(const ItemFacts &facts, const FilterContext &context,
		Condition *arg1, Condition *arg2) const {
	int matched = 0;
	for (unsigned int i = 0; i < rules.size(); i++) {
		if (const_cast<Rule&>(rules[i]).Evaluate(facts, context))
			matched++;
	}
	return IntegerCompare(matched, operation, target_count);
}

/*
 * The two an item has to exist to be asked.
 *
 * Neither checks whether it can be. A rule holding either is abandoned before
 * it is judged when the item cannot answer, which is ADR 0002, and is why these
 * read as though the answer is always there.
 */
bool ItemPriceCondition::Match(const ItemFacts &facts, const FilterContext &context,
		Condition *arg1, Condition *arg2) const {
	return IntegerCompare(facts.liveOnly->Price(context.difficulty),
		operation, targetStat);
}

bool RequiredLevelCondition::Match(const ItemFacts &facts, const FilterContext &context,
		Condition *arg1, Condition *arg2) const {
	return IntegerCompare(facts.liveOnly->RequiredLevel(), operation, requiredLevel);
}

bool Condition::Evaluate(const ItemFacts &facts, const FilterContext &context,
		Condition *arg1, Condition *arg2) {
	return Match(facts, context, arg1, arg2);
}

bool IsItemBlocked(unsigned int ignoreIndex, unsigned int keepIndex,
		bool orderedFiltering) {
	if (ignoreIndex == NO_RULE_MATCH)
		return false;
	if (orderedFiltering)
		return ignoreIndex < keepIndex;
	return keepIndex == NO_RULE_MATCH;
}

std::vector<const Action*> MatchingActions(const std::vector<Rule*> &rules,
		const ItemFacts &facts, const FilterContext &context,
		unsigned int pingLevel) {
	std::vector<const Action*> actions;
	for (unsigned int i = 0; i < rules.size(); i++) {
		Rule *rule = rules[i];
		if (!rule->Evaluate(facts, context))
			continue;
		actions.push_back(&rule->action);
		if (rule->action.pingLevel > pingLevel)
			continue;
		if (rule->action.stopProcessing)
			break;	// unless the rule said to carry on
	}
	return actions;
}

// The earliest rule in a list an item matches, for the two lists where nothing
// past the first one has a say.
static const Rule *FirstMatchingRule(const std::vector<Rule*> &rules,
		const ItemFacts &facts, const FilterContext &context) {
	for (unsigned int i = 0; i < rules.size(); i++) {
		Rule *rule = rules[i];
		if (rule->Evaluate(facts, context))
			return rule;
	}
	return NULL;
}

RuleMatch MatchRules(const RuleLists &lists, const ItemFacts &facts,
		const FilterContext &context, unsigned int pingLevel,
		bool orderedFiltering) {
	RuleMatch match;

	std::vector<const Action*> mapActions =
		MatchingActions(*lists.map, facts, context, pingLevel);
	for (unsigned int i = 0; i < mapActions.size(); i++) {
		const Action *action = mapActions[i];
		if (action->index < match.keepIndex)
			match.keepIndex = action->index;
		// A rule kept the item even if its tier is above what is being pinged;
		// it just has nothing to say about the map.
		if (action->pingLevel > pingLevel)
			continue;
		int color = action->notifyColor;
		// Never overwrite a colour with no colour, nor a real one with the
		// colour that means do not say anything.
		if (color != UNDEFINED_COLOR &&
				(color != DEAD_COLOR || match.color == UNDEFINED_COLOR))
			match.color = color;
		match.showOnMap = true;
		match.noTracking = action->noTracking;
		match.pingLevel = action->pingLevel;
	}

	// An item whose name a rule gave it is not hidden by a later one.
	const Rule *keeper = FirstMatchingRule(*lists.doNotBlock, facts, context);
	if (keeper && keeper->action.index < match.keepIndex)
		match.keepIndex = keeper->action.index;

	// With ordered filtering off this list only matters when nothing kept the
	// item, so the scan is skipped entirely in that case.
	if (orderedFiltering || match.keepIndex == NO_RULE_MATCH) {
		const Rule *hider = FirstMatchingRule(*lists.ignore, facts, context);
		if (hider)
			match.ignoreIndex = hider->action.index;
	}

	match.blocked = IsItemBlocked(match.ignoreIndex, match.keepIndex,
		orderedFiltering);
	return match;
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

RulePlacement PlaceRule(const Rule &rule) {
	RulePlacement placement = {};
	placement.description = without_invis_chars(rule.action.description).length() > 0;
	placement.map = rule.action.colorOnMap != UNDEFINED_COLOR ||
		rule.action.borderColor != UNDEFINED_COLOR ||
		rule.action.dotColor != UNDEFINED_COLOR ||
		rule.action.pxColor != UNDEFINED_COLOR ||
		rule.action.lineColor != UNDEFINED_COLOR;
	placement.name = without_invis_chars(rule.action.name).length() > 0;

	/*
	 * An item a rule gave a name to is not one to hide, and an item shown on
	 * the map is already not hidden, so only a name without a map action needs
	 * saying. A rule that carries on is not counted: something later may still
	 * hide the item.
	 */
	placement.doNotBlock = placement.name && rule.action.stopProcessing &&
		!placement.map;

	// A rule that says nothing at all about an item is a rule to hide it.
	placement.ignore = !placement.map && !placement.name &&
		!placement.description && rule.action.stopProcessing;
	return placement;
}
