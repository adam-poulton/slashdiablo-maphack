/*
 * The item filter: the conditions a rule is written out of, the rule itself,
 * and what a rule does to an item that matches it.
 *
 * A condition still answers in two ways, one for an item already in the world
 * and one for an item a packet has just described, which is why almost every
 * one of them carries a pair of near identical bodies. Collapsing that pair is
 * what ItemFacts exists for, and is why this is a file of its own rather than
 * part of the item display it was cut out of.
 */

#pragma once
#include <map>
#include <string>
#include <vector>
#include <windows.h>
#include "../../Constants.h"
#include "FilterContext.h"
#include "ItemFacts.h"

// A rule is written below the conditions it is built from, and one condition
// holds rules of its own.
struct Rule;

// All the types able to be combined with the + operator
#define COMBO_STATS					\
	{"LIFE", STAT_MAXHP},			\
	{"MANA", STAT_MAXMANA},			\
	{"STR", STAT_STRENGTH},			\
	{"DEX", STAT_DEXTERITY},		\
	{"CRES", STAT_COLDRESIST},		\
	{"FRES", STAT_FIRERESIST},		\
	{"LRES", STAT_LIGHTNINGRESIST},	\
	{"PRES", STAT_POISONRESIST},	\
	{"MINDMG", STAT_MINIMUMDAMAGE},	\
	{"MAXDMG", STAT_MAXIMUMDAMAGE},	\

// All colors here must also be defined in MAP_COLOR_REPLACEMENTS
#define COLOR_REPLACEMENTS	\
	{"WHITE", "\377c0"},		\
	{"RED", "\377c1"},			\
	{"GREEN", "\377c2"},		\
	{"BLUE", "\377c3"},		\
	{"GOLD", "\377c4"},		\
	{"GRAY", "\377c5"},		\
	{"BLACK", "\377c6"},		\
	{"TAN", "\377c7"},			\
	{"ORANGE", "\377c8"},		\
	{"YELLOW", "\377c9"},		\
	{"PURPLE", "\377c;"},		\
	{"DARK_GREEN", "\377c:"},	\
	{"CORAL", "\377c\x06"},		\
	{"SAGE", "\377c\x07"},		\
	{"TEAL", "\377c\x09"},		\
	{"LIGHT_GRAY", "\xFF" "c\x0C"}

#define MAP_COLOR_WHITE     0x20

#define MAP_COLOR_REPLACEMENTS	\
	{"WHITE", 0x20},		\
	{"RED", 0x0A},			\
	{"GREEN", 0x84},		\
	{"BLUE", 0x97},			\
	{"GOLD", 0x0D},			\
	{"GRAY", 0xD0},			\
	{"BLACK", 0x00},		\
	{"TAN", 0x5A},			\
	{"ORANGE", 0x60},		\
	{"YELLOW", 0x0C},		\
	{"PURPLE", 0x9B},		\
	{"DARK_GREEN", 0x76},	\
	{"CORAL", 0x66},		\
	{"SAGE", 0x82},			\
	{"TEAL", 0xCB},			\
	{"LIGHT_GRAY", 0xD6}

// How a condition compares what it read against what it was written with.
enum Operation {
	EQUAL,
	GREATER_THAN,
	LESS_THAN,
	NONE
};

/*
 * Where the parser is up to.
 *
 * A rule's text says nothing about the AND between two conditions written side
 * by side, so the parser puts one in whenever an operand follows an operand.
 * Knowing whether the last thing seen was an operand is the whole of the state
 * it keeps, and it is a global because building a rule is not reentrant: the
 * one condition that parses rules of its own saves and restores this by hand.
 */
extern BYTE LastConditionType;

#define EXCEPTION_INVALID_STAT			1
#define EXCEPTION_INVALID_OPERATION		2
#define EXCEPTION_INVALID_OPERATOR		3
#define EXCEPTION_INVALID_FLAG			4
#define EXCEPTION_INVALID_ITEM_TYPE		5
#define EXCEPTION_INVALID_GOLD_TYPE		6

#define PLAYER_CLASSIC 0
#define PLAYER_XP 1

#define DEAD_COLOR        0xdead
#define UNDEFINED_COLOR   0xbeef

// Sentinel returned by the rule lookups below when no rule in a list matched.
// Ordered filtering compares matched rule indices, so "no match" must sort last.
#define NO_RULE_MATCH     0xffffffff

