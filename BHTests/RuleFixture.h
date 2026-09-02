#pragma once
#include <string>
#include <vector>
#include "ItemFilter.h"

/*
 * A list of rules read the way the game reads one, for a test that wants to
 * walk them. Owns the rules and the conditions under them.
 */
class RuleList {
public:
	RuleList();
	~RuleList();

	// condition is what a rule is written with, its tokens separated by spaces
	// as a filter line separates them, and empty for a rule that matches
	// everything.
	void Add(const std::string& condition, const std::string& action);

	const std::vector<Rule*>& Rules() const { return rules; }

private:
	std::vector<Rule*> rules;
	ItemFilterSettings settings;
};
