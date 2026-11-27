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
					a[i].cells != b[i].cells)
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

	// Rows are pushed only when they differ: pushing them refits every cell.
	if (!Same(rows, pushed)) {
		list->SetRows(ListRows(rows));
		pushed = rows;
	}
}

void SessionPanel::OnOpen() {
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