// Where reading a rule reports what it could not make sense of.
struct ItemFilterDiagnostics {
	virtual ~ItemFilterDiagnostics() {}

	// A token that means nothing here.
	virtual void IgnoredToken(const std::string &token) {}

	// A token whose comparison could not be read as a number.
	virtual void UnreadableValue(const std::string &token) {}
};

/*
 * The parts of the configuration a rule is built from that are not in the
 * rule's own text.
 *
 * Two conditions count skills a player has said are worth counting, which is a
 * setting rather than something the rule says. Passing it in is what keeps
 * building a rule from reading settings itself, and so from needing them to
 * exist at all.
 */
struct ItemFilterSettings {
	// Skill ids GOODSK counts, and class and tab pairs GOODTBSK counts.
	std::vector<unsigned int> goodClassSkills;
	std::vector<unsigned int> goodTabSkills;

	// The highest stat and skill the game's tables describe, which is what
	// bounds the numbers a rule is allowed to name.
	unsigned int statMax;
	unsigned int skillMax;

	// Where a token that means nothing is reported, or null to say nothing.
	ItemFilterDiagnostics *diagnostics;

	ItemFilterSettings() : statMax(0), skillMax(0), diagnostics(NULL) {}
};

enum ConditionType {
	CT_None,
	CT_LeftParen,
	CT_RightParen,
	CT_NegationOperator,
	CT_BinaryOperator,
	CT_Operand
};

class Condition
{
public:
	Condition() {}
	virtual ~Condition() {}

	static const std::string tokenDelims;
	static void BuildConditions(std::vector<Condition*> &conditions, std::string token,
		const ItemFilterSettings &settings);
	static void ProcessConditions(std::vector<Condition*> &rawConditions, std::vector<Condition*> &processedConditions);
	static void AddOperand(std::vector<Condition*> &conditions, Condition *cond);
	static void AddNonOperand(std::vector<Condition*> &conditions, Condition *cond);

	bool Evaluate(const ItemFacts &facts, const FilterContext &context,
			Condition *arg1, Condition *arg2);

	BYTE conditionType{};

	/*
	 * Whether this can only be answered about an item that already exists.
	 *
	 * Asked once, when a rule is built, rather than each time one is judged.
	 * A rule holding any such condition is not judged at all against an item
	 * that cannot answer it, which is what ADR 0002 settles: an unknown fact
	 * stops the whole rule rather than its own condition, so that negating it
	 * does not turn not knowing into a match.
	 */
	virtual bool NeedsLiveItem() const { return false; }

private:
	/*
	 * Whether an item satisfies this condition.
	 *
	 * One body, whichever way the item arrived. Every condition used to carry
	 * two, one reading a game unit and one reading a packet, and no two of them
	 * were kept in step by anything but attention.
	 */
	virtual bool Match(const ItemFacts &facts, const FilterContext &context,
			Condition *arg1, Condition *arg2) const {
		return false;
	}
};

class TrueCondition : public Condition
{
public:
	TrueCondition() { conditionType = CT_Operand; };
private:
	bool Match(const ItemFacts &facts, const FilterContext &context,
			Condition *arg1, Condition *arg2) const override;
};

class FalseCondition : public Condition
{
public:
	FalseCondition() { conditionType = CT_Operand; };
private:
	bool Match(const ItemFacts &facts, const FilterContext &context,
			Condition *arg1, Condition *arg2) const override;
};

class NegationOperator : public Condition
{
public:
	NegationOperator() { conditionType = CT_NegationOperator; };
private:
	bool Match(const ItemFacts &facts, const FilterContext &context,
			Condition *arg1, Condition *arg2) const override;
};

class LeftParen : public Condition
{
public:
	LeftParen() { conditionType = CT_LeftParen; };
private:
	bool Match(const ItemFacts &facts, const FilterContext &context,
			Condition *arg1, Condition *arg2) const override;
};

class RightParen : public Condition
{
public:
	RightParen() { conditionType = CT_RightParen; };
private:
	bool Match(const ItemFacts &facts, const FilterContext &context,
			Condition *arg1, Condition *arg2) const override;
};

