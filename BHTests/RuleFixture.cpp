#include "RuleFixture.h"

RuleList::RuleList() {
	settings.statMax = 512;
	settings.skillMax = 512;
}

RuleList::~RuleList() {
	for (unsigned int i = 0; i < rules.size(); i++) {
		for (unsigned int c = 0; c < rules[i]->conditions.size(); c++)
			delete rules[i]->conditions[c];
		delete rules[i];
	}
}

void RuleList::Add(const std::string& condition, const std::string& action) {
	// Reading a rule is not reentrant: the parser tracks whether the last thing
	// it saw was an operand in a global, so that it can put the AND between two
	// conditions written side by side. Every rule starts afresh.
	LastConditionType = CT_None;

	std::vector<Condition*> raw;
	std::size_t at = 0;
	while (at <= condition.length()) {
		std::size_t space = condition.find(' ', at);
		if (space == std::string::npos)
			space = condition.length();
		std::string token = condition.substr(at, space - at);
		at = space + 1;
		if (!token.empty())
			Condition::BuildConditions(raw, token, settings);
	}

	std::string actionText = action;
	Rule* rule = new Rule(raw, &actionText);
	rule->action.index = (unsigned int)rules.size();
	rules.push_back(rule);
}
