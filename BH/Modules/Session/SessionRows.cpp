#include "SessionRows.h"
#include <cctype>
#include <cstdio>
#include "SessionFormat.h"

using namespace SessionFormat;

namespace SessionRows {

namespace {
	const char* const kClasses[] = { "Amazon", "Sorceress", "Necromancer", "Paladin",
		"Barbarian", "Druid", "Assassin" };
	const char* const kDifficulties[] = { "Normal", "Nightmare", "Hell" };

	const unsigned long long kMinute = 60 * 1000;

	Row Heading(const std::string& text) {
		return { { text }, true, false };
	}

	Row Pair(const std::string& name, const std::string& figure) {
		return { { name, figure }, false, false };
	}

	std::string ClassOf(unsigned int classId) {
		return classId < sizeof(kClasses) / sizeof(kClasses[0]) ? kClasses[classId] : "";
	}

	std::string DifficultyOf(unsigned int difficulty) {
		return difficulty < sizeof(kDifficulties) / sizeof(kDifficulties[0]) ?
			kDifficulties[difficulty] : "";
	}

	std::string GameLabel(const std::string& name) {
		return name.empty() ? "Single player" : name;
	}

	long long Net(const SessionModel::Game& game) {
		return (long long)game.gained - (long long)game.lost;
	}

	unsigned long long LengthOf(const SessionModel::Game& game, unsigned long long now) {
		const unsigned long long end = game.open ? now : game.leftAt;
		return end > game.joinedAt ? end - game.joinedAt : 0;
	}

	double RateOf(long long amount, unsigned long long ms) {
		return ms > 0 ? (double)amount / ((double)ms / 1000.0) : 0.0;
	}

	std::string Signed(long long amount) {
		return (amount >= 0 ? "+" : "") + Count(amount);
	}

	std::string Who(const SessionModel& model) {
		const SessionEvents::Started& header = model.Header();
		std::string who = header.character;
		if (!model.Games().empty()) {
			who += ", level " + std::to_string(model.Games().back().level);
			const std::string named = ClassOf(header.classId);
			if (!named.empty())
				who += " " + named;
		}
		if (header.hardcore)
			who += " (Hardcore)";
		return who;
	}

	// How far through its level the character is, and how long the rest of it
	// takes at the rate the session has kept.
	std::string NextLevel(const SessionModel& model, unsigned long long now,
			const Level& level) {
		const unsigned long long experience = model.Games().back().experience;
		if (level.to <= level.from || experience < level.from || experience > level.to)
			return "";

		char text[64];
		sprintf_s(text, "%.1f%%", (double)(experience - level.from) * 100.0 /
			(double)(level.to - level.from));
		const double rate = model.PlayedRate(now);
		if (rate <= 0.0)
			return text;
		const double seconds = (double)(level.to - experience) / rate;
		// Past a hundred hours is not a wait worth putting a figure on.
		if (seconds > 360000.0)
			return text;
		return std::string(text) + ", " + ShortClock((unsigned long long)(seconds * 1000.0)) +
			" to go";
	}

	Row GameRow(const SessionModel::Game& game, unsigned int number, unsigned long long now) {
		const unsigned long long length = LengthOf(game, now);
		Row row = { std::vector<std::string>(GameColumns), false, false };
		row.cells[GameNumber] = std::to_string(number);
		row.cells[GameName] = GameLabel(game.name);
		row.cells[GameLength] = ShortClock(length);
		row.cells[GameExperience] = Signed(Net(game));
		row.cells[GameRate] = ShortRate(RateOf(Net(game), length));
		row.cells[GameLevel] = game.level > game.startLevel ?
			std::to_string(game.startLevel) + " - " + std::to_string(game.level) :
			std::to_string(game.level);
		return row;
	}

	struct Run {
		std::string name;
		unsigned int difficulty;
		std::vector<unsigned int> games;	// latest first
	};

