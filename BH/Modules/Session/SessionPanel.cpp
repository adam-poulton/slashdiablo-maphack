#include "SessionPanel.h"
#include <ctime>
#include "../../Common.h"
#include "../ScreenInfo/ScreenInfo.h"
#include "SessionFormat.h"

using namespace Drawing;

namespace {
	// How often a tab reads the session again. Its fastest figure is a clock
	// showing seconds.
	const unsigned long long kRefreshEveryMs = 250;

	const unsigned int kColumnGap = 10;

	// Experience at which a level starts is ExpByLevel[level - 1], and the table
	// stops at the last level there is.
	const unsigned int kLastLevel = 99;

	bool Same(const std::vector<SessionRows::Row>& a, const std::vector<SessionRows::Row>& b) {
		if (a.size() != b.size())
			return false;
		for (size_t i = 0; i < a.size(); i++) {
			if (a[i].heading != b[i].heading || a[i].summary != b[i].summary ||
					a[i].cells != b[i].cells || a[i].colors != b[i].colors ||
					a[i].key != b[i].key)
				return false;
		}
		return true;
	}

	std::vector<ListRow> ListRows(const std::vector<SessionRows::Row>& rows) {
		std::vector<ListRow> listed;
		listed.reserve(rows.size());
		for (const SessionRows::Row& row : rows) {
			ListRow one(row.cells, row.heading);
			if (row.summary)
				one.colors.assign(row.cells.size(), Grey);
			// Disabled is the list's word for a cell that takes its column's.
			for (int color : row.colors)
				one.colors.push_back(color < 0 ? Disabled : (TextColor)color);
			listed.push_back(one);
		}
		return listed;
	}

	std::string Written(long long epochSeconds, const char* form) {
		if (epochSeconds <= 0)
			return std::string();
		const time_t moment = (time_t)epochSeconds;
		std::tm local = {};
		localtime_s(&local, &moment);
		char text[32];
		strftime(text, sizeof(text), form, &local);
		return text;
	}

	std::string TimeOfDay(long long epochSeconds) {
		return Written(epochSeconds, "%H:%M");
	}

	// A moment that may be of another day: "27 Nov 21:14".
	std::string DayAndTime(long long epochSeconds) {
		return Written(epochSeconds, "%#d %b %H:%M");
	}

	std::vector<ListColumn> HistoryColumns() {
		std::vector<ListColumn> columns(SessionRows::HistoryColumns);
		columns[SessionRows::HistoryStarted] = ListColumn("Started", 92, 0, 0, White);
		columns[SessionRows::HistoryCharacter] = ListColumn("Character", 90, 1, kColumnGap, White);
		columns[SessionRows::HistoryLevel] = ListColumn("Level", 32, 0, kColumnGap, Grey);
		columns[SessionRows::HistoryPlayed] = ListColumn("Played", 48, 0, kColumnGap, White);
		columns[SessionRows::HistoryGames] = ListColumn("Games", 38, 0, kColumnGap, White);
		columns[SessionRows::HistoryExperience] = ListColumn("Experience", 66, 0, kColumnGap, White);
		columns[SessionRows::HistoryRate] = ListColumn("Per second", 66, 0, kColumnGap, White);
		columns[SessionRows::HistoryKills] = ListColumn("Kills", 40, 0, kColumnGap, Grey);
		columns[SessionRows::HistoryDrops] = ListColumn("Drops", 36, 0, kColumnGap, Grey);
		return columns;
	}

	std::vector<ListColumn> RoutesColumns() {
		std::vector<ListColumn> columns(SessionRows::RouteColumns);
		columns[SessionRows::RouteName] = ListColumn("", 220, 1, 0, White, Gold);
		columns[SessionRows::RouteDetail] = ListColumn("", 220, 1, kColumnGap, Grey);
		return columns;
	}

