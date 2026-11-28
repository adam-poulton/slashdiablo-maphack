#include "RunModel.h"
#include <algorithm>

using namespace PlayEvents;

RunModel::RunModel() : lastAt(0), playedMs(0), inGame(false), joinedAt(0), difficulty(0),
	level(0) {}

void RunModel::Apply(const Event& event) {
	const unsigned long long at = std::max(event.at, lastAt);
	std::visit([this, at](const auto& fact) { On(fact, at); }, event.fact);
	lastAt = at;
}

void RunModel::On(const Joined& fact, unsigned long long at) {
	// Whatever was last said is when a game that was never closed stopped.
	CloseGame(lastAt);
	inGame = true;
	joinedAt = at;
	difficulty = fact.difficulty;
	level = fact.level;
}

void RunModel::On(const Left& fact, unsigned long long at) {
	CloseGame(at);
}

void RunModel::On(const LevelReached& fact, unsigned long long at) {
	if (inGame && fact.level > level)
		level = fact.level;
}

void RunModel::CloseGame(unsigned long long at) {
	if (!inGame)
		return;
	inGame = false;
	playedMs += at > joinedAt ? at - joinedAt : 0;
}

unsigned long long RunModel::PlayedMs(unsigned long long now) const {
	return playedMs + (inGame && now > joinedAt ? now - joinedAt : 0);
}

RunModel::Run* RunModel::Active() {
	return !runs.empty() && !runs.back().ended ? &runs.back() : NULL;
}

const RunModel::Run* RunModel::ActiveRun() const {
	return !runs.empty() && !runs.back().ended ? &runs.back() : NULL;
}

unsigned long long RunModel::RunRealMs(unsigned long long now) const {
	const Run* run = ActiveRun();
	return run && now > run->startedAt ? now - run->startedAt : 0;
}

unsigned long long RunModel::RunGameMs(unsigned long long now) const {
	const Run* run = ActiveRun();
	if (!run)
		return 0;
	const unsigned long long played = PlayedMs(now);
	return played > run->startedPlayedMs ? played - run->startedPlayedMs : 0;
}

RunModel::Run RunModel::Unstarted(const std::string& route, const std::string& name,
		const std::vector<std::string>& splits) {
	Run run = {};
	run.route = route;
	run.name = name;
	for (const std::string& split : splits)
		run.splits.push_back({ split, Split::Open, false, 0, 0, 0 });
	return run;
}

void RunModel::EndRun(unsigned long long at, bool finished) {
	Run* run = Active();
	if (!run)
		return;
	run->realMs = RunRealMs(at);
	run->gameMs = RunGameMs(at);
	run->ended = true;
	run->finished = finished;
}

void RunModel::On(const RunStarted& fact, unsigned long long at) {
	// A run begun while another is going is the end of that one.
	EndRun(at, false);

	Run run = Unstarted(fact.route, fact.name, fact.splits);
	run.startedAt = at;
	run.startedPlayedMs = PlayedMs(at);
	runs.push_back(run);
}

void RunModel::On(const SplitReached& fact, unsigned long long at) {
	Run* run = Active();
	if (!run || fact.index >= run->splits.size())
		return;
	Split& split = run->splits[fact.index];
	split.state = Split::Reached;
	split.manual = fact.manual;
	split.realMs = RunRealMs(at);
	split.gameMs = RunGameMs(at);
	split.level = level;
}

void RunModel::On(const SplitUndone& fact, unsigned long long at) {
	Run* run = Active();
	if (!run || fact.index >= run->splits.size())
		return;
	const std::string name = run->splits[fact.index].name;
	run->splits[fact.index] = { name, Split::Open, false, 0, 0, 0 };
}

void RunModel::On(const SplitSkipped& fact, unsigned long long at) {
	Run* run = Active();
	if (!run || fact.index >= run->splits.size())
		return;
	const std::string name = run->splits[fact.index].name;
	run->splits[fact.index] = { name, Split::Skipped, true, 0, 0, 0 };
}

void RunModel::On(const RunEnded& fact, unsigned long long at) {
	EndRun(at, fact.finished);
}
