#include "HudLayout.h"
#include <algorithm>
#include <cctype>
#include "SessionFormat.h"

namespace HudLayout {

namespace {
	const char* const kNames[ViewCount] = { "Line", "Compact", "Splits", "Full" };

	// How far back the HUD's "recent" rate looks.
	const unsigned long long kRecentMs = 60 * 1000;

	// How many splits are shown at once, the one a run is on among them.
	const size_t kSplitsShown = 8;
	const size_t kSplitsBefore = 3;

	Cell Label(const std::string& text) {
		return { text, ToneLabel };
	}

	Cell Value(const std::string& text) {
		return { text, ToneValue };
	}

	Cell Dim(const std::string& text) {
		return { text, ToneDim };
	}

	bool IsLeft(const Cell& cell) {
		return cell.tone == ToneLabel || cell.left;
	}

	std::string Lower(std::string text) {
		for (char& c : text)
			c = (char)std::tolower((unsigned char)c);
		return text;
	}

	std::string Time(long long ms) {
		return ms >= 0 ? SessionFormat::ShortClock((unsigned long long)ms) : std::string();
	}

	// How a split stands against the time to beat: its best stretch ever, or
	// ahead, or behind. Nothing where there is no time to stand against.
	Cell Standing(const RunCompare::Line& line) {
		if (!line.compared)
			return Dim("");
		const Tone tone = line.gold ? ToneBest : (line.delta <= 0 ? ToneAhead : ToneBehind);
		return { SessionFormat::Delta(line.delta), tone };
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

	Section SessionSection(const Figures& figures) {
		using namespace SessionFormat;
		return {
			{ Label("Session"), Value(ShortClock(figures.sessionMs)),
				Label("Played"), Value(ShortClock(figures.playedMs)) },
			{ Label("Game " + std::to_string(figures.games)), Value(ShortClock(figures.gameMs)),
				Label("XP"), Value(ShortRate(figures.gameRate)) },
			{ Label("Session XP"), Value(ShortRate(figures.playedRate)),
				Label("Recent"), Value(ShortRate(figures.recentRate)) },
		};
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
			if (line.state == SessionModel::RunSplit::Reached)
				row.push_back(Value(Time(line.time)));
			else if (line.state == SessionModel::RunSplit::Skipped)
				row.push_back(Dim("-"));
			else
				row.push_back(Dim(Time(line.best)));
			row.push_back(Standing(line));
			rows.push_back(row);
		}
		return rows;
	}

	// The run's clock under the times of its splits, in their column, and the
	// best run's under that.
	void AddClock(const Figures& figures, Section& rows) {
		rows.push_back({ Label("Time"), Value(Time(figures.run.clock)), Dim("") });
		if (figures.run.bestTotal >= 0)
			rows.push_back({ Label("Best"), Dim(Time(figures.run.bestTotal)), Dim("") });
	}

	Section Tallies(const Figures& figures) {
		return { { Label("Kills"), Value(SessionFormat::Count(figures.kills)),
			Label("Deaths"), Value(std::to_string(figures.deaths)) } };
	}
	int Along(Side side, int offset, int canvas, int block) {
		int at = offset;
		if (side == SideMiddle)
			at = (canvas - block) / 2 + offset;
		else if (side == SideFar)
			at = canvas - block - offset;
		return std::max(0, std::min(at, canvas - block));
	}

