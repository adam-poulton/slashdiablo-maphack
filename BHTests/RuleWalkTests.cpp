#include "doctest.h"
#include <string>
#include <vector>
#include "FilterContext.h"
#include "ItemFacts.h"
#include "ItemFilter.h"

/*
 * Walking one list of rules.
 *
 * What is checked here is only where the walk stops and what it hands back,
 * since that is all the walk decides. Which rules match is the conditions'
 * business and is checked against recorded drops elsewhere; the rules below
 * either match everything or nothing, so that the stopping is the only thing
 * left to be wrong.
 */

namespace {

// A list of rules that reads them the way the game does and owns them.
class RuleList {
public:
	RuleList() {
		settings.statMax = 512;
		settings.skillMax = 512;
	}

	~RuleList() {
		for (unsigned int i = 0; i < rules.size(); i++) {
			for (unsigned int c = 0; c < rules[i]->conditions.size(); c++)
				delete rules[i]->conditions[c];
			delete rules[i];
		}
	}

	// condition is a single token or empty for a rule that matches everything.
	void Add(const std::string &condition, const std::string &action) {
		// Reading a rule is not reentrant: the parser tracks whether the last
		// thing it saw was an operand in a global. Every rule starts afresh.
		LastConditionType = CT_None;

		std::vector<Condition*> raw;
		if (!condition.empty())
			Condition::BuildConditions(raw, condition, settings);

		std::string actionText = action;
		Rule *rule = new Rule(raw, &actionText);
		rule->action.index = (unsigned int)rules.size();
		rules.push_back(rule);
	}

	const std::vector<Rule*>& Rules() const { return rules; }

private:
	std::vector<Rule*> rules;
	ItemFilterSettings settings;
};

// An item every ILVL condition below can be compared against, and a world that
// no condition below asks about.
ItemFacts AnItem() {
	ItemFacts facts = {};
	facts.level = 20;
	return facts;
}

// The indices of the rules whose actions came back, which is what the order of
// a walk amounts to.
std::vector<unsigned int> Indices(const std::vector<const Action*> &actions) {
	std::vector<unsigned int> indices;
	for (unsigned int i = 0; i < actions.size(); i++)
		indices.push_back(actions[i]->index);
	return indices;
}

}  // namespace

TEST_CASE("actions come back in the order the rules were written") {
	RuleList list;
	list.Add("", "%NAME% first%CONTINUE%");
	list.Add("", "%NAME% second%CONTINUE%");
	list.Add("", "%NAME% third%CONTINUE%");

	ItemFacts facts = AnItem();
	FilterContext context = {};
	std::vector<const Action*> actions =
		MatchingActions(list.Rules(), facts, context, PING_LEVEL_ALL);

	std::vector<unsigned int> expected;
	expected.push_back(0);
	expected.push_back(1);
	expected.push_back(2);
	CHECK(Indices(actions) == expected);
}

TEST_CASE("a rule that does not match is left out") {
	RuleList list;
	list.Add("ILVL>200", "%NAME% too high%CONTINUE%");
	list.Add("ILVL>5", "%NAME% matches%CONTINUE%");

	ItemFacts facts = AnItem();
	FilterContext context = {};
	std::vector<const Action*> actions =
		MatchingActions(list.Rules(), facts, context, PING_LEVEL_ALL);

	REQUIRE(actions.size() == 1);
	CHECK(actions[0]->index == 1);
}

TEST_CASE("the walk ends at the first rule that asks to stop") {
	RuleList list;
	list.Add("", "%NAME% carries on%CONTINUE%");
	list.Add("", "%NAME% stops");
	list.Add("", "%NAME% never reached%CONTINUE%");

	ItemFacts facts = AnItem();
	FilterContext context = {};
	std::vector<const Action*> actions =
		MatchingActions(list.Rules(), facts, context, PING_LEVEL_ALL);

	std::vector<unsigned int> expected;
	expected.push_back(0);
	expected.push_back(1);
	CHECK(Indices(actions) == expected);
}

TEST_CASE("a rule above the ping level is still returned") {
	// Whether a rule kept an item is counted from what comes back here, and a
	// tier setting is documented never to hide an item. So a rule too high a
	// tier to be drawn is still a rule that matched.
	RuleList list;
	list.Add("", "%NAME% high tier%dot-84%%TIER-5%");

	ItemFacts facts = AnItem();
	FilterContext context = {};
	std::vector<const Action*> actions =
		MatchingActions(list.Rules(), facts, context, 0);

	REQUIRE(actions.size() == 1);
	CHECK(actions[0]->index == 0);
	CHECK(actions[0]->pingLevel == 5);
}

TEST_CASE("a rule above the ping level does not stop the walk") {
	// The first rule asks to stop, but at a tier nothing is listening to, so it
	// stops nothing. The second is heard, and it does.
	RuleList list;
	list.Add("", "%NAME% high tier%dot-84%%TIER-5%");
	list.Add("", "%NAME% heard%dot-84%");
	list.Add("", "%NAME% never reached%dot-84%%CONTINUE%");

	ItemFacts facts = AnItem();
	FilterContext context = {};
	std::vector<const Action*> actions =
		MatchingActions(list.Rules(), facts, context, 0);

	std::vector<unsigned int> expected;
	expected.push_back(0);
	expected.push_back(1);
	CHECK(Indices(actions) == expected);
}

TEST_CASE("the same rule stops the walk once the ping level reaches it") {
	RuleList list;
	list.Add("", "%NAME% high tier%dot-84%%TIER-5%");
	list.Add("", "%NAME% never reached%dot-84%");

	ItemFacts facts = AnItem();
	FilterContext context = {};
	std::vector<const Action*> actions =
		MatchingActions(list.Rules(), facts, context, 5);

	REQUIRE(actions.size() == 1);
	CHECK(actions[0]->index == 0);
}

TEST_CASE("no rule is above every ping level") {
	// The lists a tier has no say over are walked with this, so the tier a rule
	// was written with must not change where such a walk stops.
	RuleList list;
	list.Add("", "%NAME% highest tier%TIER-9%");
	list.Add("", "%NAME% never reached%CONTINUE%");

	ItemFacts facts = AnItem();
	FilterContext context = {};
	std::vector<const Action*> actions =
		MatchingActions(list.Rules(), facts, context, PING_LEVEL_ALL);

	REQUIRE(actions.size() == 1);
	CHECK(actions[0]->index == 0);
}
