#pragma once
#include <string>
#include <vector>
#include "../../Drawing.h"
#include "RaceRows.h"
#include "SplitRoute.h"

// The race window as it opens, in two parts with a rule between them, as the
// Forge is laid out. On the left the routes there are: the one clicked is the
// one runs are timed on. On the right that route: Start and Stop over its
// splits, which are the run's while one is going on it and its times to beat
// while none is.
//
// What is done to the list of routes is written in gold under it. Changing a
// route's splits is the edit page's, which this one asks for.
class RunPage {
	public:
		explicit RunPage(Drawing::HookGroup* group);

		void SetShown(bool show);

		// On the window's draw, while this page is the one shown.
		void Draw(unsigned int width, unsigned int height);

		// The route whose splits are to be changed, once that has been asked
		// for: its id, and whether it is new and its name is to be typed. False
		// where nothing has been asked.
		bool TakeEdit(std::string& id, bool& rename);

		std::string GetStatus() const { return status; }

		// How wide the routes are, for the edit page to stand its splits in the
		// same column.
		unsigned int GetRoutesWidth() const { return routesWidth; }

		static unsigned int MinWidth();
		static unsigned int MinHeight();

	private:
		enum Action {
			ActNone,
			ActNew,
			ActCopy,
			ActEdit,
			ActDelete,
			ActRestore,
			ActCount
		};

		// What a click on an action is handed, to say which was clicked.
		struct Ask {
			RunPage* page;
			Action action;
		};

		// The routes, and what is done to the list of them.
		Drawing::Texthook* routesTitle;
		Drawing::Listhook* routeList;
		Drawing::Texthook* actions[ActCount];
		Ask asks[ActCount];
		Drawing::Boxhook* rule;

		// The route picked.
		Drawing::Texthook* routeName;
		Drawing::Buttonhook* start;
		Drawing::Buttonhook* stop;
		Drawing::Listhook* table;
		Drawing::Checkhook* autoBox;

		bool shown;
		bool deleting;			// deleting the route has been asked for once
		std::string notice;		// why what was last asked for was not done

		// The routes as they were last read.
		Splits::Route route;	// the one picked
		bool hasRoute;
		bool editable;
		size_t routeCount;
		std::string routeIds;	// every route's id, to tell when the set of them changes
		bool presetsMissing;

		// What each list was last given.
		std::vector<RaceRows::Row> routeRows;
		std::vector<RaceRows::Row> tableRows;
		int pickedRow;			// the row of routeRows the route picked is

		bool autoTicked;		// what the box says
		bool autoClicked;		// the box was clicked, and says what is wanted

		std::string status;
		int asked;				// the action clicked, until it is done
		unsigned long long refreshedAt;

		bool wantsEdit;
		bool wantsRename;

		// How the parts were last laid out, and what for.
		bool laidOut;
		unsigned int laidOutWidth;
		unsigned int laidOutHeight;
		bool laidOutRestore;
		unsigned int routesWidth;
		unsigned int nameWidth;

		Drawing::Texthook* AddText(Drawing::HookGroup* group, TextColor color, const char* text);
		void AddAction(Drawing::HookGroup* group, Action action, const char* text);
		static bool OnAction(bool up, Drawing::Hook* hook, void* ask);
		static bool OnStart(bool up, Drawing::Hook* hook, void* page);
		static bool OnStop(bool up, Drawing::Hook* hook, void* page);
		static bool OnAuto(bool up, Drawing::Hook* hook, void* page);

		void TakePicks();
		void TakeAction();
		void TakeAuto();
		void Changed();

		void Refresh();
		void MeasureRoutes();
		void Layout(unsigned int width, unsigned int height);
		void UpdateTexts();
		void KeepPicks();
};
