#pragma once
#include <string>
#include <vector>
#include "../../Drawing.h"
#include "../Window/UIPanel.h"
#include "AccountRows.h"
#include "AccountStore.h"

// What the accounts panel is given to work with: the accounts as they are, and
// the things it can ask to have done to them.
//
// Implemented by the module, so the panel holds nothing of the file it is kept
// in, nothing of the claims other clients hold, and nothing of the game's own
// boxes. Every one of these can fail, and each says whether it did rather than
// leaving the panel to guess.
class AccountActions {
	public:
		virtual ~AccountActions() {};

		virtual const AccountStore& Accounts() = 0;
		virtual bool InUse(const std::string& accountName) = 0;

		// Fills the game's boxes and presses. What the player asked for by
		// clicking, and never done for any other reason: ADR 0009.
		virtual bool SignIn(const Account& account) = 0;

		// What is typed into the game's own boxes at this moment. The password is
		// answered as a length and never as itself: the panel says how much of it
		// has been typed and has no use for the rest of it.
		virtual std::string TypedAccountName() = 0;
		virtual unsigned int TypedPasswordLength() = 0;
		virtual bool SaveTyped() = 0;

		virtual bool SetRoster(const std::string& accountName, const std::string& roster) = 0;
		virtual bool SetFavourite(const std::string& accountName, bool favourite) = 0;
		virtual bool Forget(const std::string& accountName) = 0;
};

// The accounts, drawn on the login screen: the favourites, then the rosters, and
// a row for each account. Clicking a row signs in as it and does nothing else,
// ever. Right clicking one takes it in hand, and the band below the list becomes
// what can be done to the account in hand.
//
// The band is the same height either way, so the list never moves under the
// cursor as the panel changes what it is offering.
class AccountPanel : public UIPanel {
	private:
		AccountActions* actions;

		Drawing::Listhook* list;
		Drawing::Boxhook* rule;

		// The band below the list, with an account in hand, in row order.
		Drawing::Inputhook* rosterBox;
		Drawing::Texthook* inHandLabel;
		Drawing::Texthook* favouriteAction;
		Drawing::Texthook* forgetAction;
		Drawing::Texthook* doneAction;

		// The same band with nothing in hand, row for row.
		Drawing::Texthook* hintLabel;
		Drawing::Texthook* captureLabel;
		Drawing::Texthook* keepAction;

		std::vector<AccountRow> rows;

		// The account the band acts on, and empty where nothing is in hand. Held
		// by name rather than by row, so that rows being pushed again for a
		// change does not move what the player was working on.
		std::string inHand;

		// Whether forgetting has been asked for once already, so that the second
		// click is the one that does it.
		bool forgetAsked;

		bool laidOut;
		bool needsRefresh;

		// Whether the groups are still owed their one collapse.
		bool foldOnPush;

		// Set where enter was heard, and acted on by the next draw. Keys arrive
		// from the window procedure rather than from the draw, and finishing an
		// edit rereads the file and relists the rows.
		bool commitRequested;

		// What the band was last told to say, so it is only told again when it
		// changed: setting text measures it, and the band is asked every frame.
		std::string drawnInHand;
		bool drawnForgetAsked;

		// Marking a favourite leaves every other compared field the same - the
		// same account in hand, as many kept - so the band needs this to notice.
		bool drawnFavourite;
		std::string drawnTyped;
		unsigned int drawnPasswordLength;
		unsigned int drawnCount;

		unsigned int laidOutWidth;
		unsigned int laidOutHeight;

		void ApplyLayout();
		void ApplyColumns();
		void PushRows();

		// Run in this order after the rows are listed: the collapse must not shut
		// the group the reveal is about to open.
		void FoldGroups();
		void RevealInHand();

		void TakeInHand(const std::string& accountName);
		void CommitAndClose();
		void UpdateBand();

		// The account one row stands for, or nothing for a heading or a row that
		// is no longer there.
		const AccountRow* RowAt(int row);

		static bool OnFavouriteClicked(bool up, Drawing::Hook* hook, void* self);
		static bool OnForgetClicked(bool up, Drawing::Hook* hook, void* self);
		static bool OnKeepClicked(bool up, Drawing::Hook* hook, void* self);
		static bool OnDoneClicked(bool up, Drawing::Hook* hook, void* self);

	public:
		AccountPanel(Drawing::UI* ui, AccountActions* actions);

		// Asks for the rows to be listed again, for a change another client made.
		// Takes effect on the next draw rather than at once, which is what lets it
		// be called from wherever the change was noticed.
		void Refresh() { needsRefresh = true; };

		// Whether an account is in hand, and the two ways of finishing with it.
		// Enter and Done keep what was typed; escape does not.
		bool IsEditing() { return !inHand.empty(); };
		void RequestCommit() { commitRequested = true; };
		void StopEditing() { TakeInHand(std::string()); };

		void OnDraw();
		void OnOpen();
		void OnClose();
		std::string GetStatus();
};
