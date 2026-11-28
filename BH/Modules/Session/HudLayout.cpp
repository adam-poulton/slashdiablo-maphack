#include "HudLayout.h"
#include <cctype>
#include "../Tracking/PlayFormat.h"

using namespace HudBlock;

namespace HudLayout {

namespace {
	const char* const kNames[ViewCount] = { "Line", "Compact", "Full" };

	// How far back the HUD's "recent" rate looks.
	const unsigned long long kRecentMs = 60 * 1000;

	std::string Lower(std::string text) {
		for (char& c : text)
			c = (char)std::tolower((unsigned char)c);
		return text;
	}

	Section SessionSection(const Figures& figures) {
		using namespace PlayFormat;
		return {
			{ Label("Session"), Value(ShortClock(figures.sessionMs)),
				Label("Played"), Value(ShortClock(figures.playedMs)) },
			{ Label("Game " + std::to_string(figures.games)), Value(ShortClock(figures.gameMs)),
				Label("XP"), Value(ShortRate(figures.gameRate)) },
			{ Label("Session XP"), Value(ShortRate(figures.playedRate)),
				Label("Recent"), Value(ShortRate(figures.recentRate)) },
		};
	}

	Section Tallies(const Figures& figures) {
		return { { Label("Kills"), Value(PlayFormat::Count(figures.kills)),
			Label("Deaths"), Value(std::to_string(figures.deaths)) } };
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
	figures.kills = model.KillTotals().Total();
	figures.deaths = model.Deaths();
	if (!model.Drops().empty())
		figures.lastDrop = model.Drops().back().item.name;
	return figures;
}

std::vector<Section> Sections(View view, const Figures& figures) {
	using namespace PlayFormat;
	std::vector<Section> sections;

	if (view == ViewLine) {
		sections.push_back({ {
			Value(ShortClock(figures.sessionMs)),
			Label("Game " + std::to_string(figures.games)),
			Value(ShortClock(figures.gameMs)),
			Label("XP"),
			Value(ShortRate(figures.playedRate)),
		} });
		return sections;
	}

	sections.push_back(SessionSection(figures));
	if (view == ViewFull) {
		sections.push_back(Tallies(figures));
		if (!figures.lastDrop.empty())
			sections.push_back({ { Label("Drop"), Cell{ figures.lastDrop, ToneValue, true } } });
	}
	return sections;
}

}
