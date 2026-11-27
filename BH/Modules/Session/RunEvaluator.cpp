#include "RunEvaluator.h"

using namespace SessionEvents;
using Splits::Trigger;

namespace {
	const unsigned int kDifficulties = 3;
}

RunEvaluator::RunEvaluator() : running(false), difficulty(0), level(0) {}

void RunEvaluator::Start(const Splits::Route& toRun, unsigned int inDifficulty,
		unsigned int atLevel) {
	route = toRun;
	closed.assign(route.splits.size(), false);
	difficulty = inDifficulty;
	level = atLevel;
	quests.clear();
	killed.clear();
	superUniques.clear();
	areas.clear();
	items.clear();
	running = true;

	// What is already so reaches nothing: a route with a level to reach is not
	// reached by starting it above that level.
	for (size_t i = 0; i < route.splits.size(); i++) {
		if (Met(route.splits[i]))
			closed[i] = true;
	}
}

void RunEvaluator::Stop() {
	running = false;
}

std::vector<unsigned int> RunEvaluator::Feed(const Fact& fact) {
	std::vector<unsigned int> reached;
	if (!running)
		return reached;

	std::vector<bool> before(route.splits.size());
	for (size_t i = 0; i < route.splits.size(); i++)
		before[i] = Met(route.splits[i]);

	Take(fact);

	for (size_t i = 0; i < route.splits.size(); i++) {
		if (closed[i] || before[i] || !Met(route.splits[i]))
			continue;
		closed[i] = true;
		reached.push_back((unsigned int)i);
	}
	return reached;
}

void RunEvaluator::Take(const Fact& fact) {
	if (const Joined* joined = std::get_if<Joined>(&fact)) {
		difficulty = joined->difficulty;
		if (joined->level > level)
			level = joined->level;
	} else if (const LevelReached* reached = std::get_if<LevelReached>(&fact)) {
		if (reached->level > level)
			level = reached->level;
	} else if (const AreaEntered* entered = std::get_if<AreaEntered>(&fact)) {
		areas.insert({ difficulty, entered->area });
	} else if (const Killed* kills = std::get_if<Killed>(&fact)) {
		killed.insert({ difficulty, kills->classId });
	} else if (const Notable* notable = std::get_if<Notable>(&fact)) {
		killed.insert({ difficulty, notable->classId });
		if (notable->superUnique >= 0)
			superUniques.insert({ difficulty, (unsigned int)notable->superUnique });
	} else if (const Quest* quest = std::get_if<Quest>(&fact)) {
		// Kept as every bit the quest has had: some are set only while a quest
		// is under way, and a split is not unreached by one going out again.
		quests[{ difficulty, quest->quest }] |= quest->bits;
	} else if (const ItemHeld* held = std::get_if<ItemHeld>(&fact)) {
		items.insert({ difficulty, held->code });
	}
}

bool RunEvaluator::AllQuestsDone(unsigned int where) const {
	for (unsigned int quest : Splits::QuestIds()) {
		std::map<InDifficulty, unsigned int>::const_iterator found = quests.find({ where, quest });
		if (found == quests.end() || (found->second & Splits::kQuestDone) == 0)
			return false;
	}
	return true;
}

bool RunEvaluator::MetIn(const Trigger& trigger, unsigned int where) const {
	switch (trigger.kind) {
		case Trigger::Boss:
			return killed.count({ where, trigger.value }) > 0;
		case Trigger::SuperUnique:
			return superUniques.count({ where, trigger.value }) > 0;
		case Trigger::Quest: {
			std::map<InDifficulty, unsigned int>::const_iterator found =
				quests.find({ where, trigger.value });
			return found != quests.end() && (found->second & trigger.mask) != 0;
		}
		case Trigger::Area:
			return areas.count({ where, trigger.value }) > 0;
		case Trigger::Level:
			return level >= trigger.value;
		case Trigger::Item:
			return items.count({ where, trigger.code }) > 0;
		case Trigger::AllQuests:
			return AllQuestsDone(where);
	}
	return false;
}

bool RunEvaluator::Met(const Splits::Split& split) const {
	if (split.when.empty())
		return false;

	for (const Trigger& trigger : split.when) {
		bool met = false;
		if (split.difficulty >= 0) {
			met = MetIn(trigger, (unsigned int)split.difficulty);
		} else {
			for (unsigned int where = 0; where < kDifficulties && !met; where++)
				met = MetIn(trigger, where);
		}
		if (met && !split.all)
			return true;
		if (!met && split.all)
			return false;
	}
	return split.all;
}

void RunEvaluator::Close(unsigned int index) {
	if (index < closed.size())
		closed[index] = true;
}

void RunEvaluator::Reopen(unsigned int index) {
	if (index < closed.size())
		closed[index] = false;
}

unsigned int RunEvaluator::FirstOpen() const {
	for (size_t i = 0; i < closed.size(); i++) {
		if (!closed[i])
			return (unsigned int)i;
	}
	return (unsigned int)closed.size();
}
