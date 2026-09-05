#include "AccountClaim.h"
#include "../../StringUtil.h"
#include <windows.h>

namespace {

// Named for the account rather than for the file, since what is being said is
// that this account is signed in somewhere. Lowercased first, because the realm
// tells account names apart without regard to case and so must this.
std::string ClaimNameFor(const std::string& accountName) {
	unsigned int hash = 2166136261u;
	std::string key = ToLower(Trim(accountName));
	for (unsigned int i = 0; i < key.size(); i++) {
		hash ^= (unsigned char)key[i];
		hash *= 16777619u;
	}

	char named[64];
	sprintf_s(named, "Local\\BH_account_%08x", hash);
	return std::string(named);
}

}  // namespace

AccountClaim::~AccountClaim() {
	Drop();
}

bool AccountClaim::Take(const std::string& accountName) {
	Drop();
	if (Trim(accountName).empty())
		return false;

	HANDLE claim = CreateMutexA(NULL, FALSE, ClaimNameFor(accountName).c_str());
	if (!claim)
		return false;

	// Asked for without waiting: another client holding it is the answer, not
	// something to wait for.
	DWORD waited = WaitForSingleObject(claim, 0);
	// Abandoned means the client that held it died without letting go, which
	// makes the account free and the claim ours.
	if (waited != WAIT_OBJECT_0 && waited != WAIT_ABANDONED) {
		CloseHandle(claim);
		return false;
	}

	held = claim;
	taken = true;
	name = Trim(accountName);
	return true;
}

void AccountClaim::Drop() {
	if (held) {
		if (taken)
			ReleaseMutex((HANDLE)held);
		CloseHandle((HANDLE)held);
	}
	held = NULL;
	taken = false;
	name.clear();
}

bool AccountClaim::InUse(const std::string& accountName) {
	if (Trim(accountName).empty())
		return false;

	HANDLE claim = CreateMutexA(NULL, FALSE, ClaimNameFor(accountName).c_str());
	if (!claim)
		return false;

	DWORD waited = WaitForSingleObject(claim, 0);
	bool free = (waited == WAIT_OBJECT_0 || waited == WAIT_ABANDONED);
	// Asking must not claim it. Whatever the ask took is given straight back.
	if (free)
		ReleaseMutex(claim);
	CloseHandle(claim);
	return !free;
}
