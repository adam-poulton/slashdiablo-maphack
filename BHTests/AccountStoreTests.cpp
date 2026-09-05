#include "doctest.h"
#include <string>
#include "Modules/Accounts/AccountStore.h"

/*
 * The accounts BH keeps, and the rules about them.
 *
 * The store knows nothing about the file it will live in and nothing about the
 * screen it will be drawn on, so every rule about what is kept, what is told
 * apart from what, and what is listed where can be checked without a client.
 */

namespace {

// The names below are account names rather than player names, which is what the
// login box takes.
const char* const kName = "grumpyadam";
const char* const kPassword = "hunter2";

}  // namespace

TEST_CASE("an account that was kept is found") {
	AccountStore store;
	REQUIRE(store.Save(kName, kPassword));

	const Account* found = store.Find(kName);
	REQUIRE(found != NULL);
	CHECK(found->name == kName);
	CHECK(found->password == kPassword);
	CHECK(found->roster == "");
	CHECK(found->favourite == false);
	CHECK(store.Count() == 1);
}

TEST_CASE("nothing is found under a name never kept") {
	AccountStore store;
	CHECK(store.Find(kName) == NULL);
	CHECK(store.Count() == 0);
}

TEST_CASE("a name is told apart without regard to case") {
	AccountStore store;
	store.Save("GrumpyAdam", kPassword);

	const Account* found = store.Find("grumpyadam");
	REQUIRE(found != NULL);
	// Kept as it was typed, whatever it is asked for as.
	CHECK(found->name == "GrumpyAdam");
}

TEST_CASE("one account cannot be kept twice under two spellings of itself") {
	AccountStore store;
	store.Save("GrumpyAdam", "first");
	store.Save("grumpyadam", "second");

	CHECK(store.Count() == 1);
	REQUIRE(store.Find("GRUMPYADAM") != NULL);
	CHECK(store.Find("GRUMPYADAM")->password == "second");
}

TEST_CASE("retyping a password keeps the roster and the favourite mark") {
	AccountStore store;
	store.Save(kName, "first");
	store.SetRoster(kName, "hardcore");
	store.SetFavourite(kName, true);

	REQUIRE(store.Save(kName, "second"));

	const Account* found = store.Find(kName);
	REQUIRE(found != NULL);
	CHECK(found->password == "second");
	CHECK(found->roster == "hardcore");
	CHECK(found->favourite == true);
}

TEST_CASE("an account with nothing to sign in with is refused") {
	AccountStore store;
	CHECK_FALSE(store.Save("", kPassword));
	CHECK_FALSE(store.Save("   ", kPassword));
	CHECK_FALSE(store.Save(kName, ""));
	CHECK_FALSE(store.Save(kName, "\t "));
	CHECK(store.Count() == 0);
}

TEST_CASE("what was typed around a name is not part of it") {
	AccountStore store;
	store.Save("  grumpyadam  ", "  hunter2  ");

	const Account* found = store.Find("grumpyadam");
	REQUIRE(found != NULL);
	CHECK(found->name == "grumpyadam");
	CHECK(found->password == "hunter2");
}

TEST_CASE("forgetting says whether there was anything to forget") {
	AccountStore store;
	store.Save(kName, kPassword);

	CHECK(store.Forget("GRUMPYADAM"));
	CHECK(store.Count() == 0);
	CHECK_FALSE(store.Forget(kName));
}

TEST_CASE("a roster is named by putting an account in it") {
	AccountStore store;
	store.Save("mule", kPassword);
	REQUIRE(store.SetRoster("mule", "softcore"));

	std::vector<std::string> rosters = store.Rosters();
	REQUIRE(rosters.size() == 1);
	CHECK(rosters[0] == "softcore");
}

TEST_CASE("a roster is named once however many accounts are kept in it") {
	AccountStore store;
	store.Save("mule", kPassword);
	store.Save("main", kPassword);
	store.SetRoster("mule", "softcore");
	store.SetRoster("main", "Softcore");

	CHECK(store.Rosters().size() == 1);
	CHECK(store.InRoster("SOFTCORE").size() == 2);
}

TEST_CASE("the rosters are named alphabetically without regard to case") {
	AccountStore store;
	store.Save("one", kPassword);
	store.Save("two", kPassword);
	store.Save("three", kPassword);
	store.SetRoster("one", "softcore");
	store.SetRoster("two", "Hardcore");
	store.SetRoster("three", "mules");

	std::vector<std::string> rosters = store.Rosters();
	REQUIRE(rosters.size() == 3);
	CHECK(rosters[0] == "Hardcore");
	CHECK(rosters[1] == "mules");
	CHECK(rosters[2] == "softcore");
}