class AndOperator : public Condition
{
public:
	AndOperator() { conditionType = CT_BinaryOperator; };
private:
	bool Match(const ItemFacts &facts, const FilterContext &context,
			Condition *arg1, Condition *arg2) const override;
};

class OrOperator : public Condition
{
public:
	OrOperator() { conditionType = CT_BinaryOperator; };
private:
	bool Match(const ItemFacts &facts, const FilterContext &context,
			Condition *arg1, Condition *arg2) const override;
};

class ItemCodeCondition : public Condition
{
public:
	ItemCodeCondition(const char *code) {
		targetCode[0] = code[0];
		targetCode[1] = code[1];
		targetCode[2] = code[2];
		targetCode[3] = 0;
		conditionType = CT_Operand;
	};
private:
	char targetCode[4];
	bool Match(const ItemFacts &facts, const FilterContext &context,
			Condition *arg1, Condition *arg2) const override;
};

class FlagsCondition : public Condition
{
public:
	FlagsCondition(unsigned int flg) : flag(flg) { conditionType = CT_Operand; };
private:
	unsigned int flag;
	bool Match(const ItemFacts &facts, const FilterContext &context,
			Condition *arg1, Condition *arg2) const override;
};

class PlayerTypeCondition : public Condition
{
public:
	PlayerTypeCondition(unsigned int m) : mode(m) { conditionType = CT_Operand; };
private:
	unsigned int mode;
	bool Match(const ItemFacts &facts, const FilterContext &context,
			Condition *arg1, Condition *arg2) const override;
};

class CharClassCondition : public Condition
{
public:
	CharClassCondition(unsigned int c) : charClass(c) { conditionType = CT_Operand; };
private:
	unsigned int charClass;
	bool Match(const ItemFacts &facts, const FilterContext &context,
			Condition *arg1, Condition *arg2) const override;
};

class QualityCondition : public Condition
{
public:
	QualityCondition(unsigned int qual) : quality(qual) { conditionType = CT_Operand; };
private:
	unsigned int quality;
	bool Match(const ItemFacts &facts, const FilterContext &context,
			Condition *arg1, Condition *arg2) const override;
};

class NonMagicalCondition : public Condition
{
public:
	NonMagicalCondition() { conditionType = CT_Operand; };
private:
	bool Match(const ItemFacts &facts, const FilterContext &context,
			Condition *arg1, Condition *arg2) const override;
};

class QualityIdCondition : public Condition
{
public:
	QualityIdCondition(unsigned int quality, unsigned int id) : quality(quality), id(id){ conditionType = CT_Operand; };
private:
	unsigned int quality;
	unsigned int id;
	bool Match(const ItemFacts &facts, const FilterContext &context,
			Condition *arg1, Condition *arg2) const override;
};

class GemLevelCondition : public Condition
{
public:
	GemLevelCondition(BYTE op, BYTE gem) : gemLevel(gem), operation(op) { conditionType = CT_Operand; };
private:
	BYTE operation;
	BYTE gemLevel;
	bool Match(const ItemFacts &facts, const FilterContext &context,
			Condition *arg1, Condition *arg2) const override;
};

class GemTypeCondition : public Condition
{
public:
	GemTypeCondition(BYTE op, BYTE gType) : gemType(gType), operation(op) { conditionType = CT_Operand; };
private:
	BYTE operation;
	BYTE gemType;
	bool Match(const ItemFacts &facts, const FilterContext &context,
			Condition *arg1, Condition *arg2) const override;
};

class RuneCondition : public Condition
{
public:
	RuneCondition(BYTE op, BYTE rune) : runeNumber(rune), operation(op) { conditionType = CT_Operand; };
private:
	BYTE operation;
	BYTE runeNumber;
	bool Match(const ItemFacts &facts, const FilterContext &context,
			Condition *arg1, Condition *arg2) const override;
};

class GoldCondition : public Condition
{
public:
	GoldCondition(BYTE op, unsigned int amt) : goldAmount(amt), operation(op) { conditionType = CT_Operand; };
private:
	BYTE operation;
	unsigned int goldAmount;
	bool Match(const ItemFacts &facts, const FilterContext &context,
			Condition *arg1, Condition *arg2) const override;
};

