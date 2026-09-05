#pragma once
#include <string>
#include "../Window/WindowModule.h"
#include "AccountClaim.h"
#include "AccountFile.h"
#include "AccountPanel.h"
#include "AccountStore.h"

// What the window's own visibility is held under. Never read from or written to
// the config: the window is not summoned by a hotkey, it appears with the login
// screen, and LoadConfig is overridden so this does not land in the file as a
// binding nobody set.
#define ACCOUNTS_TOGGLE_NAME	"Accounts Window"

// What the player switches the feature on and off by, in the lobby settings.
#define ACCOUNTS_SETTING_KEY	"Accounts Panel"

// The accounts BH keeps, and the panel that signs in with them.
//
// The window is drawn on the login screen and nowhere else, and it appears there
// on its own rather than waiting to be asked for: a panel that has to be summoned
// on a screen whose whole point was one click would not be worth having.
//
// This is the only part of the feature that knows there is a game. The store
// knows the accounts, the file knows the disk, the claim knows what other clients
// hold and the panel knows how to draw; what is left here is the game's own
// controls, and answering the panel out of the other three.
class AccountsWindow : public WindowModule, public AccountActions {
	private:
		AccountFile* file;
		AccountPanel* panel;

		// What the file said when it was last read: a copy to draw from, and
		// never the thing that is saved. AccountFile's header says why.
		AccountStore accounts;

		// Held while this client is past the login screen as an account, so that
		// another client can see the account is in use.
		AccountClaim claim;

		bool onLoginScreen;		// as of the last frame drawn

		// Whether the player wants the panel at all. Read every frame the login
		// screen is drawn, so switching it off in a game takes hold by the time
		// they are back out of one.
		bool showPanel;

		void Reread();

	public:
		AccountsWindow() : WindowModule("Accounts", ACCOUNTS_TOGGLE_NAME, "None"),
			file(NULL), panel(NULL), onLoginScreen(false), showPanel(true) {};
		~AccountsWindow();

		void OnLoad();
		void LoadConfig();

		// The panel is drawn out of a game, the login screen being there.
		void OnOOGDraw();

		// Deliberately nothing. The base's loop collapses a window whose toggle
		// is off, and collapsing is written to UI.ini: for a window that is off
		// for the whole of every game, that would record this panel as collapsed
		// the first time the player got into one and it would never open again.
		// What the loop does for other windows, OnOOGDraw does for this one.
		void OnLoop() {};

		// What the panel is given to work with.
		const AccountStore& Accounts() { return accounts; };
		bool InUse(const std::string& accountName);
		bool SignIn(const Account& account);
		std::string TypedAccountName();
		unsigned int TypedPasswordLength();
		bool SaveTyped();
		bool SetRoster(const std::string& accountName, const std::string& roster);
		bool SetFavourite(const std::string& accountName, bool favourite);
		bool Forget(const std::string& accountName);
};
