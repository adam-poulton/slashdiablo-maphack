#include "RaceRows.h"
#include <cctype>
#include "../Tracking/PlayFormat.h"

using namespace PlayFormat;
using ListRows::Heading;

namespace RaceRows {

namespace {
	std::string Lower(std::string text) {
		for (char& c : text)
			c = (char)std::tolower((unsigned char)c);
		return text;
	}

	std::string TimeOrNone(long long ms) {
		return ms >= 0 ? ShortClock((unsigned long long)ms) : std::string();
	}

	Row SplitRow(const RunCompare::Line& line, const RunStore::Times* times, unsigned int index,
			bool current) {
		Row row = { std::vector<std::string>(SplitColumns), false, false };
		row.colors.assign(SplitColumns, -1);
		row.cells[SplitName] = (current ? "> " : "") + line.name;
		if (line.state == RunModel::Split::Reached)
			row.cells[SplitTime] = TimeOrNone(line.time);
		else if (line.state == RunModel::Split::Skipped)
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
	std::string Outcome(const RunModel::Run& run) {
		if (run.finished)
			return "Finished";
		size_t reached = 0;
		for (const RunModel::Split& split : run.splits) {
			if (split.state == RunModel::Split::Reached)
				reached++;
		}
		return "Stopped after " + std::to_string(reached) + " of " + std::to_string(run.splits.size());
	}

	// A run set out split by split, and what is known of its route under it.
	// It stands under no heading: whose splits they are is said over the list.
	// `going` is whether its clock is running: one that is not has no split it
	// is on and no time of its own.
	void AddRun(const RunModel::Run& run, bool going, unsigned long long clock,
			const RunsShown& shown, std::vector<Row>& rows) {
		const RunStore::Best* best = shown.store ? shown.store->Find(
			RunStore::KeyFor(run.route, shown.classId, shown.hardcore)) : NULL;
		const RunCompare::Lines lines = RunCompare::Compare(run, best, shown.clock, clock);
		const RunStore::Times* times = best && best->IsOf(RunStore::SplitsOf(run)) ?
			&best->by[shown.clock] : NULL;

		for (unsigned int i = 0; i < lines.splits.size(); i++)
			rows.push_back(SplitRow(lines.splits[i], times, i, going && (int)i == lines.current));
		if (going)
			rows.push_back(Summary("Run time", ShortClock(clock)));
		if (lines.bestTotal >= 0)
			rows.push_back(Summary("Best run", ShortClock((unsigned long long)lines.bestTotal)));
		if (lines.sumOfBest >= 0)
			rows.push_back(Summary("Sum of best", ShortClock((unsigned long long)lines.sumOfBest)));
		if (going && lines.bestPossible >= 0)
			rows.push_back(Summary("Best possible", ShortClock((unsigned long long)lines.bestPossible)));
		// Attempts at other splits than these are not attempts at this route.
		if (times && best->attempts > 0)
			rows.push_back(Summary("Finished", std::to_string(best->finished) + " of " +
				std::to_string(best->attempts)));
	}

	Row Choice(const ::Splits::Split& split, const std::string& what) {
		Row row = { std::vector<std::string>(ChoiceColumns), false, false };
		row.cells[ChoiceName] = split.name;
		row.cells[ChoiceMarkedBy] = RouteEdit::MarkedBy(split);
		row.key = RouteKeys::kAdd + what;
		return row;
	}
}

std::vector<Row> Runs(const RunModel& model, const RunsShown& shown) {
	std::vector<Row> rows;
	if (!shown.route)
		return rows;
	const ::Splits::Route& route = *shown.route;
	const RunModel::Run* active = model.ActiveRun();

	if (active && active->route == route.id) {
		AddRun(*active, true, shown.clock == RunStore::RealTime ? model.RunRealMs(shown.now) :
			model.RunGameMs(shown.now), shown, rows);
	} else if (!route.splits.empty()) {
		std::vector<std::string> names;
		for (const ::Splits::Split& split : route.splits)
			names.push_back(split.name);
		AddRun(RunModel::Unstarted(route.id, route.name, names), false, 0, shown, rows);
	}

	// Latest first, and not the one in hand, which is at the top.
	bool headed = false;
	for (size_t i = model.Runs().size(); i-- > 0;) {
		const RunModel::Run& run = model.Runs()[i];
		if (!run.ended || run.route != route.id)
			continue;
		// Named by a row of their own, after a row left empty, and not by a
		// heading: a list with a heading in it sets every row in from the left.
		if (!headed) {
			if (!rows.empty())
				rows.push_back(Summary("", ""));
			rows.push_back(Summary(kEarlierRuns, ""));
			headed = true;
		}
		Row row = { std::vector<std::string>(SplitColumns), false, false };
		row.cells[SplitName] = Outcome(run);
		row.cells[SplitTime] = ShortClock(shown.clock == RunStore::RealTime ? run.realMs : run.gameMs);
		rows.push_back(row);
	}
	return rows;
}

std::vector<Row> RouteList(const std::vector<::Splits::Route>& routes, const std::string& running) {
	std::vector<Row> rows;
	for (const ::Splits::Route& route : routes) {
		Row row = { std::vector<std::string>(RouteColumns), false, false };
		row.cells[RouteName] = route.name;
		row.cells[RouteNote] = route.id == running ? kRunningNote : "";
		row.key = RouteKeys::kEdit + route.id;
		rows.push_back(row);
	}
	return rows;
}

std::vector<Row> RouteSplits(const ::Splits::Route& route) {
	std::vector<Row> rows;
	for (size_t i = 0; i < route.splits.size(); i++) {
		Row row = { std::vector<std::string>(RouteSplitColumns), false, false };
		row.cells[RouteSplitPlace] = std::to_string(i + 1);
		row.cells[RouteSplitName] = route.splits[i].name;
		rows.push_back(row);
	}
	return rows;
}

std::vector<Row> SplitChoices(const std::string& text) {
	std::vector<Row> rows;
	::Splits::Split level;
	if (RouteEdit::LevelSplit(text, level)) {
		rows.push_back(Heading(RouteEdit::DifficultyOf(level)));
		rows.push_back(Choice(level, "level"));
	}

	const std::string wanted = Lower(text);
	const std::vector<::Splits::Split>& catalogue = RouteEdit::Catalogue();
	int difficulty = -2;
	for (size_t i = 0; i < catalogue.size(); i++) {
		const ::Splits::Split& split = catalogue[i];
		if (!wanted.empty() && Lower(split.name).find(wanted) == std::string::npos &&
				Lower(RouteEdit::MarkedBy(split)).find(wanted) == std::string::npos &&
				Lower(RouteEdit::DifficultyOf(split)).find(wanted) == std::string::npos)
			continue;
		if (split.difficulty != difficulty) {
			difficulty = split.difficulty;
			rows.push_back(Heading(RouteEdit::DifficultyOf(split)));
		}
		rows.push_back(Choice(split, std::to_string(i)));
	}
	return rows;
}

}