class ItemLevelCondition : public Condition
{
public:
	ItemLevelCondition(BYTE op, BYTE ilvl) : itemLevel(ilvl), operation(op) { conditionType = CT_Operand; };
private:
	BYTE operation;
	BYTE itemLevel;
	bool Match(const ItemFacts &facts, const FilterContext &context,
			Condition *arg1, Condition *arg2) const override;
};

class QualityLevelCondition : public Condition
{
public:
	QualityLevelCondition(BYTE op, BYTE qlvl) : qualityLevel(qlvl), operation(op) { conditionType = CT_Operand; };
private:
	BYTE operation;
	BYTE qualityLevel;
	bool Match(const ItemFacts &facts, const FilterContext &context,
			Condition *arg1, Condition *arg2) const override;
};

class AffixLevelCondition : public Condition
{
public:
	AffixLevelCondition(BYTE op, BYTE alvl) : affixLevel(alvl), operation(op) { conditionType = CT_Operand; };
private:
	BYTE operation;
	BYTE affixLevel;
	bool Match(const ItemFacts &facts, const FilterContext &context,
			Condition *arg1, Condition *arg2) const override;
};

class CraftAffixLevelCondition : public Condition
{
public:
	CraftAffixLevelCondition(BYTE op, BYTE alvl) : affixLevel(alvl), operation(op) { conditionType = CT_Operand; };
private:
	BYTE operation;
	BYTE affixLevel;
	bool Match(const ItemFacts &facts, const FilterContext &context,
			Condition *arg1, Condition *arg2) const override;
};

class RequiredLevelCondition : public Condition
{
public:
	RequiredLevelCondition(BYTE op, BYTE rlvl) : requiredLevel(rlvl), operation(op) { conditionType = CT_Operand; };
private:
	bool NeedsLiveItem() const override { return true; }

	BYTE operation;
	BYTE requiredLevel;
	bool Match(const ItemFacts &facts, const FilterContext &context,
			Condition *arg1, Condition *arg2) const override;
};

class UsedSocketsCondition : public Condition
{
public:
	UsedSocketsCondition(BYTE op, unsigned int target) : operation(op), targetUsedSockets(target) { conditionType = CT_Operand; };
private:
	BYTE operation;
	unsigned int targetUsedSockets;
	bool Match(const ItemFacts &facts, const FilterContext &context,
			Condition *arg1, Condition *arg2) const override;
};

class ItemGroupCondition : public Condition
{
public:
	ItemGroupCondition(unsigned int group) : itemGroup(group) { conditionType = CT_Operand; };
private:
	unsigned int itemGroup;
	bool Match(const ItemFacts &facts, const FilterContext &context,
			Condition *arg1, Condition *arg2) const override;
};

class EDCondition : public Condition
{
public:
	EDCondition(BYTE op, unsigned int target) : operation(op), targetED(target) { conditionType = CT_Operand; };
private:
	BYTE operation;
	unsigned int targetED;
	bool Match(const ItemFacts &facts, const FilterContext &context,
			Condition *arg1, Condition *arg2) const override;
	bool EvaluateED(unsigned int flags);
};

class DurabilityCondition : public Condition
{
public:
	DurabilityCondition(BYTE op, unsigned int target) : operation(op), targetDurability(target) { conditionType = CT_Operand; };
private:
	BYTE operation;
	unsigned int targetDurability;
	bool Match(const ItemFacts &facts, const FilterContext &context,
			Condition *arg1, Condition *arg2) const override;
};

class ChargedCondition : public Condition
{
public:
	ChargedCondition(BYTE op, unsigned int sk, unsigned int target) : operation(op), skill(sk), targetLevel(target) { conditionType = CT_Operand; };
private:
	BYTE operation;
	unsigned int skill;
	unsigned int targetLevel;
	bool Match(const ItemFacts &facts, const FilterContext &context,
			Condition *arg1, Condition *arg2) const override;
};

class FoolsCondition : public Condition
{
public:
	FoolsCondition() { conditionType = CT_Operand; };
private:
	bool Match(const ItemFacts &facts, const FilterContext &context,
			Condition *arg1, Condition *arg2) const override;
};

