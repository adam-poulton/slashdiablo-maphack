#include "doctest.h"
#include <set>
#include <string>
#include <vector>
#include "Modules/Accounts/AccountRows.h"

/*
 * The lines the accounts panel draws, in the order it draws them.
 *
 * Which accounts are in use is handed in rather than asked of Windows, so the
 * order, the headings and the marks can all be checked without a claim, a client
 * or a screen.
 */

namespace {

// Stands in for the claims other clients hold.
std::function<bool(const std::string&)> InUse(const std::set<std::string>& names) {
	return [names](const std::string& account) {
		return names.find(account) != names.end();
	};
}

const std::function<bool(const std::string&)> kNoneInUse = InUse(std::set<std::string>());

std::string Labels(const std::vector<AccountRow>& rows) {
	std::string said;
	for (unsigned int i = 0; i < rows.size(); i++) {
		if (i)
			said += "|";
		if (rows[i].heading)
			said += "[" + rows[i].label + "]";
		else
			said += rows[i].label;
	}
	return said;
}

}  // namespace

TEST_CASE("nothing kept is nothing drawn") {
	AccountStore store;
	CHECK(BuildAccountRows(store, kNoneInUse).empty());
}

TEST_CASE("a few accounts and nothing to tell apart are drawn without headings") {
	AccountStore store;
	store.Save("main", "p");
	store.Save("mule", "p");

	std::vector<AccountRow> rows = BuildAccountRows(store, kNoneInUse);
	CHECK(Labels(rows) == "main|mule");
	CHECK(rows[0].account == "main");
	CHECK_FALSE(rows[0].heading);
}

TEST_CASE("one roster on its own is still nothing to tell apart") {
	AccountStore store;
	store.Save("main", "p");
	store.Save("mule", "p");
	store.SetRoster("main", "hardcore");
	store.SetRoster("mule", "hardcore");

	CHECK(Labels(BuildAccountRows(store, kNoneInUse)) == "main|mule");
}

TEST_CASE("the favourites are drawn above the rosters") {
	AccountStore store;
	store.Save("main", "p");
	store.Save("mule", "p");
	store.SetRoster("main", "hardcore");
	store.SetRoster("mule", "softcore");
	store.SetFavourite("main", true);

	CHECK(Labels(BuildAccountRows(store, kNoneInUse)) ==
		"[Favourites]|main|[softcore]|mule");
}

TEST_CASE("the rosters are drawn in the order the store names them") {
	AccountStore store;
	store.Save("one", "p");
	store.Save("two", "p");
	store.Save("three", "p");
	store.SetRoster("one", "softcore");
	store.SetRoster("two", "Hardcore");
	store.SetRoster("three", "mules");

	CHECK(Labels(BuildAccountRows(store, kNoneInUse)) ==
		"[Hardcore]|two|[mules]|three|[softcore]|one");
}

TEST_CASE("what is kept in no roster is drawn last, under a heading of its own") {
	AccountStore store;
	store.Save("kept", "p");
	store.Save("loose", "p");
	store.SetRoster("kept", "hardcore");

	CHECK(Labels(BuildAccountRows(store, kNoneInUse)) ==
		"[hardcore]|kept|[No label]|loose");
}

TEST_CASE("a favourite and nothing else needs no heading") {
	AccountStore store;
	store.Save("main", "p");
	store.SetFavourite("main", true);

	std::vector<AccountRow> rows = BuildAccountRows(store, kNoneInUse);
	CHECK(Labels(rows) == "main");
	CHECK(rows[0].favourite);
}

TEST_CASE("an account another client is signed in as is marked as in use") {
	AccountStore store;
	store.Save("main", "p");
	store.Save("mule", "p");

	std::set<std::string> claimed;
	claimed.insert("mule");
	std::vector<AccountRow> rows = BuildAccountRows(store, InUse(claimed));
	REQUIRE(rows.size() == 2);
	CHECK_FALSE(rows[0].inUse);
	CHECK(rows[1].inUse);
}

TEST_CASE("a heading is never in use") {
	AccountStore store;
	store.Save("main", "p");
	store.Save("mule", "p");
	store.SetRoster("main", "hardcore");

	std::set<std::string> claimed;
	claimed.insert("hardcore");
	claimed.insert("No label");
	std::vector<AccountRow> rows = BuildAccountRows(store, InUse(claimed));
	for (unsigned int i = 0; i < rows.size(); i++) {
		if (rows[i].heading)
			CHECK_FALSE(rows[i].inUse);
	}
}

TEST_CASE("a heading stands for no account") {
	AccountStore store;
	store.Save("main", "p");
	store.Save("mule", "p");
	store.SetRoster("main", "hardcore");

	std::vector<AccountRow> rows = BuildAccountRows(store, kNoneInUse);
	for (unsigned int i = 0; i < rows.size(); i++)
		CHECK(rows[i].heading == rows[i].account.empty());
}
