#include "SessionModel.h"
#include <algorithm>

using namespace SessionEvents;

namespace {
	double PerSecond(long long amount, unsigned long long ms) {
		return ms > 0 ? (double)amount / ((double)ms / 1000.0) : 0.0;
	}
}

SessionModel::SessionModel() :
	started(false), header(), lastAt(0), playedMs(0), gained(0), lost(0),
	levelsGained(0), deaths(0), inArea(false), area(0), enteredAt(0) {}

void SessionModel::Apply(const Event& event) {
	const unsigned long long at = std::max(event.at, lastAt);
	std::visit([this, at](const auto& fact) { On(fact, at); }, event.fact);
	lastAt = at;
}

void SessionModel::On(const Started& fact, unsigned long long at) {
	if (started)
		return;
	started = true;
	header = fact;
}

void SessionModel::On(const Joined& fact, unsigned long long at) {
	// Whatever the log last said is when a game it never closed stopped.
	CloseGame(lastAt);

	Game game = {};
	game.name = fact.game;
	game.difficulty = fact.difficulty;
	game.joinedAt = at;
	game.open = true;
	game.startLevel = game.level = fact.level;
	game.startExperience = game.experience = fact.experience;
	games.push_back(game);
}

void SessionModel::On(const Left& fact, unsigned long long at) {
	CloseGame(at);
}

void SessionModel::On(const AreaEntered& fact, unsigned long long at) {
	if (!InGame())
		return;
	LeaveArea(at);

	Area& entered = areas[fact.area];
	entered.town = fact.town;
	entered.entries++;
	inArea = true;
	area = fact.area;
	enteredAt = at;
}

void SessionModel::On(const Experience& fact, unsigned long long at) {
	if (!InGame())
		return;
	Game& game = games.back();
	if (fact.total == game.experience)
		return;

	if (fact.total > game.experience) {
		const unsigned long long amount = fact.total - game.experience;
		game.gained += amount;
		gained += amount;
		if (inArea)
			areas[area].gained += amount;
		recent.push_back({ at, (long long)amount });
	} else {
		const unsigned long long amount = game.experience - fact.total;
		game.lost += amount;
		lost += amount;
		recent.push_back({ at, -(long long)amount });
	}
	game.experience = fact.total;

	while (!recent.empty() && at - recent.front().at > kRecentMs)
		recent.pop_front();
}

void SessionModel::On(const LevelReached& fact, unsigned long long at) {
	if (!InGame())
		return;
	Game& game = games.back();
	if (fact.level <= game.level)
		return;
	levelsGained += fact.level - game.level;
	game.level = fact.level;
}

void SessionModel::On(const Died& fact, unsigned long long at) {
	if (!InGame())
		return;
	games.back().deaths++;
	deaths++;
}

void SessionModel::On(const Players& fact, unsigned long long at) {
	if (!InGame())
		return;
	Game& game = games.back();
	game.players = std::max(game.players, fact.count);
}

void SessionModel::LeaveArea(unsigned long long at) {
	if (!inArea)
		return;
	areas[area].ms += at - enteredAt;
	inArea = false;
}

void SessionModel::CloseGame(unsigned long long at) {
	if (!InGame())
		return;
	LeaveArea(at);
	Game& game = games.back();
	game.open = false;
	game.leftAt = at;
	playedMs += at - game.joinedAt;
}

unsigned long long SessionModel::GameMs(unsigned long long now) const {
	if (!InGame())
		return 0;
	const Game& game = games.back();
	return now > game.joinedAt ? now - game.joinedAt : 0;
}

unsigned long long SessionModel::PlayedMs(unsigned long long now) const {
	return playedMs + GameMs(now);
}

double SessionModel::GameRate(unsigned long long now) const {
	if (!InGame())
		return 0.0;
	const Game& game = games.back();
	return PerSecond((long long)game.gained - (long long)game.lost, GameMs(now));
}

double SessionModel::PlayedRate(unsigned long long now) const {
	return PerSecond((long long)gained - (long long)lost, PlayedMs(now));
}

double SessionModel::SessionRate(unsigned long long now) const {
	return PerSecond((long long)gained - (long long)lost, now);
}

unsigned long long SessionModel::PlayedBetween(unsigned long long from,
		unsigned long long to) const {
	unsigned long long played = 0;
	for (std::vector<Game>::const_reverse_iterator game = games.rbegin();
			game != games.rend(); ++game) {
		const unsigned long long end = game->open ? to : std::min(game->leftAt, to);
		if (end <= from)
			break;
		const unsigned long long begin = std::max(game->joinedAt, from);
		if (end > begin)
			played += end - begin;
	}
	return played;
}

double SessionModel::RecentRate(unsigned long long now,
		unsigned long long windowMs) const {
	windowMs = std::min(windowMs, kRecentMs);
	const unsigned long long from = now > windowMs ? now - windowMs : 0;

	long long amount = 0;
	for (std::deque<Change>::const_reverse_iterator change = recent.rbegin();
			change != recent.rend() && change->at >= from; ++change)
		amount += change->amount;
	return PerSecond(amount, PlayedBetween(from, now));
}

unsigned long long SessionModel::AreaMs(unsigned int which,
		unsigned long long now) const {
	std::map<unsigned int, Area>::const_iterator found = areas.find(which);
	unsigned long long ms = found != areas.end() ? found->second.ms : 0;
	if (inArea && area == which && now > enteredAt)
		ms += now - enteredAt;
	return ms;
}