class SkillListCondition : public Condition
{
public:
	SkillListCondition(BYTE op, unsigned int t, unsigned int target,
			const ItemFilterSettings &settings)
		: operation(op), type(t), targetStat(target),
		  goodClassSkills(settings.goodClassSkills),
		  goodTabSkills(settings.goodTabSkills) {
		conditionType = CT_Operand;
	};
private:
	BYTE operation;
	unsigned int type;
	unsigned int targetStat;
	std::vector<unsigned int> goodClassSkills;
	std::vector<unsigned int> goodTabSkills;
	bool Match(const ItemFacts &facts, const FilterContext &context,
			Condition *arg1, Condition *arg2) const override;
};

class CharStatCondition : public Condition
{
public:
	CharStatCondition(unsigned int stat, unsigned int stat2, BYTE op, unsigned int target)
		: stat1(stat), stat2(stat2), operation(op), targetStat(target) { conditionType = CT_Operand; };
private:
	unsigned int stat1;
	unsigned int stat2;
	BYTE operation;
	unsigned int targetStat;
	bool Match(const ItemFacts &facts, const FilterContext &context,
			Condition *arg1, Condition *arg2) const override;
};

class DifficultyCondition : public Condition
{
public:
	DifficultyCondition(BYTE op, unsigned int target)
		: operation(op), targetDiff(target) { conditionType = CT_Operand; };
private:
	BYTE operation;
	unsigned int targetDiff;
	bool Match(const ItemFacts &facts, const FilterContext &context,
			Condition *arg1, Condition *arg2) const override;
};

class FilterLevelCondition : public Condition
{
public:
	FilterLevelCondition(BYTE op, unsigned int target)
		: operation(op), filterLevel(target) { conditionType = CT_Operand; };
private:
	BYTE operation;
	unsigned int filterLevel;
	bool Match(const ItemFacts &facts, const FilterContext &context,
			Condition *arg1, Condition *arg2) const override;
};

class AreaLevelCondition : public Condition
{
public:
	AreaLevelCondition(BYTE op, unsigned int target)
		: operation(op), areaLevel(target) { conditionType = CT_Operand; };
private:
	BYTE operation;
	unsigned int areaLevel;
	bool Match(const ItemFacts &facts, const FilterContext &context,
			Condition *arg1, Condition *arg2) const override;
};

class AreaIdCondition : public Condition
{
public:
	AreaIdCondition(BYTE op, unsigned int target)
		: operation(op), areaId(target) { conditionType = CT_Operand; };
private:
	BYTE operation;
	unsigned int areaId;
	bool Match(const ItemFacts &facts, const FilterContext &context,
			Condition *arg1, Condition *arg2) const override;
};

class ItemStatCondition : public Condition
{
public:
	ItemStatCondition(unsigned int stat, unsigned int stat2, BYTE op, unsigned int target)
		: itemStat(stat), itemStat2(stat2), operation(op), targetStat(target) { conditionType = CT_Operand; };
private:
	unsigned int itemStat;
	unsigned int itemStat2;
	BYTE operation;
	unsigned int targetStat;
	bool Match(const ItemFacts &facts, const FilterContext &context,
			Condition *arg1, Condition *arg2) const override;
};

class PartialCondition : public Condition
{
public:
	PartialCondition(BYTE op, int target_count, std::vector<std::string> tokens,
			const ItemFilterSettings &settings)
		: operation(op), target_count(target_count) {
		for (auto token : tokens) {
			make_count_subrule(token, settings);
		}
		conditionType = CT_Operand;
	};
	
	// make_count_subrule calls BuildConditon, which creates new Conditions. We free these here.
	~PartialCondition();
private:
	BYTE operation;
	const int target_count;
	std::vector<Rule> rules; // TODO: should be const, but Rule::Evalate needs to be modified
	void make_count_subrule(std::string token, const ItemFilterSettings &settings);
	bool Match(const ItemFacts &facts, const FilterContext &context,
			Condition *arg1, Condition *arg2) const override;
};

class ItemPriceCondition : public Condition
{
public:
	ItemPriceCondition(BYTE op, unsigned int target)
		: operation(op), targetStat(target) {
		conditionType = CT_Operand;
	};
private:
	bool NeedsLiveItem() const override { return true; }

	BYTE operation;
	unsigned int targetStat;
	bool Match(const ItemFacts &facts, const FilterContext &context,
			Condition *arg1, Condition *arg2) const override;
};

