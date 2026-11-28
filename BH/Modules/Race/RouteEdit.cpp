#include "RouteEdit.h"
#include <cctype>
#include <cstdlib>

using Splits::Route;
using Splits::Split;
using Splits::Trigger;

namespace RouteEdit {

namespace {
	const char* const kDifficulties[] = { "Normal", "Nightmare", "Hell" };
	const char* const kSuffixes[] = { "", " (NM)", " (Hell)" };

	struct Named {
		const char* name;
		unsigned int value;
		unsigned int mask;
	};

	// The act bosses: their rows in MonStats.txt, and the quest each one's
	// death moves on. Duriel's is the bit his death sets.
	const Named kBosses[] = { { "Andariel", 156, 0 }, { "Duriel", 211, 0 }, { "Mephisto", 242, 0 },
		{ "Diablo", 243, 0 }, { "Baal", 544, 0 } };
	const Named kBossQuests[] = { { "", 6, Splits::kQuestDone }, { "", 14, 0x0020 },
		{ "", 22, Splits::kQuestDone }, { "", 26, Splits::kQuestDone }, { "", 40, Splits::kQuestDone } };

	// The other quests, by what a player calls each, and the Cow King, who is
	// a bit of The Search for Cain.
	const Named kQuests[] = {
		{ "Den of Evil", 1, Splits::kQuestDone }, { "Blood Raven", 2, Splits::kQuestDone },
		{ "Horadric Malus", 3, Splits::kQuestDone }, { "Cain", 4, Splits::kQuestDone },
		{ "Countess", 5, Splits::kQuestDone }, { "Radament", 9, Splits::kQuestDone },
		{ "Horadric Staff quest", 10, Splits::kQuestDone }, { "Tainted Sun", 11, Splits::kQuestDone },
		{ "Arcane Sanctuary", 12, Splits::kQuestDone }, { "Summoner", 13, Splits::kQuestDone },
		{ "Lam Esen's Tome", 17, Splits::kQuestDone }, { "Khalim's Will quest", 18, Splits::kQuestDone },
		{ "Gidbinn", 19, Splits::kQuestDone }, { "Golden Bird", 20, Splits::kQuestDone },
		{ "Travincal", 21, Splits::kQuestDone }, { "Izual", 25, Splits::kQuestDone },
		{ "Hellforge", 27, Splits::kQuestDone }, { "Shenk", 35, Splits::kQuestDone },
		{ "Rescue on Mount Arreat", 36, Splits::kQuestDone }, { "Anya", 37, Splits::kQuestDone },
		{ "Nihlathak", 38, Splits::kQuestDone }, { "Ancients", 39, Splits::kQuestDone },
		{ "Cow King", 4, 0x0400 },
	};

	struct Item {
		const char* name;
		const char* code;
	};
	const Item kItems[] = { { "Horadric Cube", "box" }, { "Staff of Kings", "msf" },
		{ "Viper Amulet", "vip" }, { "Horadric Staff", "hst" }, { "Khalim's Eye", "qey" },
		{ "Khalim's Heart", "qhr" }, { "Khalim's Brain", "qbr" }, { "Khalim's Flail", "qf1" },
		{ "Khalim's Will", "qf2" } };

	const unsigned int kHighestLevel = 99;

	std::vector<Split> BuildCatalogue() {
		std::vector<Split> splits;
		for (int difficulty = 0; difficulty < 3; difficulty++) {
			const std::string suffix = kSuffixes[difficulty];
			for (size_t i = 0; i < sizeof(kBosses) / sizeof(kBosses[0]); i++) {
				splits.push_back({ kBosses[i].name + suffix, difficulty, false, {
					{ Trigger::Boss, kBosses[i].value, 0, "" },
					{ Trigger::Quest, kBossQuests[i].value, kBossQuests[i].mask, "" } } });
			}
			for (const Named& quest : kQuests) {
				splits.push_back({ quest.name + suffix, difficulty, false,
					{ { Trigger::Quest, quest.value, quest.mask, "" } } });
			}
			for (const Item& item : kItems) {
				splits.push_back({ item.name + suffix, difficulty, false,
					{ { Trigger::Item, 0, 0, item.code } } });
			}
			splits.push_back({ "All quests" + suffix, difficulty, false,
				{ { Trigger::AllQuests, 0, 0, "" } } });
		}
		return splits;
	}

	std::string Lower(std::string text) {
		for (char& c : text)
			c = (char)std::tolower((unsigned char)c);
		return text;
	}

