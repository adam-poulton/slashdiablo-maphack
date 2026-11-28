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

		// A row of the list was clicked.
		virtual void OnRowClicked(const SessionRows::Row& row) {}

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
