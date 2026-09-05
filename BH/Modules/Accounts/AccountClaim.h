#pragma once
#include <string>

// What one client holds on an account while it is signed in as it, so that
// another client can see the account is in use before signing in as it too.
//
// Held as a lock the operating system owns rather than as a mark in the accounts
// file, because the interesting case is a client that dies. Windows lets go of
// what a dead process held, however it died; a mark written to a file outlives
// the client that wrote it, and a panel that says an account is in use when it
// is not is a panel a player stops believing.
//
// What it says is advice and never a refusal. A client that crashed leaves the
// realm believing it is still signed in, and signing in again is the player's
// way out of that, which ADR 0009 records.
class AccountClaim {
	private:
		void* held;			// HANDLE, kept untyped so the header stays clean
		bool taken;
		std::string name;

		AccountClaim(const AccountClaim&);
		AccountClaim& operator=(const AccountClaim&);

	public:
		AccountClaim() : held(NULL), taken(false) {}
		~AccountClaim();

		// Claims the account, letting go of whatever was claimed before. Answers
		// whether the claim is held, which it is not where another client holds
		// it already.
		bool Take(const std::string& accountName);

		void Drop();

		bool Held() const { return taken; }
		const std::string& Name() const { return name; }

		// Whether the account is claimed by anything other than the asking
		// thread. A claim the asking thread holds itself is not reported, since
		// the lock underneath is one a thread may take again, and that costs
		// nothing: a client on the login screen holds no claim to begin with.
		static bool InUse(const std::string& accountName);
};