	std::vector<ListColumn> OverviewColumns() {
		return {
			ListColumn("", 170, 0, 0, Grey),
			ListColumn("", 0, 1, kColumnGap, White),
		};
	}

	std::vector<ListColumn> GamesColumns() {
		std::vector<ListColumn> columns(SessionRows::GameColumns);
		columns[SessionRows::GameNumber] = ListColumn("#", 22, 0, 0, Grey);
		columns[SessionRows::GameName] = ListColumn("Game", 90, 2, kColumnGap, White);
		columns[SessionRows::GameLength] = ListColumn("Length", 50, 0, kColumnGap, White);
		columns[SessionRows::GameExperience] = ListColumn("Experience", 80, 1, kColumnGap, White);
		columns[SessionRows::GameRate] = ListColumn("Per second", 70, 0, kColumnGap, White);
		columns[SessionRows::GameLevel] = ListColumn("Level", 58, 0, kColumnGap, White);
		return columns;
	}

	std::vector<ListColumn> KillsColumns() {
		std::vector<ListColumn> columns(SessionRows::KillColumns);
		// The two columns of names share what the figures leave: an area's name
		// in the first, and in the last the area a unique monster died in.
		columns[SessionRows::KillName] = ListColumn("", 180, 1, 0, White);
		columns[SessionRows::KillTotal] = ListColumn("Kills", 36, 0, kColumnGap, White);
		columns[SessionRows::KillChampions] = ListColumn("Champion", 60, 0, kColumnGap, White);
		columns[SessionRows::KillUniques] = ListColumn("Unique", 46, 0, kColumnGap, White);
		columns[SessionRows::KillTime] = ListColumn("Time", 40, 0, kColumnGap, White);
		columns[SessionRows::KillDetail] = ListColumn("", 150, 1, kColumnGap, Grey);
		return columns;
	}

	std::vector<ListColumn> SplitsColumns() {
		std::vector<ListColumn> columns(SessionRows::SplitColumns);
		columns[SessionRows::SplitName] = ListColumn("", 200, 1, 0, White, Gold);
		columns[SessionRows::SplitTime] = ListColumn("Time", 60, 0, kColumnGap, White);
		columns[SessionRows::SplitDelta] = ListColumn("+/-", 50, 0, kColumnGap, Grey);
		columns[SessionRows::SplitBest] = ListColumn("Best run", 60, 0, kColumnGap, Grey);
		columns[SessionRows::SplitSegment] = ListColumn("Best split", 64, 0, kColumnGap, Grey);
		columns[SessionRows::SplitLevel] = ListColumn("Level", 36, 0, kColumnGap, Grey);
		return columns;
	}

	std::vector<ListColumn> DropsColumns() {
		std::vector<ListColumn> columns(SessionRows::DropColumns);
		columns[SessionRows::DropTime] = ListColumn("Time", 44, 0, 0, Grey);
		columns[SessionRows::DropItem] = ListColumn("Item", 140, 2, kColumnGap, White);
		columns[SessionRows::DropArea] = ListColumn("Area", 110, 1, kColumnGap, Grey);
		columns[SessionRows::DropGame] = ListColumn("Game", 34, 0, kColumnGap, Grey);
		columns[SessionRows::DropPing] = ListColumn("Tier", 26, 0, kColumnGap, Grey);
		return columns;
	}
}

SessionListPanel::SessionListPanel(std::string name, UI* ui,
		const std::vector<ListColumn>& columns) : UIPanel(name, ui), refreshedAt(0) {
	list = new Listhook(tab, UI_CONTENT_MARGIN, 0, 0, 0);
	list->SetColumns(columns);
	ApplyLayout();
}

void SessionListPanel::ApplyLayout() {
	laidOutWidth = tab->GetXSize();
	laidOutHeight = tab->GetYSize();
	list->SetSize(laidOutWidth > 2 * UI_CONTENT_MARGIN ? laidOutWidth - 2 * UI_CONTENT_MARGIN : 0,
		laidOutHeight);
}

