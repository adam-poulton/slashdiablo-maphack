#include "AccountRows.h"

namespace {

// What the favourites and the accounts in no roster are headed with. A roster
// heads itself with its own name.
//
// A roster is called a label everywhere the player reads it. The project's word
// for what accounts are kept in is roster, and the code keeps it, but a player
// putting one account under "mules" is labelling it rather than filling in a
// roster. CONTEXT.md records the two words and which is which.
const char* const kFavouritesHeading = "Favourites";
const char* const kNoRosterHeading = "No label";

// One section: what heads it, and what is kept in it.
struct Section {
	std::string heading;
	std::vector<Account> accounts;
};

void AddAccounts(std::vector<AccountRow>& rows, const std::vector<Account>& accounts,
		const std::function<bool(const std::string&)>& inUse) {
	for (unsigned int i = 0; i < accounts.size(); i++) {
		AccountRow row;
		row.label = accounts[i].name;
		row.account = accounts[i].name;
		row.favourite = accounts[i].favourite;
		row.inUse = inUse ? inUse(accounts[i].name) : false;
		rows.push_back(row);
	}
}

}  // namespace

std::vector<AccountRow> BuildAccountRows(const AccountStore& store,
		const std::function<bool(const std::string&)>& inUse) {
	std::vector<Section> sections;

	std::vector<Account> favourites = store.Favourites();
	if (!favourites.empty()) {
		Section section;
		section.heading = kFavouritesHeading;
		section.accounts = favourites;
		sections.push_back(section);
	}

	std::vector<std::string> rosters = store.Rosters();
	for (unsigned int i = 0; i < rosters.size(); i++) {
		Section section;
		section.heading = rosters[i];
		section.accounts = store.InRoster(rosters[i]);
		sections.push_back(section);
	}

	std::vector<Account> loose = store.InRoster("");
	if (!loose.empty()) {
		Section section;
		section.heading = kNoRosterHeading;
		section.accounts = loose;
		sections.push_back(section);
	}

	// Nothing to tell apart, so nothing to head it with.
	bool headed = sections.size() > 1;

	std::vector<AccountRow> rows;
	for (unsigned int i = 0; i < sections.size(); i++) {
		if (headed) {
			AccountRow heading;
			heading.label = sections[i].heading;
			heading.heading = true;
			rows.push_back(heading);
		}
		AddAccounts(rows, sections[i].accounts, inUse);
	}
	return rows;
}