	std::string Kind(const Trigger& trigger) {
		switch (trigger.kind) {
			case Trigger::Boss:			return "kill";
			case Trigger::SuperUnique:	return "kill";
			case Trigger::Quest:		return "quest";
			case Trigger::Area:			return "area " + std::to_string(trigger.value);
			case Trigger::Level:		return "level " + std::to_string(trigger.value);
			case Trigger::Item:			return "item " + trigger.code;
			case Trigger::AllQuests:	return "all quests";
		}
		return std::string();
	}

	Route* Find(std::vector<Route>& routes, const std::string& id) {
		for (Route& route : routes) {
			if (route.id == id)
				return &route;
		}
		return NULL;
	}

	bool NameTaken(const std::vector<Route>& routes, const std::string& name) {
		for (const Route& route : routes) {
			if (Lower(route.name) == Lower(name))
				return true;
		}
		return false;
	}
}

const std::vector<Split>& Catalogue() {
	static const std::vector<Split> catalogue = BuildCatalogue();
	return catalogue;
}

bool LevelSplit(const std::string& text, Split& split) {
	const std::string asked = Lower(text);
	const std::string word = "level ";
	if (asked.compare(0, word.size(), word) != 0 || asked.size() == word.size())
		return false;
	for (size_t i = word.size(); i < asked.size(); i++) {
		if (!std::isdigit((unsigned char)asked[i]))
			return false;
	}
	const unsigned long level = std::strtoul(asked.c_str() + word.size(), NULL, 10);
	if (level < 2 || level > kHighestLevel)
		return false;
	split = { "Level " + std::to_string(level), -1, false,
		{ { Trigger::Level, (unsigned int)level, 0, "" } } };
	return true;
}

std::string DifficultyOf(const Split& split) {
	return split.difficulty >= 0 && split.difficulty < 3 ? kDifficulties[split.difficulty] :
		"Any difficulty";
}

std::string MarkedBy(const Split& split) {
	if (split.when.empty())
		return "hand";
	std::string text;
	for (const Trigger& trigger : split.when) {
		if (!text.empty())
			text += split.all ? " and " : " or ";
		text += Kind(trigger);
	}
	return text;
}

std::string Describe(const Split& split) {
	if (split.when.empty())
		return "marked by hand";
	const bool anyDifficulty = split.difficulty < 0 || split.difficulty >= 3;
	return (anyDifficulty ? std::string() : DifficultyOf(split) + ": ") + MarkedBy(split);
}

std::string Add(std::vector<Route>& routes, const Route* copyOf) {
	Route route = copyOf ? *copyOf : Route();
	unsigned int number = 1;
	while (Find(routes, "route-" + std::to_string(number)) ||
			NameTaken(routes, "My route " + std::to_string(number)))
		number++;
	route.id = "route-" + std::to_string(number);
	route.name = "My route " + std::to_string(number);
	routes.push_back(route);
	return route.id;
}

void Remove(std::vector<Route>& routes, const std::string& id) {
	for (size_t i = 0; i < routes.size(); i++) {
		if (routes[i].id == id) {
			routes.erase(routes.begin() + i);
			return;
		}
	}
}

bool Rename(std::vector<Route>& routes, const std::string& id, const std::string& name) {
	Route* route = Find(routes, id);
	if (!route || name.empty())
		return false;
	for (const Route& other : routes) {
		if (&other != route && Lower(other.name) == Lower(name))
			return false;
	}
	route->name = name;
	return true;
}

void Restore(std::vector<Route>& routes) {
	for (const Route& preset : Splits::Presets()) {
		if (!Find(routes, preset.id))
			routes.push_back(preset);
	}
}

bool PresetsMissing(const std::vector<Route>& routes) {
	for (const Route& preset : Splits::Presets()) {
		bool there = false;
		for (const Route& route : routes)
			there = there || route.id == preset.id;
		if (!there)
			return true;
	}
	return false;
}

unsigned int AddSplit(Route& route, const Split& split, int after) {
	const bool inside = after >= 0 && (size_t)after < route.splits.size();
	const size_t at = inside ? (size_t)after + 1 : route.splits.size();
	route.splits.insert(route.splits.begin() + at, split);
	return (unsigned int)at;
}

void RemoveSplit(Route& route, unsigned int index) {
	if (index < route.splits.size())
		route.splits.erase(route.splits.begin() + index);
}

bool MoveSplit(Route& route, unsigned int index, int by) {
	if (index >= route.splits.size() || (by != -1 && by != 1))
		return false;
	if ((by < 0 && index == 0) || (by > 0 && index + 1 == route.splits.size()))
		return false;
	std::swap(route.splits[index], route.splits[index + by]);
	return true;
}

}
