#include "AccountFile.h"
#include "../../WholeFile.h"

// What the lock every client takes before writing the file is named for.
static const char* const kLockTag = "accounts";

AccountFile::AccountFile(const std::string& path) : path(path) {
}

// Reading takes no lock. A save is a whole file moved over the old one, so a
// reader sees one file or the other and never half of either, and the lock is
// only ever waited on by something about to write.
bool AccountFile::Read(AccountStore& into) const {
	std::string text;
	if (!WholeFile::Read(path, text)) {
		// Nothing kept yet, which is every player before their first save.
		into = AccountStore();
		return true;
	}

	// A file of nothing but whitespace is one something else truncated. Read as
	// nothing kept rather than refused, since there is nothing in it to lose.
	if (WholeFile::IsBlank(text)) {
		into = AccountStore();
		return true;
	}

	return into.FromJson(text);
}

bool AccountFile::Modify(const std::function<bool(AccountStore&)>& change) {
	WholeFile::Lock lock(kLockTag, path);
	if (!lock.Taken())
		return false;

	AccountStore held;
	std::string text;
	if (WholeFile::Read(path, text) && !WholeFile::IsBlank(text)) {
		// A file that cannot be read is not written over. What is in it is a
		// player's accounts, and replacing them with one change made against
		// nothing is the loss this whole class exists to prevent.
		if (!held.FromJson(text))
			return false;
	}

	if (!change(held))
		return false;

	return WholeFile::Write(path, held.ToJson());
}
