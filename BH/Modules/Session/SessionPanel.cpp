#include "SessionPanel.h"
#include <ctime>
#include "../../Common.h"
#include "../ScreenInfo/ScreenInfo.h"
#include "Session.h"
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
					a[i].cells != b[i].cells || a[i].colors != b[i].colors)
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

	std::string TimeOfDay(long long epochSeconds) {
		if (epochSeconds <= 0)
			return std::string();
		const time_t moment = (time_t)epochSeconds;
		std::tm local = {};
		localtime_s(&local, &moment);
		char text[16];
		strftime(text, sizeof(text), "%H:%M", &local);
		return text;
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

SessionPanel::SessionPanel(std::string name, UI* ui, const std::vector<ListColumn>& columns) :
		UIPanel(name, ui), refreshedAt(0) {
	list = new Listhook(tab, UI_CONTENT_MARGIN, 0, 0, 0);
	list->SetColumns(columns);
	ApplyLayout();
}

void SessionPanel::ApplyLayout() {
	laidOutWidth = tab->GetXSize();
	laidOutHeight = tab->GetYSize();
	list->SetSize(laidOutWidth > 2 * UI_CONTENT_MARGIN ? laidOutWidth - 2 * UI_CONTENT_MARGIN : 0,
		laidOutHeight);
}

void SessionPanel::Refresh() {
	std::vector<SessionRows::Row> rows;
	const bool read = Session::Read([&](const SessionModel& model, unsigned long long now) {
		rows = BuildRows(model, now);
		status = BuildStatus(model, now);
	});
	if (!read) {
		status = Session::IsEnabled() ? "No session yet" :
			"The session tracker is off";
	}
	if (!search.empty() && !rows.empty()) {
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

void SessionPanel::OnOpen() {
	refreshedAt = 0;
}

void SessionPanel::Search(const std::string& text) {
	if (text == search)
		return;
	search = text;
	list->SetScrollTop(0);
	refreshedAt = 0;
}

void SessionPanel::OnDraw() {
	if (!IsActive())
		return;
	if (tab->GetXSize() != laidOutWidth || tab->GetYSize() != laidOutHeight)
		ApplyLayout();

	const unsigned long long tick = BHGetTickCount();
	if (refreshedAt == 0 || tick - refreshedAt >= kRefreshEveryMs) {
		refreshedAt = tick;
		Refresh();
	}
}

OverviewPanel::OverviewPanel(UI* ui) : SessionPanel("Overview", ui, OverviewColumns()) {}

std::vector<SessionRows::Row> OverviewPanel::BuildRows(const SessionModel& model,
		unsigned long long now) {
	SessionRows::Level level = { 0, 0 };
	if (!model.Games().empty()) {
		const unsigned int reached = model.Games().back().level;
		if (reached >= 1 && reached < kLastLevel)
			level = { ExpByLevel[reached - 1], ExpByLevel[reached] };
	}
	return SessionRows::Overview(model, now, level, TimeOfDay(model.Header().startedAt));
}

GamesPanel::GamesPanel(UI* ui) : SessionPanel("Games", ui, GamesColumns()) {}

std::vector<SessionRows::Row> GamesPanel::BuildRows(const SessionModel& model,
		unsigned long long now) {
	return SessionRows::Games(model, now);
}

std::string GamesPanel::BuildStatus(const SessionModel& model, unsigned long long now) {
	const size_t games = model.Games().size();
	if (games == 0)
		return "No games yet";
	return std::to_string(games) + (games == 1 ? " game, " : " games, ") +
		SessionFormat::ShortClock(model.PlayedMs(now)) + " played";
}

KillsPanel::KillsPanel(UI* ui) : SessionPanel("Kills", ui, KillsColumns()) {}

std::vector<SessionRows::Row> KillsPanel::BuildRows(const SessionModel& model,
		unsigned long long now) {
	return SessionRows::Kills(model, now);
}

std::string KillsPanel::BuildStatus(const SessionModel& model, unsigned long long now) {
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

std::vector<SessionRows::Row> DropsPanel::BuildRows(const SessionModel& model,
		unsigned long long now) {
	return SessionRows::Drops(model);
}

std::string DropsPanel::BuildStatus(const SessionModel& model, unsigned long long now) {
	const size_t drops = model.Drops().size();
	if (drops == 0)
		return "No drops yet";
	return std::to_string(drops) + (drops == 1 ? " drop" : " drops");
}