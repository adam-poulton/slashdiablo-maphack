#pragma once
#include <functional>
#include <string>
#include <vector>
#include "AccountStore.h"

// One line of the accounts panel: an account to sign in as, or a heading over
// the accounts that follow it.
struct AccountRow {
	std::string label;		// what the line says
	std::string account;	// the account it stands for, empty on a heading
	bool heading;
	bool inUse;				// claimed by a client, heading rows never are
	bool favourite;

	AccountRow() : heading(false), inUse(false), favourite(false) {}
};

// Every line the panel draws, in the order it draws them: the favourites, then
// each roster under its own heading, then whatever is kept in no roster.
//
// A heading is only drawn where there is more than one section to tell apart. A
// player keeping four accounts and no rosters is shown four accounts, not a
// heading they never asked for.
//
// Which accounts are in use is asked rather than looked up, so that what the
// panel draws can be worked out without a claim, a client or a screen.
std::vector<AccountRow> BuildAccountRows(const AccountStore& store,
		const std::function<bool(const std::string&)>& inUse);