	void PinAlong(int at, int block, int canvas, Side& side, int& offset) {
		const int middle = at + block / 2;
		if (middle * 3 < canvas) {
			side = SideNear;
			offset = at;
		} else if (middle * 3 > canvas * 2) {
			side = SideFar;
			offset = canvas - block - at;
		} else {
			side = SideMiddle;
			offset = at - (canvas - block) / 2;
		}
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

Figures FiguresOf(const SessionModel& model, unsigned long long now,
		const RunStore::Best* best, RunStore::Clock clock) {
	Figures figures;
	figures.sessionMs = now;
	figures.playedMs = model.PlayedMs(now);
	figures.gameMs = model.GameMs(now);
	figures.games = (unsigned int)model.Games().size();
	figures.gameRate = model.GameRate(now);
	figures.playedRate = model.PlayedRate(now);
	figures.recentRate = model.RecentRate(now, kRecentMs);
	figures.kills = model.KillTotals().Total();
	figures.deaths = model.Deaths();
	if (!model.Drops().empty())
		figures.lastDrop = model.Drops().back().item.name;

	figures.inRun = false;
	figures.runEnded = false;
	figures.run = RunCompare::Lines();
	if (model.Runs().empty())
		return figures;

	const SessionModel::Run& run = model.Runs().back();
	if (run.ended && (!run.finished || now > run.startedAt + run.realMs + kRunLingersMs))
		return figures;

	unsigned long long clockNow;
	if (run.ended)
		clockNow = clock == RunStore::RealTime ? run.realMs : run.gameMs;
	else
		clockNow = clock == RunStore::RealTime ? model.RunRealMs(now) : model.RunGameMs(now);
	figures.inRun = true;
	figures.runEnded = run.ended;
	figures.run = RunCompare::Compare(run, best, clock, clockNow);
	return figures;
}

std::vector<Section> Sections(View view, const Figures& figures) {
	using namespace SessionFormat;
	std::vector<Section> sections;

	if (view == ViewLine) {
		Row row;
		if (figures.inRun) {
			row.push_back(Label(OnSplit(figures)));
			row.push_back(Value(Time(figures.run.clock)));
			const RunCompare::Line* last = LastReached(figures.run);
			if (last && last->compared)
				row.push_back(Standing(*last));
		} else {
			row.push_back(Value(ShortClock(figures.sessionMs)));
			row.push_back(Label("Game " + std::to_string(figures.games)));
			row.push_back(Value(ShortClock(figures.gameMs)));
		}
		row.push_back(Label("XP"));
		row.push_back(Value(ShortRate(figures.playedRate)));
		sections.push_back({ row });
		return sections;
	}

	if (figures.inRun && (view == ViewSplits || view == ViewFull)) {
		Section splits = SplitRows(figures);
		AddClock(figures, splits);
		sections.push_back(splits);
		if (view == ViewSplits)
			return sections;
	}

	Section session = SessionSection(figures);
	if (figures.inRun && view == ViewCompact) {
		const RunCompare::Line* last = LastReached(figures.run);
		session.push_back({ Label(OnSplit(figures)), Value(Time(figures.run.clock)),
			Label("Last"), last && last->compared ? Standing(*last) : Dim("") });
	}
	sections.push_back(session);

	if (view == ViewFull) {
		sections.push_back(Tallies(figures));
		if (!figures.lastDrop.empty())
			sections.push_back({ { Label("Drop"), Cell{ figures.lastDrop, ToneValue, true } } });
	}
	return sections;
}

namespace {
	// Sets one section's rows out from a corner, and says how far right they
	// came. `y` is left under the last of them.
	int ArrangeFrom(const Section& rows, const Metrics& metrics, int& y, std::vector<Placed>& cells) {
		std::vector<int> columns;
		std::vector<std::vector<int>> widths;
		for (const Row& row : rows) {
			widths.push_back(std::vector<int>());
			for (size_t c = 0; c < row.size(); c++) {
				const int width = metrics.width(row[c].text);
				widths.back().push_back(width);
				if (c >= columns.size())
					columns.push_back(0);
				columns[c] = std::max(columns[c], width);
			}
		}

		int right = metrics.padding;
		for (size_t r = 0; r < rows.size(); r++) {
			int x = metrics.padding;
			for (size_t c = 0; c < rows[r].size(); c++) {
				const Cell& cell = rows[r][c];
				// A gap is what the cell before it calls for: a name keeps its
				// figure close, and a figure stands off from whatever comes next.
				if (c > 0)
					x += IsLeft(rows[r][c - 1]) ? metrics.labelGap : metrics.groupGap;
				const int inset = IsLeft(cell) ? 0 : columns[c] - widths[r][c];
				cells.push_back({ { x + inset, y }, cell.text, cell.tone });
				x += columns[c];
			}
			right = std::max(right, x);
			y += metrics.textHeight;
			if (r + 1 < rows.size())
				y += metrics.rowGap;
		}
		return right;
	}
}

Block Arrange(const std::vector<Row>& rows, const Metrics& metrics) {
	return ArrangeSections({ rows }, metrics);
}

Block ArrangeSections(const std::vector<Section>& sections, const Metrics& metrics) {
	Block block;
	int right = metrics.padding;
	int y = metrics.padding;
	bool any = false;
	for (const Section& section : sections) {
		if (section.empty())
			continue;
		if (any)
			y += metrics.rowGap + metrics.sectionGap;
		right = std::max(right, ArrangeFrom(section, metrics, y, block.cells));
		any = true;
	}
	block.size = { right + metrics.padding, y + metrics.padding };
	return block;
}
Point Resolve(const Place& place, Size canvas, Size block) {
	return { Along(place.across, place.x, canvas.width, block.width),
		Along(place.down, place.y, canvas.height, block.height) };
}

Place PlaceOf(Point topLeft, Size block, Size canvas) {
	Place place;
	PinAlong(topLeft.x, block.width, canvas.width, place.across, place.x);
	PinAlong(topLeft.y, block.height, canvas.height, place.down, place.y);
	return place;
}

bool UnderPanel(Point topLeft, Size block, Size canvas, bool leftOpen, bool rightOpen) {
	const int middle = canvas.width / 2;
	return (leftOpen && topLeft.x < middle) ||
		(rightOpen && topLeft.x + block.width > middle);
}

}
