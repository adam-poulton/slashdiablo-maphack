#pragma once
#include <string>
#include <vector>
#include "../Window/UIPanel.h"
#include "Session.h"
#include "SessionRows.h"

// A tab of the session window: a list of rows read off the session in hand.
// What the rows are is each tab's to say. Reading the session, keeping the list
// the size of the tab and pushing rows only when they have changed is the same
// for all of them, and is here.
class SessionPanel : public UIPanel {
	private:
		Drawing::Listhook* list;
		std::vector<SessionRows::Row> pushed;	// what the list was last given
		std::string search;						// what the window's search box holds
		std::string status;
		unsigned long long refreshedAt;
		unsigned int laidOutWidth;
		unsigned int laidOutHeight;

		void ApplyLayout();
		void Refresh();

	protected:
		SessionPanel(std::string name, Drawing::UI* ui,
			const std::vector<Drawing::ListColumn>& columns);

		virtual std::vector<SessionRows::Row> BuildRows(const Session::View& view) = 0;

		// A row of the list was clicked.
		virtual void OnRowClicked(const SessionRows::Row& row) {}

		// What the footer says of the session while this tab is in front.
		virtual std::string BuildStatus(const Session::View& view) {
			return std::string();
		}

	public:
		void OnDraw();
		void OnOpen();
		void Search(const std::string& text);
		std::string GetStatus() { return status; }
};

// The session at a glance: a name and a figure to a row.
class OverviewPanel : public SessionPanel {
	protected:
		std::vector<SessionRows::Row> BuildRows(const Session::View& view);

	public:
		explicit OverviewPanel(Drawing::UI* ui);
		std::string GetSearchPlaceholder() { return "Search the overview"; }
};

// Every game of the session, under the kind of run it was.
class GamesPanel : public SessionPanel {
	protected:
		std::vector<SessionRows::Row> BuildRows(const Session::View& view);
		std::string BuildStatus(const Session::View& view);

	public:
		explicit GamesPanel(Drawing::UI* ui);
		std::string GetSearchPlaceholder() { return "Search games by name"; }
};

// What died, where, and of what kind.
class KillsPanel : public SessionPanel {
	protected:
		std::vector<SessionRows::Row> BuildRows(const Session::View& view);
		std::string BuildStatus(const Session::View& view);

	public:
		explicit KillsPanel(Drawing::UI* ui);
		std::string GetSearchPlaceholder() { return "Search by area or monster"; }
};

// The run being timed against the best its route has been run, the routes there
// are to run, and the runs of the session. Clicking a route chooses it.
class SplitsPanel : public SessionPanel {
	protected:
		std::vector<SessionRows::Row> BuildRows(const Session::View& view);
		std::string BuildStatus(const Session::View& view);
		void OnRowClicked(const SessionRows::Row& row);

	public:
		explicit SplitsPanel(Drawing::UI* ui);
		std::string GetSearchPlaceholder() { return "Search splits and routes"; }
};

// Every drop the item filter pinged.
class DropsPanel : public SessionPanel {
	protected:
		std::vector<SessionRows::Row> BuildRows(const Session::View& view);
		std::string BuildStatus(const Session::View& view);

	public:
		explicit DropsPanel(Drawing::UI* ui);
		std::string GetSearchPlaceholder() { return "Search drops by item or area"; }
};
