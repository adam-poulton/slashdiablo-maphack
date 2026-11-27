#include "HudLayout.h"
#include <algorithm>
#include <cctype>
#include "SessionFormat.h"

namespace HudLayout {

namespace {
	const char* const kNames[ViewCount] = { "Line", "Compact" };

	// How far back the HUD's "recent" rate looks.
	const unsigned long long kRecentMs = 60 * 1000;

	Cell Label(const std::string& text) {
		return { text, ToneLabel };
	}

	Cell Value(const std::string& text) {
		return { text, ToneValue };
	}

	std::string Lower(std::string text) {
		for (char& c : text)
			c = (char)std::tolower((unsigned char)c);
		return text;
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

Figures FiguresOf(const SessionModel& model, unsigned long long now) {
	Figures figures;
	figures.sessionMs = now;
	figures.playedMs = model.PlayedMs(now);
	figures.gameMs = model.GameMs(now);
	figures.games = (unsigned int)model.Games().size();
	figures.gameRate = model.GameRate(now);
	figures.playedRate = model.PlayedRate(now);
	figures.recentRate = model.RecentRate(now, kRecentMs);
	return figures;
}

std::vector<Row> Rows(View view, const Figures& figures) {
	using namespace SessionFormat;
	const std::string game = "Game " + std::to_string(figures.games);

	if (view == ViewLine) {
		return { {
			Value(ShortClock(figures.sessionMs)),
			Label(game), Value(ShortClock(figures.gameMs)),
			Label("XP"), Value(ShortRate(figures.playedRate)),
		} };
	}

	return {
		{ Label("Session"), Value(ShortClock(figures.sessionMs)),
			Label("Played"), Value(ShortClock(figures.playedMs)) },
		{ Label(game), Value(ShortClock(figures.gameMs)),
			Label("XP"), Value(ShortRate(figures.gameRate)) },
		{ Label("Session XP"), Value(ShortRate(figures.playedRate)),
			Label("Recent"), Value(ShortRate(figures.recentRate)) },
	};
}

Block Arrange(const std::vector<Row>& rows, const Metrics& metrics) {
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

	Block block;
	int right = metrics.padding;
	int y = metrics.padding;
	for (size_t r = 0; r < rows.size(); r++) {
		int x = metrics.padding;
		for (size_t c = 0; c < rows[r].size(); c++) {
			const Cell& cell = rows[r][c];
			// A gap is what the cell before it calls for: a name keeps its figure
			// close, and a figure stands off from whatever comes next.
			if (c > 0)
				x += rows[r][c - 1].tone == ToneLabel ? metrics.labelGap : metrics.groupGap;
			const int inset = cell.tone == ToneValue ? columns[c] - widths[r][c] : 0;
			block.cells.push_back({ { x + inset, y }, cell.text, cell.tone });
			x += columns[c];
		}
		right = std::max(right, x);
		y += metrics.textHeight;
		if (r + 1 < rows.size())
			y += metrics.rowGap;
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
