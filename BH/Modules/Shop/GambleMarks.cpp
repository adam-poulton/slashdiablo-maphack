#include "GambleMarks.h"
#include <algorithm>
#include <cctype>

namespace GambleMarks {

namespace {

std::string Root(std::map<std::string, std::string>& parent, const std::string& code) {
	std::string at = code;
	while (parent[at] != at)
		at = parent[at];
	parent[code] = at;
	return at;
}

void Join(std::map<std::string, std::string>& parent, const std::string& a,
		const std::string& b) {
	if (b.empty())
		return;
	if (parent.find(b) == parent.end())
		parent[b] = b;
	std::string ra = Root(parent, a);
	std::string rb = Root(parent, b);
	if (ra != rb)
		parent[rb] = ra;
}

const std::vector<std::string>* FamilyOf(const TierFamilies& families,
		const std::string& code) {
	TierFamilies::const_iterator found = families.find(code);
	return (found != families.end()) ? &found->second : NULL;
}

}  // namespace

TierFamilies FamiliesOf(const std::vector<BaseUpgrades>& bases) {
	std::map<std::string, std::string> parent;
	for (unsigned int i = 0; i < bases.size(); i++) {
		if (parent.find(bases[i].code) == parent.end())
			parent[bases[i].code] = bases[i].code;
	}
	for (unsigned int i = 0; i < bases.size(); i++) {
		Join(parent, bases[i].code, bases[i].exceptional);
		Join(parent, bases[i].code, bases[i].elite);
	}

	std::map<std::string, std::vector<std::string>> byRoot;
	for (std::map<std::string, std::string>::iterator it = parent.begin();
			it != parent.end(); ++it)
		byRoot[Root(parent, it->first)].push_back(it->first);

	TierFamilies families;
	for (std::map<std::string, std::string>::iterator it = parent.begin();
			it != parent.end(); ++it)
		families[it->first] = byRoot[Root(parent, it->first)];
	return families;
}

std::vector<std::string> Parse(const std::string& text) {
	std::vector<std::string> marks;
	std::string code;
	for (unsigned int i = 0; i <= text.size(); i++) {
		char c = (i < text.size()) ? text[i] : ',';
		if (c == ',' || isspace((unsigned char)c)) {
			if (!code.empty() && std::find(marks.begin(), marks.end(), code) == marks.end())
				marks.push_back(code);
			code.clear();
		} else {
			code += (char)tolower((unsigned char)c);
		}
	}
	return marks;
}

std::string Format(const std::vector<std::string>& marks) {
	std::string text;
	for (unsigned int i = 0; i < marks.size(); i++) {
		if (i > 0)
			text += ", ";
		text += marks[i];
	}
	return text;
}

void Toggle(std::vector<std::string>& marks, const std::string& code,
		bool tiers, const TierFamilies& families) {
	std::vector<std::string> covering(1, code);
	const std::vector<std::string>* family = tiers ? FamilyOf(families, code) : NULL;
	if (family)
		covering = *family;

	bool marked = false;
	for (unsigned int i = 0; i < covering.size(); i++) {
		std::vector<std::string>::iterator at =
			std::find(marks.begin(), marks.end(), covering[i]);
		if (at != marks.end()) {
			marks.erase(at);
			marked = true;
		}
	}
	if (!marked)
		marks.push_back(code);
}

std::set<std::string> Expand(const std::vector<std::string>& marks, bool tiers,
		const TierFamilies& families) {
	std::set<std::string> codes;
	for (unsigned int i = 0; i < marks.size(); i++) {
		codes.insert(marks[i]);
		const std::vector<std::string>* family = tiers ? FamilyOf(families, marks[i]) : NULL;
		if (family)
			codes.insert(family->begin(), family->end());
	}
	return codes;
}

}