void SessionListPanel::Refresh() {
	std::vector<SessionRows::Row> rows;
	status.clear();
	ReadRows(rows, status);
	if (!UsesSearchItself() && !search.empty() && !rows.empty()) {
		rows = SessionRows::Matching(rows, search);
		size_t matching = 0;
		for (const SessionRows::Row& row : rows) {
			if (!row.heading)
				matching++;
		}
		status = (matching > 0 ? std::to_string(matching) + " matching \"" : "Nothing matches \"") +
			search + "\"";
	}

	// Rows are pushed only when they differ: pushing them refits every cell.
	if (!Same(rows, pushed)) {
		list->SetRows(ListRows(rows));
		pushed = rows;
	}
}

void SessionListPanel::OnOpen() {
	refreshedAt = 0;
}

void SessionListPanel::Search(const std::string& text) {
	if (text == search)
		return;
	search = text;
	list->SetScrollTop(0);
	refreshedAt = 0;
}

void SessionListPanel::OnDraw() {
	if (!IsActive())
		return;
	if (tab->GetXSize() != laidOutWidth || tab->GetYSize() != laidOutHeight)
		ApplyLayout();

	// Taken before the rows are read again: the row clicked is one of those the
	// list was last given.
	const int clicked = list->TakeClickedRow();
	const int rightClicked = list->TakeRightClickedRow();
	if (clicked >= 0 && (size_t)clicked < pushed.size()) {
		OnRowClicked(pushed[clicked]);
		refreshedAt = 0;
	} else if (rightClicked >= 0 && (size_t)rightClicked < pushed.size()) {
		OnRowRightClicked(pushed[rightClicked]);
		refreshedAt = 0;
	}

	const unsigned long long tick = BHGetTickCount();
	if (refreshedAt == 0 || tick - refreshedAt >= kRefreshEveryMs) {
		refreshedAt = tick;
		Refresh();
	}
}

SessionPanel::SessionPanel(std::string name, UI* ui, const std::vector<ListColumn>& columns) :
		SessionListPanel(name, ui, columns) {}

void SessionPanel::ReadRows(std::vector<SessionRows::Row>& rows, std::string& status) {
	const bool read = Session::Read([&](const Session::View& view) {
		rows = BuildRows(view);
		status = BuildStatus(view);
		// A session that is over is said to be, whichever tab it is read in.
		if (!view.viewing.empty()) {
			const SessionEvents::Started& header = view.model.Header();
			const std::string whose = header.character + ", " + DayAndTime(header.startedAt);
			status = status.empty() ? whose : whose + ": " + status;
		}
	});
	if (!read) {
		status = Session::IsEnabled() ? "No session yet" :
			"The session tracker is off";
	}
}

OverviewPanel::OverviewPanel(UI* ui) : SessionPanel("Overview", ui, OverviewColumns()) {}

std::vector<SessionRows::Row> OverviewPanel::BuildRows(const Session::View& view) {
	const SessionModel& model = view.model;
	SessionRows::Level level = { 0, 0 };
	if (!model.Games().empty()) {
		const unsigned int reached = model.Games().back().level;
		if (reached >= 1 && reached < kLastLevel)
			level = { ExpByLevel[reached - 1], ExpByLevel[reached] };
	}
	const long long startedAt = model.Header().startedAt;
	return SessionRows::Overview(model, view.now, level,
		view.viewing.empty() ? TimeOfDay(startedAt) : DayAndTime(startedAt));
}

GamesPanel::GamesPanel(UI* ui) : SessionPanel("Games", ui, GamesColumns()) {}

std::vector<SessionRows::Row> GamesPanel::BuildRows(const Session::View& view) {
	return SessionRows::Games(view.model, view.now);
}