class ResistAllCondition : public Condition
{
public:
	ResistAllCondition(BYTE op, unsigned int target) : operation(op), targetStat(target) { conditionType = CT_Operand; };
private:
	BYTE operation;
	unsigned int targetStat;
	bool Match(const ItemFacts &facts, const FilterContext &context,
			Condition *arg1, Condition *arg2) const override;
};

class AddCondition : public Condition
{
public:
	AddCondition(std::string& k, BYTE op, unsigned int target) : key(k), operation(op), targetStat(target) { 
		conditionType = CT_Operand;
		Init();
	};
private:
	BYTE operation;
	std::vector<std::string> codes;
	std::vector<DWORD> stats;
	unsigned int targetStat;
	std::string key;
	void Init();
	bool Match(const ItemFacts &facts, const FilterContext &context,
			Condition *arg1, Condition *arg2) const override;
};

extern TrueCondition *trueCondition;
extern FalseCondition *falseCondition;

struct ActionReplace {
	std::string key;
	std::string value;
};

struct ColorReplace {
	std::string key;
	int value;
};

struct SkillReplace {
	std::string key;
	int value;
};

// The stats that may be added together with the + operator in a rule.
extern SkillReplace skills[];

struct Action {
	bool stopProcessing;
	std::string name;
	std::string description;
	int colorOnMap;
	int borderColor;
	int dotColor;
	int pxColor;
	int lineColor;
	int notifyColor;
	bool noTracking;
	unsigned int pingLevel;
	// Position of the owning rule in the config file. Rules are split across
	// several lists, so this is how their relative order is recovered later.
	unsigned int index;
	Action() :
		colorOnMap(UNDEFINED_COLOR),
		borderColor(UNDEFINED_COLOR),
		dotColor(UNDEFINED_COLOR),
		pxColor(UNDEFINED_COLOR),
		lineColor(UNDEFINED_COLOR),
		notifyColor(UNDEFINED_COLOR),
		pingLevel(0),
		stopProcessing(true),
		noTracking(false),
		index(NO_RULE_MATCH),
		name(""),
		description("") {}
};

struct Rule {
	std::vector<Condition*> conditions;
	Action action;
	std::vector<Condition*> conditionStack;

	// Whether any condition here can only be answered about an item that
	// exists. Worked out once, when the rule is built.
	bool needsLiveItem;

	Rule(std::vector<Condition*> &inputConditions, std::string *str);

	// TODO: Should this really be defined in the header? This will force it to be inlined AFAIK. -ybd
	// Evaluate conditions which are in Reverse Polish Notation
	bool Evaluate(const ItemFacts &facts, const FilterContext &context) {
		// An item that does not exist yet cannot answer what this rule asks,
		// and a rule that cannot be judged does not match.
		if (needsLiveItem && !facts.liveOnly) {
			return false;
		}

		if (conditions.size() == 0) {
			return true;  // a rule with no conditions always matches
		}

		bool retval;
		try {
			// conditionStack was previously declared on the stack within this function. This caused
			// excessive allocaiton calls and was limiting the speed of the item display (causing
			// frame rate drops with complex item displays while ALT was held down). Moving this vector
			// to the object level, preallocating size, and using the resize(0) method to clear avoids
			// excessive reallocation.
			conditionStack.resize(0); 
			for (unsigned int i = 0; i < conditions.size(); i++) {
				Condition *input = conditions[i];
				if (input->conditionType == CT_Operand) {
					conditionStack.push_back(input);
				} else if (input->conditionType == CT_BinaryOperator || input->conditionType == CT_NegationOperator) {
					Condition *arg1 = NULL, *arg2 = NULL;
					if (conditionStack.size() < 1) {
						return false;
					}
					arg1 = conditionStack.back();
					conditionStack.pop_back();
					if (input->conditionType == CT_BinaryOperator) {
						if (conditionStack.size() < 1) {
							return false;
						}
						arg2 = conditionStack.back();
						conditionStack.pop_back();
					}
					if (input->Evaluate(facts, context, arg1, arg2)) {
						conditionStack.push_back(trueCondition);
					} else {
						conditionStack.push_back(falseCondition);
					}
				}
			}
			if (conditionStack.size() == 1) {
				retval = conditionStack[0]->Evaluate(facts, context, NULL, NULL);
			} else {
				retval = false;
			}
		} catch (...) {
			retval = false;
		}
		return retval;
	}
};

