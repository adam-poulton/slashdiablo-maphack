#include "doctest.h"
#include <windows.h>
#include <cctype>
#include <functional>
#include <sstream>
#include <string>
#include <thread>
#include "Modules/Accounts/AccountClaim.h"

/*
 * What a client holds on an account while it is signed in as it.
 *
 * Claims are taken per thread by the lock underneath, so another thread stands
 * in for another client throughout. Each test claims a name of its own, since a
 * claim outlives nothing but the process and two tests sharing a name would
 * answer for each other.
 */

namespace {

std::string UniqueName(const char* named) {
	std::stringstream built;
	built << named << "_" << GetCurrentProcessId();
	return built.str();
}

// Runs one thing on another thread and waits for it, another thread being what
// stands in for another client.
void OnAnotherThread(const std::function<void()>& what) {
	std::thread running(what);
	running.join();
}

}  // namespace

TEST_CASE("an account nothing has claimed is not in use") {
	CHECK_FALSE(AccountClaim::InUse(UniqueName("unclaimed")));
}

TEST_CASE("a claim is held once taken") {
	std::string name = UniqueName("claimed");
	AccountClaim claim;
	REQUIRE(claim.Take(name));
	CHECK(claim.Held());
	CHECK(claim.Name() == name);
}

TEST_CASE("an account with no name is claimed by nothing") {
	AccountClaim claim;
	CHECK_FALSE(claim.Take(""));
	CHECK_FALSE(claim.Take("   "));
	CHECK_FALSE(claim.Held());
	CHECK_FALSE(AccountClaim::InUse("  "));
}

TEST_CASE("asking whether an account is in use does not claim it") {
	std::string name = UniqueName("asked");
	CHECK_FALSE(AccountClaim::InUse(name));
	CHECK_FALSE(AccountClaim::InUse(name));

	// Still there to be claimed, which it would not be had asking taken it.
	bool takenElsewhere = false;
	OnAnotherThread([&]() {
		AccountClaim claim;
		takenElsewhere = claim.Take(name);
	});
	CHECK(takenElsewhere);
}

TEST_CASE("an account another client has claimed is in use") {
	std::string name = UniqueName("elsewhere");
	AccountClaim claim;
	REQUIRE(claim.Take(name));

	bool seen = false;
	bool takenTwice = true;
	OnAnotherThread([&]() {
		seen = AccountClaim::InUse(name);
		AccountClaim other;
		takenTwice = other.Take(name);
	});

	CHECK(seen);
	// And cannot be claimed twice at once.
	CHECK_FALSE(takenTwice);
}

TEST_CASE("an account is told from another without regard to case") {
	std::string name = UniqueName("Cased");
	AccountClaim claim;
	REQUIRE(claim.Take(name));

	bool seen = false;
	OnAnotherThread([&]() {
		std::string shouted = name;
		for (unsigned int i = 0; i < shouted.size(); i++)
			shouted[i] = (char)toupper((unsigned char)shouted[i]);
		seen = AccountClaim::InUse(shouted);
	});
	CHECK(seen);
}

TEST_CASE("letting a claim go frees the account") {
	std::string name = UniqueName("released");
	AccountClaim claim;
	REQUIRE(claim.Take(name));
	claim.Drop();

	CHECK_FALSE(claim.Held());
	bool seen = true;
	OnAnotherThread([&]() { seen = AccountClaim::InUse(name); });
	CHECK_FALSE(seen);
}

TEST_CASE("claiming another account lets the first one go") {
	std::string first = UniqueName("first");
	std::string second = UniqueName("second");
	AccountClaim claim;
	REQUIRE(claim.Take(first));
	REQUIRE(claim.Take(second));

	CHECK(claim.Name() == second);
	bool firstSeen = true;
	bool secondSeen = false;
	OnAnotherThread([&]() {
		firstSeen = AccountClaim::InUse(first);
		secondSeen = AccountClaim::InUse(second);
	});
	CHECK_FALSE(firstSeen);
	CHECK(secondSeen);
}

TEST_CASE("a claim whose client died frees the account") {
	std::string name = UniqueName("abandoned");

	// Deliberately never let go of: the claim outlives the thread that took it,
	// which is what a client killed while signed in leaves behind. The handle is
	// leaked on purpose, so that the lock is held by a thread that no longer
	// exists rather than simply released.
	OnAnotherThread([&]() {
		AccountClaim* orphan = new AccountClaim();
		REQUIRE(orphan->Take(name));
	});

	// Abandoned rather than held, so the player can sign in again.
	CHECK_FALSE(AccountClaim::InUse(name));
	AccountClaim mine;
	CHECK(mine.Take(name));
}
