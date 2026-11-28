#include "RaceHudLayout.h"
#include <cctype>
#include "../Tracking/PlayFormat.h"

using namespace HudBlock;

namespace RaceHudLayout {

namespace {
	const char* const kNames[ViewCount] = { "Line", "Splits" };

	// How many splits are shown at once, the one a run is on among them.
	const size_t kSplitsShown = 8;
	const size_t kSplitsBefore = 3;

	std::string Lower(std::string text) {
		for (char& c : text)
			c = (char)std::tolower((unsigned char)c);
		return text;
	}

	std::string Time(long long ms) {
		return ms >= 0 ? PlayFormat::ShortClock((unsigned long long)ms) : std::string();
	}

	// How a split stands against the time to beat: its best stretch ever, or
	// ahead, or behind. Nothing where there is no time to stand against.
	Cell Standing(const RunCompare::Line& line) {
		if (!line.compared)
			return Dim("");
		const Tone tone = line.gold ? ToneBest : (line.delta <= 0 ? ToneAhead : ToneBehind);
		return { PlayFormat::Delta(line.delta), tone };
	}

	// The split the run is on, or what there is to say where it is on none.
	std::string OnSplit(const Figures& figures) {
		if (figures.run.current >= 0)
			return figures.run.splits[figures.run.current].name;
		return figures.runEnded ? "Finished" : "Done";
	}

	// The split last reached, or NULL.
	const RunCompare::Line* LastReached(const RunCompare::Lines& run) {
		const RunCompare::Line* last = NULL;
		for (const RunCompare::Line& line : run.splits) {
			if (line.time >= 0 && (!last || line.time >= last->time))
				last = &line;
		}
		return last;
	}

	Section SplitRows(const Figures& figures) {
		const std::vector<RunCompare::Line>& splits = figures.run.splits;
		const size_t on = figures.run.current >= 0 ? (size_t)figures.run.current : splits.size();
		size_t first = on > kSplitsBefore ? on - kSplitsBefore : 0;
		if (first + kSplitsShown > splits.size())
			first = splits.size() > kSplitsShown ? splits.size() - kSplitsShown : 0;

		Section rows;
		for (size_t i = first; i < splits.size() && i < first + kSplitsShown; i++) {
			const RunCompare::Line& line = splits[i];
			Row row;
			row.push_back((int)i == figures.run.current ? Cell{ line.name, ToneValue, true } :
				Label(line.name));
			if (line.state == RunModel::Split::Reached)
				row.push_back(Value(Time(line.time)));
			else if (line.state == RunModel::Split::Skipped)
				row.push_back(Dim("-"));
			else
				row.push_back(Dim(Time(line.best)));
			row.push_back(Standing(line));
			rows.push_back(row);
		}

		// The run's clock under the times of its splits, in their column, and
		// the best run's under that.
		rows.push_back({ Label("Time"), Value(Time(figures.run.clock)), Dim("") });
		if (figures.run.bestTotal >= 0)
			rows.push_back({ Label("Best"), Dim(Time(figures.run.bestTotal)), Dim("") });
		return rows;
	}
}

const char* NameOf(View view) {
	return view < ViewCount ? kNames[view] : "";
}

View ViewNamed(const std::string& name) {
	const std::string wanted = Lower(name);
	for (int view = 0; view < ViewCount; view++) {
		if (wanted == Lower(kNames[view]))
			return (View)view;
	}
	return ViewCount;
}

View Next(View view, int step) {
	const int count = ViewCount;
	return (View)((((int)view + step) % count + count) % count);
}

Figures FiguresOfRun(const RunModel::Run& run, unsigned long long clockNow,
		const RunStore::Best* best, RunStore::Clock clock) {
	Figures figures;
	figures.inRun = true;
	figures.runEnded = run.ended;
	figures.route = run.name;
	figures.run = RunCompare::Compare(run, best, clock, clockNow);
	return figures;
}

Figures FiguresOf(const RunModel& model, unsigned long long now, const RunStore::Best* best,
		RunStore::Clock clock) {
	Figures none;
	none.inRun = false;
	none.runEnded = false;
	none.run = RunCompare::Lines();
	if (model.Runs().empty())
		return none;

	const RunModel::Run& run = model.Runs().back();
	if (run.ended && (!run.finished || now > run.startedAt + run.realMs + kRunLingersMs))
		return none;

	unsigned long long clockNow;
	if (run.ended)
		clockNow = clock == RunStore::RealTime ? run.realMs : run.gameMs;
	else
		clockNow = clock == RunStore::RealTime ? model.RunRealMs(now) : model.RunGameMs(now);
	return FiguresOfRun(run, clockNow, best, clock);
}

std::vector<Section> Sections(View view, const Figures& figures) {
	std::vector<Section> sections;
	if (!figures.inRun)
		return sections;

	if (view == ViewLine) {
		Row row;
		row.push_back(Label(OnSplit(figures)));
		row.push_back(Value(Time(figures.run.clock)));
		const RunCompare::Line* last = LastReached(figures.run);
		if (last && last->compared)
			row.push_back(Standing(*last));
		sections.push_back({ row });
		return sections;
	}

	if (!figures.route.empty())
		sections.push_back({ { Label(figures.route) } });
	sections.push_back(SplitRows(figures));
	return sections;
}

}
