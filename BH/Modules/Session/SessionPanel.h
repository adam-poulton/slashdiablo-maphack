#pragma once
#include <string>
#include <vector>
#include "../Window/UIPanel.h"
#include "Session.h"
#include "SessionRows.h"

// A tab of the session window: a list of rows. What the rows are, and where
// they are read from, is each tab's to say. Keeping the list the size of the
// tab, reading the rows again as time passes, pushing them only when they have
// changed and saying which was clicked is the same for all of them, and is here.
class SessionListPanel : public UIPanel {
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
		SessionListPanel(std::string name, Drawing::UI* ui,
			const std::vector<Drawing::ListColumn>& columns);

		// The rows to list, and what the footer says while this tab is in front.
		virtual void ReadRows(std::vector<SessionRows::Row>& rows, std::string& status) = 0;

		// A row of the list was clicked, with the left button or the right.
		virtual void OnRowClicked(const SessionRows::Row& row) {}
		virtual void OnRowRightClicked(const SessionRows::Row& row) {}

		// Whether the tab puts the search text to a use of its own. Where it
		// does not, the rows listed are those that have the text in them.
		virtual bool UsesSearchItself() { return false; }

		const std::string& SearchText() const { return search; }

	public:
		void OnDraw();
		void OnOpen();
		void Search(const std::string& text);
		std::string GetStatus() { return status; }
};

// A tab that lists something of a session: the one being played, or one that is
// over and is being looked through.
class SessionPanel : public SessionListPanel {
	protected:
		SessionPanel(std::string name, Drawing::UI* ui,
			const std::vector<Drawing::ListColumn>& columns);

		void ReadRows(std::vector<SessionRows::Row>& rows, std::string& status);

		virtual std::vector<SessionRows::Row> BuildRows(const Session::View& view) = 0;

		// What the footer says of the session while this tab is in front.
		virtual std::string BuildStatus(const Session::View& view) {
			return std::string();
		}
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

// The sessions there are logs of. Clicking one puts it up in the other tabs,
// and clicking the session being played goes back to it.
class HistoryPanel : public SessionListPanel {
	protected:
		void ReadRows(std::vector<SessionRows::Row>& rows, std::string& status);
		void OnRowClicked(const SessionRows::Row& row);

	public:
		explicit HistoryPanel(Drawing::UI* ui);
		std::string GetSearchPlaceholder() { return "Search sessions by character or date"; }
};

// The routes there are, and the changing of them: a route is picked to change,
// a split of it is picked out, and a split clicked in the list of those there
// are is added after it. The search box narrows that list, and is where a name
// for the route or a level to add is typed.
class RoutesPanel : public SessionListPanel {
	private:
		std::string editing;	// the id of the route being changed
		int split;				// the split of it picked out, or -1
		bool deleting;			// deleting the route has been asked for once

		void Act(const std::string& what);

	protected:
		void ReadRows(std::vector<SessionRows::Row>& rows, std::string& status);
		void OnRowClicked(const SessionRows::Row& row);
		void OnRowRightClicked(const SessionRows::Row& row);
		bool UsesSearchItself() { return true; }

	public:
		explicit RoutesPanel(Drawing::UI* ui);
		std::string GetSearchPlaceholder() { return "Find a split, or type a route name or level 24"; }
		void OnClose();
};