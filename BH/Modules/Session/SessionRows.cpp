#include "SessionRows.h"
#include <algorithm>
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

	// "1,234", and what of them were more than ordinary: "1,234 (12 champion, 9 unique)".
	std::string KillSummary(const SessionModel::KillCounts& kills) {
		std::string text = Count(kills.Total());
		const unsigned int champions = kills.byTier[SessionEvents::TierChampion];
		const unsigned int uniques = kills.Uniques();
		if (champions == 0 && uniques == 0)
			return text;
		text += " (";
		if (champions > 0)
			text += Count(champions) + " champion";
		if (uniques > 0)
			text += std::string(champions > 0 ? ", " : "") + Count(uniques) + " unique";
		return text + ")";
	}

	// A count, or nothing where there is nothing to count: a column of zeroes
	// hides the figures in it.
	std::string Some(unsigned int count) {
		return count > 0 ? Count(count) : std::string();
	}

	std::string AreaLabel(const SessionModel& model, unsigned int area) {
		std::map<unsigned int, SessionModel::Area>::const_iterator known = model.Areas().find(area);
		if (known != model.Areas().end() && !known->second.name.empty())
			return known->second.name;
		return "Area " + std::to_string(area);
	}

	std::string Lower(std::string text) {
		for (char& c : text)
			c = (char)std::tolower((unsigned char)c);
		return text;
	}

	bool Has(const Row& row, const std::string& lowered) {
		for (const std::string& cell : row.cells) {
			if (Lower(cell).find(lowered) != std::string::npos)
				return true;
		}
		return false;
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
	rows.push_back(Pair("Kills", KillSummary(model.KillTotals())));
	rows.push_back(Pair("Drops", std::to_string(model.Drops().size())));

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

std::vector<Row> Kills(const SessionModel& model, unsigned long long now) {
	using SessionEvents::TierChampion;
	std::vector<Row> rows;

	// Wherever something died, and wherever outside a town time was spent.
	std::vector<unsigned int> fought;
	for (const auto& area : model.Areas()) {
		if (area.second.kills.Total() > 0 || (!area.second.town && model.AreaMs(area.first, now) > 0))
			fought.push_back(area.first);
	}
	std::stable_sort(fought.begin(), fought.end(), [&model](unsigned int a, unsigned int b) {
		return model.Areas().at(a).kills.Total() > model.Areas().at(b).kills.Total();
	});
	if (!fought.empty())
		rows.push_back(Heading("Areas"));
	for (unsigned int id : fought) {
		const SessionModel::Area& area = model.Areas().at(id);
		Row row = { std::vector<std::string>(KillColumns), false, false };
		row.cells[KillName] = AreaLabel(model, id);
		row.cells[KillTotal] = Some(area.kills.Total());
		row.cells[KillChampions] = Some(area.kills.byTier[TierChampion]);
		row.cells[KillUniques] = Some(area.kills.Uniques());
		row.cells[KillTime] = ShortClock(model.AreaMs(id, now));
		row.cells[KillDetail] = area.gained > 0 ? ShortAmount((long long)area.gained) + " xp" : "";
		rows.push_back(row);
	}

	std::vector<unsigned int> kinds;
	for (const auto& monster : model.Monsters())
		kinds.push_back(monster.first);
	std::stable_sort(kinds.begin(), kinds.end(), [&model](unsigned int a, unsigned int b) {
		return model.Monsters().at(a).kills.Total() > model.Monsters().at(b).kills.Total();
	});
	if (!kinds.empty())
		rows.push_back(Heading("Monsters"));
	for (unsigned int id : kinds) {
		const SessionModel::Monster& monster = model.Monsters().at(id);
		Row row = { std::vector<std::string>(KillColumns), false, false };
		row.cells[KillName] = monster.name.empty() ? "Monster " + std::to_string(id) : monster.name;
		row.cells[KillTotal] = Some(monster.kills.Total());
		row.cells[KillChampions] = Some(monster.kills.byTier[TierChampion]);
		row.cells[KillUniques] = Some(monster.kills.Uniques());
		rows.push_back(row);
	}

	if (!model.Notables().empty())
		rows.push_back(Heading("Unique monsters"));
	for (size_t i = model.Notables().size(); i-- > 0;) {
		const SessionModel::NotableKill& kill = model.Notables()[i];
		Row row = { std::vector<std::string>(KillColumns), false, false };
		row.cells[KillName] = kill.name;
		row.cells[KillTime] = ShortClock(kill.at);
		row.cells[KillDetail] = AreaLabel(model, kill.area);
		rows.push_back(row);
	}
	return rows;
}

std::vector<Row> Drops(const SessionModel& model) {
	std::vector<Row> rows;
	for (size_t i = model.Drops().size(); i-- > 0;) {
		const SessionModel::Drop& drop = model.Drops()[i];
		Row row = { std::vector<std::string>(DropColumns), false, false };
		row.cells[DropTime] = ShortClock(drop.at);
		row.cells[DropItem] = drop.item.name;
		row.cells[DropArea] = drop.item.areaName.empty() ? AreaLabel(model, drop.item.area) :
			drop.item.areaName;
		row.cells[DropGame] = std::to_string(drop.game + 1);
		row.cells[DropPing] = std::to_string(drop.item.ping);
		row.colors.assign(DropColumns, -1);
		row.colors[DropItem] = (int)drop.item.color;
		rows.push_back(row);
	}
	return rows;
}

namespace {
	const char* const kRoutePrefix = "route:";

	std::string TimeOrNone(long long ms) {
		return ms >= 0 ? ShortClock((unsigned long long)ms) : std::string();
	}

	const RunStore::Best* BestFor(const SessionModel& model, const RunsShown& shown,
			const std::string& route) {
		if (!shown.store)
			return NULL;
		const SessionEvents::Started& header = model.Header();
		return shown.store->Find(RunStore::KeyFor(route, header.classId, header.hardcore));
	}

	Row SplitRow(const RunCompare::Line& line, const RunStore::Times* times, unsigned int index,
			bool current) {
		Row row = { std::vector<std::string>(SplitColumns), false, false };
		row.colors.assign(SplitColumns, -1);
		row.cells[SplitName] = (current ? "> " : "") + line.name;
		if (line.state == SessionModel::RunSplit::Reached)
			row.cells[SplitTime] = TimeOrNone(line.time);
		else if (line.state == SessionModel::RunSplit::Skipped)
			row.cells[SplitTime] = "-";
		if (line.compared) {
			row.cells[SplitDelta] = Delta(line.delta);
			row.colors[SplitDelta] = line.gold ? kColorBest :
				(line.delta <= 0 ? kColorAhead : kColorBehind);
		}
		row.cells[SplitBest] = TimeOrNone(line.best);
		if (times && index < times->segments.size())
			row.cells[SplitSegment] = TimeOrNone(times->segments[index]);
		if (line.level > 0)
			row.cells[SplitLevel] = std::to_string(line.level);
		return row;
	}

	Row Summary(const std::string& name, const std::string& figure) {
		Row row = { std::vector<std::string>(SplitColumns), false, true };
		row.cells[SplitName] = name;
		row.cells[SplitTime] = figure;
		return row;
	}

	// What became of a run that is over.
	std::string Outcome(const SessionModel::Run& run) {
		if (run.finished)
			return "finished";
		size_t reached = 0;
		for (const SessionModel::RunSplit& split : run.splits) {
			if (split.state == SessionModel::RunSplit::Reached)
				reached++;
		}
		return "given up after " + std::to_string(reached) + " of " + std::to_string(run.splits.size());
	}
}

std::string RouteKey(const std::string& routeId) {
	return kRoutePrefix + routeId;
}

std::string RouteOf(const std::string& key) {
	const std::string prefix = kRoutePrefix;
	return key.compare(0, prefix.size(), prefix) == 0 ? key.substr(prefix.size()) : std::string();
}

std::vector<Row> Runs(const SessionModel& model, const RunsShown& shown) {
	std::vector<Row> rows;
	const SessionModel::Run* active = model.ActiveRun();

	if (active) {
		const RunStore::Best* best = BestFor(model, shown, active->route);
		const unsigned long long clock = shown.clock == RunStore::RealTime ?
			model.RunRealMs(shown.now) : model.RunGameMs(shown.now);
		const RunCompare::Lines lines = RunCompare::Compare(*active, best, shown.clock, clock);
		const RunStore::Times* times = best && best->IsOf(RunStore::SplitsOf(*active)) ?
			&best->by[shown.clock] : NULL;

		rows.push_back(Heading(active->name));
		for (unsigned int i = 0; i < lines.splits.size(); i++)
			rows.push_back(SplitRow(lines.splits[i], times, i, (int)i == lines.current));
		rows.push_back(Summary("Run time", ShortClock(clock)));
		if (lines.bestTotal >= 0)
			rows.push_back(Summary("Best run", ShortClock((unsigned long long)lines.bestTotal)));
		if (lines.sumOfBest >= 0)
			rows.push_back(Summary("Sum of best", ShortClock((unsigned long long)lines.sumOfBest)));
		if (lines.bestPossible >= 0)
			rows.push_back(Summary("Best possible", ShortClock((unsigned long long)lines.bestPossible)));
		if (best && best->attempts > 0)
			rows.push_back(Summary("Finished", std::to_string(best->finished) + " of " +
				std::to_string(best->attempts)));
	}

	if (shown.routes && !shown.routes->empty()) {
		rows.push_back(Heading("Routes"));
		for (const ::Splits::Route& route : *shown.routes) {
			const RunStore::Best* best = BestFor(model, shown, route.id);
			std::vector<std::string> names;
			for (const ::Splits::Split& split : route.splits)
				names.push_back(split.name);
			const RunStore::Times* times = best && best->IsOf(names) ? &best->by[shown.clock] : NULL;
			Row row = { std::vector<std::string>(SplitColumns), false, false };
			row.key = RouteKey(route.id);
			row.cells[SplitName] = (route.id == shown.selected ? "> " : "") + route.name;
			row.cells[SplitTime] = std::to_string(route.splits.size()) +
				(route.splits.size() == 1 ? " split" : " splits");
			if (times)
				row.cells[SplitBest] = TimeOrNone(times->total);
			rows.push_back(row);
		}
	}

	// Latest first, and not the one in hand, which is at the top.
	bool headed = false;
	for (size_t i = model.Runs().size(); i-- > 0;) {
		const SessionModel::Run& run = model.Runs()[i];
		if (!run.ended)
			continue;
		if (!headed) {
			rows.push_back(Heading("Runs this session"));
			headed = true;
		}
		Row row = { std::vector<std::string>(SplitColumns), false, false };
		row.cells[SplitName] = run.name + ", " + Outcome(run);
		row.cells[SplitTime] = ShortClock(shown.clock == RunStore::RealTime ? run.realMs : run.gameMs);
		rows.push_back(row);
	}
	return rows;
}

namespace {
	const char* const kSessionPrefix = "session:";
	const char* const kMarker = "> ";

	Row ActionRow(const std::string& text, const std::string& what) {
		Row row = { std::vector<std::string>(RouteColumns), false, false };
		row.cells[RouteName] = text;
		row.colors.assign(RouteColumns, -1);
		row.colors[RouteName] = kColorAction;
		row.key = RouteKeys::kDo + what;
		return row;
	}

	Row Note(const std::string& text) {
		Row row = { std::vector<std::string>(RouteColumns), false, true };
		row.cells[RouteName] = text;
		return row;
	}

	std::string SplitCount(size_t splits) {
		return std::to_string(splits) + (splits == 1 ? " split" : " splits");
	}

	bool PresetMissing(const std::vector<::Splits::Route>& routes) {
		for (const ::Splits::Route& preset : ::Splits::Presets()) {
			bool there = false;
			for (const ::Splits::Route& route : routes)
				there = there || route.id == preset.id;
			if (!there)
				return true;
		}
		return false;
	}
}

std::string SessionKey(const std::string& file) {
	return kSessionPrefix + file;
}

bool SessionOf(const std::string& key, std::string& file) {
	return KeyRest(key, kSessionPrefix, file);
}

bool KeyRest(const std::string& key, const char* prefix, std::string& rest) {
	const std::string start = prefix;
	if (key.compare(0, start.size(), start) != 0)
		return false;
	rest = key.substr(start.size());
	return true;
}

std::vector<Row> History(const std::vector<SessionIndex::Entry>& entries,
		const HistoryShown& shown) {
	std::vector<Row> rows;
	if (shown.hasCurrent) {
		Row row = { std::vector<std::string>(HistoryColumns), false, false };
		row.cells[HistoryStarted] = (shown.viewing.empty() ? kMarker : "") +
			std::string("This session");
		row.key = SessionKey("");
		rows.push_back(row);
	}

	if (!entries.empty())
		rows.push_back(Heading("Past sessions"));
	for (const SessionIndex::Entry& entry : entries) {
		Row row = { std::vector<std::string>(HistoryColumns), false, false };
		row.cells[HistoryStarted] = (entry.file == shown.viewing ? kMarker : "") +
			(shown.started ? shown.started(entry.startedAt) : std::string());
		row.cells[HistoryCharacter] = entry.character + (entry.hardcore ? " (HC)" : "");
		row.cells[HistoryLevel] = entry.level > 0 ? std::to_string(entry.level) : std::string();
		row.cells[HistoryPlayed] = ShortClock(entry.playedMs);
		row.cells[HistoryGames] = std::to_string(entry.games);
		row.cells[HistoryExperience] = ShortAmount(entry.experience);
		row.cells[HistoryRate] = ShortRate(RateOf(entry.experience, entry.playedMs));
		row.cells[HistoryKills] = Some(entry.kills);
		row.cells[HistoryDrops] = Some(entry.drops);
		row.key = SessionKey(entry.file);
		rows.push_back(row);
	}
	return rows;
}

std::vector<Row> Routes(const RoutesShown& shown) {
	std::vector<Row> rows;
	if (!shown.routes)
		return rows;
	const std::vector<::Splits::Route>& routes = *shown.routes;

	const ::Splits::Route* editing = NULL;
	for (const ::Splits::Route& route : routes) {
		if (route.id == shown.editing)
			editing = &route;
	}

	rows.push_back(Heading("Routes"));
	for (const ::Splits::Route& route : routes) {
		Row row = { std::vector<std::string>(RouteColumns), false, false };
		row.cells[RouteName] = (&route == editing ? kMarker : "") + route.name;
		row.cells[RouteDetail] = SplitCount(route.splits.size()) +
			(route.id == shown.chosen ? ", the route runs are timed on" : "");
		row.key = RouteKeys::kEdit + route.id;
		rows.push_back(row);
	}
	rows.push_back(ActionRow("New route", "new"));
	if (editing)
		rows.push_back(ActionRow("Copy " + editing->name, "copy"));
	if (PresetMissing(routes))
		rows.push_back(ActionRow("Put back the routes BH comes with", "restore"));
	if (!editing)
		return rows;

	const bool picked = shown.split >= 0 && (size_t)shown.split < editing->splits.size();
	rows.push_back(Heading(editing->name));
	for (size_t i = 0; i < editing->splits.size(); i++) {
		const ::Splits::Split& split = editing->splits[i];
		Row row = { std::vector<std::string>(RouteColumns), false, false };
		row.cells[RouteName] = (picked && (size_t)shown.split == i ? kMarker : "") +
			std::to_string(i + 1) + ". " + split.name;
		row.cells[RouteDetail] = RouteEdit::Describe(split);
		row.key = RouteKeys::kPick + std::to_string(i);
		rows.push_back(row);
	}
	if (editing->splits.empty())
		rows.push_back(Note("No splits yet"));
	if (picked) {
		const std::string& name = editing->splits[shown.split].name;
		if (shown.split > 0)
			rows.push_back(ActionRow("Move " + name + " up", "up"));
		if ((size_t)shown.split + 1 < editing->splits.size())
			rows.push_back(ActionRow("Move " + name + " down", "down"));
		rows.push_back(ActionRow("Remove " + name, "remove"));
	}
	if (editing->id != shown.chosen && !editing->splits.empty())
		rows.push_back(ActionRow("Time runs on this route", "use"));
	if (!shown.text.empty() && shown.text != editing->name)
		rows.push_back(ActionRow("Rename this route to \"" + shown.text + "\"", "rename"));
	if (routes.size() > 1) {
		Row row = ActionRow(shown.deleting ? "Click again to delete " + editing->name :
			"Delete this route", "delete");
		if (shown.deleting)
			row.colors[RouteName] = kColorBehind;
		rows.push_back(row);
	}

	// What there is to add, under the difficulty each is of. The search box
	// narrows these and nothing above them.
	const std::string wanted = Lower(shown.text);
	::Splits::Split level;
	if (RouteEdit::LevelSplit(shown.text, level)) {
		rows.push_back(Heading("Add a split: any difficulty"));
		Row row = { std::vector<std::string>(RouteColumns), false, false };
		row.cells[RouteName] = level.name;
		row.cells[RouteDetail] = RouteEdit::Describe(level);
		row.key = std::string(RouteKeys::kAdd) + "level";
		rows.push_back(row);
	}
	const std::vector<::Splits::Split>& catalogue = RouteEdit::Catalogue();
	int difficulty = -2;
	for (size_t i = 0; i < catalogue.size(); i++) {
		const ::Splits::Split& split = catalogue[i];
		const std::string described = RouteEdit::Describe(split);
		if (!wanted.empty() && Lower(split.name).find(wanted) == std::string::npos &&
				Lower(described).find(wanted) == std::string::npos)
			continue;
		if (split.difficulty != difficulty) {
			difficulty = split.difficulty;
			rows.push_back(Heading("Add a split: " + DifficultyOf((unsigned int)difficulty)));
		}
		Row row = { std::vector<std::string>(RouteColumns), false, false };
		row.cells[RouteName] = split.name;
		row.cells[RouteDetail] = described;
		row.key = RouteKeys::kAdd + std::to_string(i);
		rows.push_back(row);
	}
	return rows;
}

std::vector<Row> Matching(const std::vector<Row>& rows, const std::string& text) {
	if (text.empty())
		return rows;
	const std::string lowered = Lower(text);

	std::vector<Row> matching;
	const Row* heading = NULL;		// the one the row in hand stands under
	bool headingShown = false;
	bool wholeSection = false;		// the heading itself has the text
	for (const Row& row : rows) {
		if (row.heading) {
			heading = &row;
			wholeSection = Has(row, lowered);
			headingShown = wholeSection;
			if (wholeSection)
				matching.push_back(row);
			continue;
		}
		if (row.summary || (!wholeSection && !Has(row, lowered)))
			continue;
		if (heading && !headingShown) {
			matching.push_back(*heading);
			headingShown = true;
		}
		matching.push_back(row);
	}
	return matching;
}

}