std::string GamesPanel::BuildStatus(const Session::View& view) {
	const SessionModel& model = view.model;
	const size_t games = model.Games().size();
	if (games == 0)
		return "No games yet";
	return std::to_string(games) + (games == 1 ? " game, " : " games, ") +
		SessionFormat::ShortClock(model.PlayedMs(view.now)) + " played";
}

KillsPanel::KillsPanel(UI* ui) : SessionPanel("Kills", ui, KillsColumns()) {}

std::vector<SessionRows::Row> KillsPanel::BuildRows(const Session::View& view) {
	return SessionRows::Kills(view.model, view.now);
}

std::string KillsPanel::BuildStatus(const Session::View& view) {
	const SessionModel& model = view.model;
	const unsigned int kills = model.KillTotals().Total();
	if (kills == 0)
		return "No kills yet";
	unsigned int areas = 0;
	for (const auto& area : model.Areas()) {
		if (area.second.kills.Total() > 0)
			areas++;
	}
	return SessionFormat::Count(kills) + (kills == 1 ? " kill in " : " kills in ") +
		std::to_string(areas) + (areas == 1 ? " area" : " areas");
}

DropsPanel::DropsPanel(UI* ui) : SessionPanel("Drops", ui, DropsColumns()) {}

std::vector<SessionRows::Row> DropsPanel::BuildRows(const Session::View& view) {
	return SessionRows::Drops(view.model);
}

std::string DropsPanel::BuildStatus(const Session::View& view) {
	const size_t drops = view.model.Drops().size();
	if (drops == 0)
		return "No drops yet";
	return std::to_string(drops) + (drops == 1 ? " drop" : " drops");
}

SplitsPanel::SplitsPanel(UI* ui) : SessionPanel("Splits", ui, SplitsColumns()) {}

std::vector<SessionRows::Row> SplitsPanel::BuildRows(const Session::View& view) {
	const SessionRows::RunsShown shown = { &view.routes, view.route, &view.bests, view.clock,
		view.now };
	return SessionRows::Runs(view.model, shown);
}

std::string SplitsPanel::BuildStatus(const Session::View& view) {
	const char* clock = view.clock == RunStore::RealTime ? "real time" : "time in game";
	if (view.model.ActiveRun())
		return std::string("Timed in ") + clock;
	return std::string("No run being timed. Click a route, then .run start");
}

void SplitsPanel::OnRowClicked(const SessionRows::Row& row) {
	const std::string route = SessionRows::RouteOf(row.key);
	if (!route.empty())
		Session::RequestRoute(route);
}

HistoryPanel::HistoryPanel(UI* ui) : SessionListPanel("History", ui, HistoryColumns()) {}

void HistoryPanel::ReadRows(std::vector<SessionRows::Row>& rows, std::string& status) {
	Session::ReadHistory([&](const Session::HistoryView& view) {
		rows = SessionRows::History(view.entries, { view.hasCurrent, view.viewing, DayAndTime });
		const size_t sessions = view.entries.size();
		if (sessions == 0 && view.pending == 0) {
			status = "No past sessions";
			return;
		}
		status = std::to_string(sessions) + (sessions == 1 ? " past session" : " past sessions");
		if (view.pending > 0)
			status += ", reading " + std::to_string(view.pending) + " more";
		else
			status += ". Click one to look through it";
	});
}

void HistoryPanel::OnRowClicked(const SessionRows::Row& row) {
	std::string file;
	if (!SessionRows::SessionOf(row.key, file))
		return;
	if (!Session::ViewPast(file))
		PrintText(Red, "\377c4Session:\377c1 that session's file could not be read.");
}

RoutesPanel::RoutesPanel(UI* ui) : SessionListPanel("Routes", ui, RoutesColumns()),
		split(-1), deleting(false) {}

void RoutesPanel::OnClose() {
	deleting = false;
}