// Building an action out of the text that follows a rule.
void BuildAction(std::string *str, Action *act);
std::string ParseDescription(Action *act);
int ParsePingLevel(Action *act, const std::string& reg_string);
int ParseMapColor(Action *act, const std::string& reg_string);

// Comparisons a condition is written with.
BYTE GetOperation(std::string *op);
bool IntegerCompare(unsigned int Lvalue, BYTE operation, unsigned int Rvalue);

// Item facts several conditions ask for.
BYTE GetAffixLevel(BYTE ilvl, BYTE qlvl, BYTE mlvl);
bool IsGem(ItemAttributes *attrs);
BYTE GetGemLevel(ItemAttributes *attrs);
char *GetGemLevelString(BYTE level);
BYTE GetGemType(ItemAttributes *attrs);
char *GetGemTypeString(BYTE type);
bool IsRune(ItemAttributes *attrs);
BYTE RuneNumberFromItemCode(char *code);

/*
 * What the rules decided about one item.
 *
 * keepIndex and ignoreIndex are where in the configuration the earliest rule
 * that wanted the item kept, and the earliest that wanted it hidden, were
 * written. Ordered filtering compares the two; otherwise anything kept wins.
 */
struct RuleMatch {
	unsigned int keepIndex;
	unsigned int ignoreIndex;
	bool blocked;
	bool showOnMap;
	bool noTracking;
	int color;
	int pingLevel;

	RuleMatch() : keepIndex(NO_RULE_MATCH), ignoreIndex(NO_RULE_MATCH),
		blocked(false), showOnMap(false), noTracking(false),
		color(UNDEFINED_COLOR), pingLevel(-1) {}
};

// The three lists a rule is sorted into when it is read, in the order they are
// walked.
struct RuleLists {
	const std::vector<Rule*> *map;
	const std::vector<Rule*> *doNotBlock;
	const std::vector<Rule*> *ignore;
};

// Whether an item is hidden, given where the earliest rule wanting it kept and
// the earliest wanting it hidden were written.
bool IsItemBlocked(unsigned int ignoreIndex, unsigned int keepIndex,
		bool orderedFiltering);

// A ping level no rule can exceed, for the lists where the setting has no say.
// A rule's tier governs the map box and the notification; the name and the
// description a rule gives an item are not tiered.
#define PING_LEVEL_ALL    0xffffffff

/*
 * The actions of the rules in one list that an item matches.
 *
 * Every walk of a rule list stops the same way and few of them agreed about it,
 * so the stopping is here and nothing else is. The actions come back in the
 * order they were written, and what to do with each one is the caller's: the
 * automap draws them, a name folds them together, the map walk reads a colour
 * off them.
 *
 * Actions a rule asked to be shown above the ping level in force are still
 * returned. They are what stops a rule that is too high a tier to be drawn from
 * also hiding the item: whoever counts a rule as keeping an item counts those,
 * and whoever shows one does not. Leaving them out here would make a tier
 * setting hide items, which is what it is documented never to do.
 *
 * The walk ends after the first action that both sits at or below the ping
 * level and asks to stop. A rule that asks to stop from above the ping level
 * does not stop anything, since nothing it says is being listened to.
 */
std::vector<const Action*> MatchingActions(const std::vector<Rule*> &rules,
		const ItemFacts &facts, const FilterContext &context,
		unsigned int pingLevel);

/*
 * What the rules make of one item.
 *
 * The walk itself: which rules match, which of them has a say about the map,
 * and whether what is left hides the item. Written once here because it is what
 * the filter does, and because a recorded decision can only be checked against
 * it if it is the same walk that made the recording.
 */
RuleMatch MatchRules(const RuleLists &lists, const ItemFacts &facts,
		const FilterContext &context, unsigned int pingLevel,
		bool orderedFiltering);

// A name or description that is only colour codes and spaces says nothing, so
// what is left once those are taken out is what decides whether a rule has one.
void removeSubstrs(std::string& s, const std::string& p);
std::string without_invis_chars(const std::string &name);

// Which lists a rule belongs to, which follows entirely from what its action
// does. Answered in one place so that reading a rule set and judging against
// one cannot disagree about it.
struct RulePlacement {
	bool name;
	bool description;
	bool map;
	bool doNotBlock;
	bool ignore;
};

RulePlacement PlaceRule(const Rule &rule);
