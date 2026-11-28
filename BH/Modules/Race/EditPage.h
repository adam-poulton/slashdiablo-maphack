#pragma once
#include <string>
#include <vector>
#include "../../Drawing.h"
#include "RaceRows.h"
#include "SplitRoute.h"

// The race window while a route is being changed, in two parts with a rule
// between them. On the left the route: its name in a box to type over, its
// splits in the order they are run in, and under them what is done to the one
// picked out. On the right the splits there are to add, under a box that
// narrows them and is where a level to add is typed.
//
// Every change is written as it is made. Done goes back to the run page.
class EditPage {
	public:
		explicit EditPage(Drawing::HookGroup* group);

		void SetShown(bool show);

		// Starts changing a route, by its id. `rename` puts the caret in its
		// name with all of it selected, as for a route just made.
		void Begin(const std::string& id, bool rename);

		// Keeps what is being typed and lets go of the keyboard, as the page is
		// left or the window goes.
		void Leave();

		// On the window's draw, while this page is the one shown. `splitsWidth`
		// is how wide the route's splits stand.
		void Draw(unsigned int width, unsigned int height, unsigned int splitsWidth);

		// True once, when the page is done with: Done was clicked, or the route
		// is no longer there.
		bool TakeDone();

		bool OnKey(bool up, BYTE key);

		std::string GetStatus() const { return status; }

	private:
		enum Action {
			ActNone,
			ActUp,
			ActDown,
			ActRemove,
			ActCount
		};

		// What a click on an action is handed, to say which was clicked.
		struct Ask {
			EditPage* page;
			Action action;
		};

		// The route: its name, its splits, and what is done to the one picked.
		Drawing::Inputhook* nameBox;
		Drawing::Listhook* splitList;
		Drawing::Boxhook* rule;

		// The splits there are to add, and the way back.
		Drawing::Texthook* addTitle;
		Drawing::Buttonhook* doneButton;
		Drawing::Inputhook* findBox;
		Drawing::Listhook* choiceList;

		Drawing::Texthook* actions[ActCount];
		Ask asks[ActCount];

		std::string editing;	// the id of the route
		int split;				// the split of it picked out, or -1
		std::string notice;		// why what was last asked for was not done
		bool done;
		bool doneClicked;

		// The route as it was last read.
		Splits::Route route;
		bool hasRoute;
		bool editable;

		// What each list was last given.
		std::vector<RaceRows::Row> splitRows;
		std::vector<RaceRows::Row> choiceRows;
		std::string find;		// what the box over the splits to add holds
		std::string choicesFor;	// the text choiceRows were made for
		bool choicesMade;
		bool foldOwed;			// the difficulties after the first have not been folded yet

		std::string status;
		int asked;				// the action clicked, until it is done
		unsigned long long refreshedAt;

		bool nameWasFocused;
		bool nameWanted;		// the caret goes to the name once it is the route's
		bool cancelName;		// escape was pressed in the name box
		bool escapeHeld;

		// How the parts were last laid out.
		bool laidOut;
		unsigned int laidOutWidth;
		unsigned int laidOutHeight;
		unsigned int laidOutSplits;
		unsigned int addWidth;

		void AddAction(Drawing::HookGroup* group, Action action, const char* text);
		static bool OnAction(bool up, Drawing::Hook* hook, void* ask);
		static bool OnDone(bool up, Drawing::Hook* hook, void* page);

		void TakeName();
		void CommitName();
		void DropName();
		void TakeFind();
		void TakePicks();
		void AddChoice(const RaceRows::Row& row);
		void TakeAction();
		void Changed();

		void Refresh();
		void RefreshChoices();
		void Layout(unsigned int width, unsigned int height, unsigned int splitsWidth);
		void UpdateTexts();
		void KeepPicks();
};