void RoutesPanel::ReadRows(std::vector<SessionRows::Row>& rows, std::string& status) {
	Session::ReadRoutes([&](const Session::RoutesView& view) {
		const Splits::Route* found = NULL;
		for (const Splits::Route& route : view.routes) {
			if (route.id == editing)
				found = &route;
		}
		// The route runs are timed on is the one to start with, and to fall
		// back on when the one being changed has gone.
		if (!found) {
			editing = view.chosen;
			split = -1;
			deleting = false;
		} else if (split >= (int)found->splits.size()) {
			split = -1;
		}

		rows = SessionRows::Routes({ &view.routes, view.chosen, editing, split, SearchText(),
			deleting });
		if (!view.editable)
			status = "BH_routes.json could not be read, so routes cannot be changed here";
		else if (split >= 0)
			status = "A split clicked below is added after the one picked out";
		else
			status = "Click a split below to add it. Right click one of the route's to remove it";
	});
}

void RoutesPanel::OnRowClicked(const SessionRows::Row& row) {
	using namespace SessionRows;
	std::string rest;
	const bool confirming = deleting;
	deleting = false;

	if (KeyRest(row.key, RouteKeys::kEdit, rest)) {
		editing = rest;
		split = -1;
	} else if (KeyRest(row.key, RouteKeys::kPick, rest)) {
		const int picked = atoi(rest.c_str());
		split = split == picked ? -1 : picked;
	} else if (KeyRest(row.key, RouteKeys::kAdd, rest)) {
		Splits::Split added;
		if (rest == "level") {
			if (!RouteEdit::LevelSplit(SearchText(), added))
				return;
		} else {
			const size_t index = (size_t)atoi(rest.c_str());
			if (index >= RouteEdit::Catalogue().size())
				return;
			added = RouteEdit::Catalogue()[index];
		}
		Session::EditRoutes([&](std::vector<Splits::Route>& routes) {
			for (Splits::Route& route : routes) {
				if (route.id == editing)
					split = (int)RouteEdit::AddSplit(route, added, split);
			}
		});
	} else if (KeyRest(row.key, RouteKeys::kDo, rest)) {
		if (rest == "delete" && !confirming)
			deleting = true;
		else
			Act(rest);
	}
}

void RoutesPanel::OnRowRightClicked(const SessionRows::Row& row) {
	std::string rest;
	deleting = false;
	if (!SessionRows::KeyRest(row.key, SessionRows::RouteKeys::kPick, rest))
		return;
	const unsigned int index = (unsigned int)atoi(rest.c_str());
	Session::EditRoutes([&](std::vector<Splits::Route>& routes) {
		for (Splits::Route& route : routes) {
			if (route.id == editing)
				RouteEdit::RemoveSplit(route, index);
		}
	});
	split = -1;
}

void RoutesPanel::Act(const std::string& what) {
	if (what == "use") {
		Session::RequestRoute(editing);
		return;
	}

	const std::string text = SearchText();
	Session::EditRoutes([&](std::vector<Splits::Route>& routes) {
		Splits::Route* route = NULL;
		for (Splits::Route& each : routes) {
			if (each.id == editing)
				route = &each;
		}

		if (what == "new") {
			editing = RouteEdit::Add(routes, NULL);
			split = -1;
		} else if (what == "restore") {
			RouteEdit::Restore(routes);
		} else if (!route) {
			return;
		} else if (what == "copy") {
			const Splits::Route original = *route;
			editing = RouteEdit::Add(routes, &original);
			split = -1;
		} else if (what == "rename") {
			RouteEdit::Rename(routes, editing, text);
		} else if (what == "delete") {
			if (routes.size() > 1)
				RouteEdit::Remove(routes, editing);
			editing.clear();
			split = -1;
		} else if (split < 0) {
			return;
		} else if (what == "up" || what == "down") {
			const int by = what == "up" ? -1 : 1;
			if (RouteEdit::MoveSplit(*route, (unsigned int)split, by))
				split += by;
		} else if (what == "remove") {
			RouteEdit::RemoveSplit(*route, (unsigned int)split);
			split = -1;
		}
	});
}