	Row AverageRow(const SessionModel& model, const Run& run) {
		unsigned int finished = 0;
		unsigned long long length = 0;
		long long net = 0;
		for (unsigned int index : run.games) {
			const SessionModel::Game& game = model.Games()[index];
			if (game.open)
				continue;
			finished++;
			length += LengthOf(game, 0);
			net += Net(game);
		}

		Row row = { std::vector<std::string>(GameColumns), false, true };
		// One game is its own average.
		if (finished < 2)
			return row;
		row.cells[GameName] = "Average of " + std::to_string(finished);
		row.cells[GameLength] = ShortClock(length / finished);
		row.cells[GameExperience] = Signed(net / (long long)finished);
		row.cells[GameRate] = ShortRate(RateOf(net, length));
		return row;
	}
}

std::string RunNameOf(const std::string& game) {
	size_t end = game.size();
	while (end > 0 && std::isdigit((unsigned char)game[end - 1]))
		end--;
	// A name that is nothing but a number is no kind of run but itself.
	if (end == 0)
		return game;
	while (end > 0 && std::string(" -_.#").find(game[end - 1]) != std::string::npos)
		end--;
	return game.substr(0, end);
}

std::vector<Row> Overview(const SessionModel& model, unsigned long long now,
		const Level& level, const std::string& startedAt) {
	std::vector<Row> rows;
	if (!model.HasStarted())
		return rows;

	const long long net = (long long)model.Gained() - (long long)model.Lost();

	rows.push_back(Heading("Session"));
	rows.push_back(Pair("Character", Who(model)));
	if (!startedAt.empty())
		rows.push_back(Pair("Started", startedAt));
	rows.push_back(Pair("Session time", ShortClock(now)));
	rows.push_back(Pair("Time played", ShortClock(model.PlayedMs(now))));
	rows.push_back(Pair("Games", std::to_string(model.Games().size())));
	rows.push_back(Pair("Deaths", std::to_string(model.Deaths())));

	rows.push_back(Heading("Experience"));
	rows.push_back(Pair("Gained", Count((long long)model.Gained())));
	if (model.Lost() > 0) {
		rows.push_back(Pair("Lost to deaths", Count((long long)model.Lost())));
		rows.push_back(Pair("Net", Signed(net)));
	}
	rows.push_back(Pair("Per second played", ShortRate(model.PlayedRate(now))));
	rows.push_back(Pair("Per second with lobby time", ShortRate(model.SessionRate(now))));
	rows.push_back(Pair("Last minute", ShortRate(model.RecentRate(now, kMinute))));
	rows.push_back(Pair("Last five minutes", ShortRate(model.RecentRate(now, 5 * kMinute))));
	rows.push_back(Pair("Levels gained", std::to_string(model.LevelsGained())));
	if (!model.Games().empty()) {
		const std::string next = NextLevel(model, now, level);
		if (!next.empty())
			rows.push_back(Pair("Next level", next));
	}

	if (model.InGame()) {
		const SessionModel::Game& game = model.Games().back();
		const std::string difficulty = DifficultyOf(game.difficulty);
		rows.push_back(Heading("This game"));
		rows.push_back(Pair("Game", difficulty.empty() ? GameLabel(game.name) :
			GameLabel(game.name) + " (" + difficulty + ")"));
		rows.push_back(Pair("Time", ShortClock(model.GameMs(now))));
		rows.push_back(Pair("Experience", Signed(Net(game))));
		rows.push_back(Pair("Per second", ShortRate(model.GameRate(now))));
		if (game.players > 0)
			rows.push_back(Pair("Players", std::to_string(game.players)));
		if (game.deaths > 0)
			rows.push_back(Pair("Deaths", std::to_string(game.deaths)));
	}
	return rows;
}

std::vector<Row> Games(const SessionModel& model, unsigned long long now) {
	const std::vector<SessionModel::Game>& games = model.Games();

	// Latest game first, so a run is met in the order it was last played.
	std::vector<Run> runs;
	for (unsigned int i = (unsigned int)games.size(); i-- > 0;) {
		const std::string name = RunNameOf(games[i].name);
		Run* found = NULL;
		for (Run& run : runs) {
			if (run.name == name && run.difficulty == games[i].difficulty)
				found = &run;
		}
		if (!found) {
			runs.push_back({ name, games[i].difficulty, {} });
			found = &runs.back();
		}
		found->games.push_back(i);
	}

	std::vector<Row> rows;
	for (const Run& run : runs) {
		const std::string difficulty = DifficultyOf(run.difficulty);
		rows.push_back(Heading(difficulty.empty() ? GameLabel(run.name) :
			GameLabel(run.name) + " (" + difficulty + ")"));
		if (run.games.size() > 1) {
			const Row average = AverageRow(model, run);
			if (!average.cells[GameName].empty())
				rows.push_back(average);
		}
		for (unsigned int index : run.games)
			rows.push_back(GameRow(games[index], index + 1, now));
	}
	return rows;
}

}