TEST_CASE("accounts are listed alphabetically without regard to case") {
	AccountStore store;
	store.Save("zeal", kPassword);
	store.Save("Amazon", kPassword);
	store.Save("barb", kPassword);

	std::vector<Account> listed = store.InRoster("");
	REQUIRE(listed.size() == 3);
	CHECK(listed[0].name == "Amazon");
	CHECK(listed[1].name == "barb");
	CHECK(listed[2].name == "zeal");
}

TEST_CASE("an account in no roster is asked for with a blank name") {
	AccountStore store;
	store.Save("loose", kPassword);
	store.Save("kept", kPassword);
	store.SetRoster("kept", "softcore");

	std::vector<Account> ungrouped = store.InRoster("");
	REQUIRE(ungrouped.size() == 1);
	CHECK(ungrouped[0].name == "loose");
	CHECK(store.Rosters().size() == 1);
}

TEST_CASE("a favourite is kept above the rosters rather than inside one") {
	AccountStore store;
	store.Save("main", kPassword);
	store.SetRoster("main", "hardcore");
	REQUIRE(store.SetFavourite("main", true));

	std::vector<Account> favourites = store.Favourites();
	REQUIRE(favourites.size() == 1);
	CHECK(favourites[0].name == "main");
	// Not drawn twice: the roster it belongs to does not list it.
	CHECK(store.InRoster("hardcore").empty());
	// And nothing else is kept there, so the roster heads no list at all.
	CHECK(store.Rosters().empty());
}

TEST_CASE("dropping the favourite mark puts the account back in its roster") {
	AccountStore store;
	store.Save("main", kPassword);
	store.SetRoster("main", "hardcore");
	store.SetFavourite("main", true);

	REQUIRE(store.SetFavourite("main", false));

	CHECK(store.Favourites().empty());
	std::vector<Account> listed = store.InRoster("hardcore");
	REQUIRE(listed.size() == 1);
	CHECK(listed[0].name == "main");
}

TEST_CASE("the favourites are listed alphabetically without regard to case") {
	AccountStore store;
	store.Save("zeal", kPassword);
	store.Save("Amazon", kPassword);
	store.SetFavourite("zeal", true);
	store.SetFavourite("Amazon", true);

	std::vector<Account> favourites = store.Favourites();
	REQUIRE(favourites.size() == 2);
	CHECK(favourites[0].name == "Amazon");
	CHECK(favourites[1].name == "zeal");
}

TEST_CASE("a roster still lists what is not a favourite") {
	AccountStore store;
	store.Save("main", kPassword);
	store.Save("mule", kPassword);
	store.SetRoster("main", "hardcore");
	store.SetRoster("mule", "hardcore");
	store.SetFavourite("main", true);

	std::vector<std::string> rosters = store.Rosters();
	REQUIRE(rosters.size() == 1);
	CHECK(rosters[0] == "hardcore");
	std::vector<Account> listed = store.InRoster("hardcore");
	REQUIRE(listed.size() == 1);
	CHECK(listed[0].name == "mule");
}

TEST_CASE("what is asked of an account never kept is refused") {
	AccountStore store;
	CHECK_FALSE(store.SetRoster(kName, "hardcore"));
	CHECK_FALSE(store.SetFavourite(kName, true));
}

TEST_CASE("what was typed around a roster name is not part of it") {
	AccountStore store;
	store.Save("mule", kPassword);
	store.SetRoster("mule", "  softcore  ");

	std::vector<std::string> rosters = store.Rosters();
	REQUIRE(rosters.size() == 1);
	CHECK(rosters[0] == "softcore");
	CHECK(store.InRoster("softcore").size() == 1);
}

TEST_CASE("a roster is unnamed by taking the last account out of it") {
	AccountStore store;
	store.Save("mule", kPassword);
	store.SetRoster("mule", "softcore");
	REQUIRE(store.Rosters().size() == 1);

	store.SetRoster("mule", "");

	CHECK(store.Rosters().empty());
	CHECK(store.InRoster("").size() == 1);
}

/*
 * The store as the text of the file it is kept in.
 *
 * Every rule about a file a player has managed to corrupt is here rather than in
 * the file class, because none of it needs a disk.
 */

TEST_CASE("what was kept comes back out of the text it was written as") {
	AccountStore written;
	written.Save("main", "hunter2");
	written.SetRoster("main", "hardcore");
	written.SetFavourite("main", true);
	written.Save("mule", "other");
	written.SetRoster("mule", "softcore");

	AccountStore read;
	REQUIRE(read.FromJson(written.ToJson()));

	CHECK(read.Count() == 2);
	const Account* main = read.Find("main");
	REQUIRE(main != NULL);
	CHECK(main->password == "hunter2");
	CHECK(main->roster == "hardcore");
	CHECK(main->favourite == true);
	const Account* mule = read.Find("mule");
	REQUIRE(mule != NULL);
	CHECK(mule->roster == "softcore");
	CHECK(mule->favourite == false);
}

TEST_CASE("the text says which shape it is") {
	AccountStore store;
	store.Save("main", "hunter2");
	CHECK(store.ToJson().find("\"version\"") != std::string::npos);
}

TEST_CASE("a store with nothing in it is still a file") {
	AccountStore written;
	AccountStore read;
	read.Save("stale", "stale");

	REQUIRE(read.FromJson(written.ToJson()));
	CHECK(read.Count() == 0);
}

TEST_CASE("text that is not the file's shape is refused and loses nothing") {
	AccountStore store;
	store.Save("main", "hunter2");

	CHECK_FALSE(store.FromJson("{\"accounts\": ["));
	CHECK_FALSE(store.FromJson("not json at all"));
	CHECK_FALSE(store.FromJson("[]"));
	CHECK_FALSE(store.FromJson("{\"accounts\": 4}"));

	// Exactly what it held before each of those was refused.
	CHECK(store.Count() == 1);
	REQUIRE(store.Find("main") != NULL);
	CHECK(store.Find("main")->password == "hunter2");
}

TEST_CASE("a file listing nothing is nothing kept") {
	AccountStore store;
	store.Save("stale", "stale");
	REQUIRE(store.FromJson("{\"version\": 1}"));
	CHECK(store.Count() == 0);
}

TEST_CASE("one unusable line is not worth the others") {
	const char* const text =
		"{\"version\": 1, \"accounts\": ["
		"{\"name\": \"\", \"password\": \"orphan\"},"
		"{\"name\": \"nameless\"},"
		"{\"password\": \"passwordless\"},"
		"\"a line that is not an account at all\","
		"{\"name\": \"main\", \"password\": \"hunter2\"}"
		"]}";

	AccountStore store;
	REQUIRE(store.FromJson(text));
	CHECK(store.Count() == 1);
	REQUIRE(store.Find("main") != NULL);
	CHECK(store.Find("main")->password == "hunter2");
}

TEST_CASE("a member of the wrong type is nothing said rather than a throw") {
	const char* const text =
		"{\"version\": 1, \"accounts\": ["
		"{\"name\": 42, \"password\": \"hunter2\"},"
		"{\"name\": \"main\", \"password\": \"hunter2\", \"roster\": 7, \"favourite\": \"yes\"}"
		"]}";

	AccountStore store;
	REQUIRE(store.FromJson(text));
	CHECK(store.Count() == 1);
	const Account* main = store.Find("main");
	REQUIRE(main != NULL);
	CHECK(main->roster == "");
	CHECK(main->favourite == false);
}

TEST_CASE("a name the file gives twice is one account") {
	const char* const text =
		"{\"version\": 1, \"accounts\": ["
		"{\"name\": \"main\", \"password\": \"first\", \"roster\": \"hardcore\"},"
		"{\"name\": \"MAIN\", \"password\": \"second\"}"
		"]}";

	AccountStore store;
	REQUIRE(store.FromJson(text));
	CHECK(store.Count() == 1);
	const Account* main = store.Find("main");
	REQUIRE(main != NULL);
	// A line describes a whole account, so the later one is the account: the
	// password it gives and the roster it does not. Retyping a password in the
	// panel is a different act, and keeps what it did not mention.
	CHECK(main->password == "second");
	CHECK(main->roster == "");
}

TEST_CASE("what the file does not say is not read as something") {
	AccountStore store;
	REQUIRE(store.FromJson("{\"accounts\": [{\"name\": \"main\", \"password\": \"hunter2\", \"unknown\": true}]}"));
	CHECK(store.Count() == 1);
}
