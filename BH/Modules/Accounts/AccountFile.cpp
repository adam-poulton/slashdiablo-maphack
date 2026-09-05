#include "AccountFile.h"
#include "../../StringUtil.h"
#include <windows.h>
#include <cctype>
#include <fstream>
#include <sstream>

namespace {

// How long an instance waits for whichever one is writing. Bounded rather than
// infinite because Modify is called from the client's own thread: a wedged
// instance holding the lock must cost a save that says it failed, not a client
// that stops drawing.
const DWORD kLockWaitMs = 5000;

// The lock is named for the file, so two installs keeping their own accounts do
// not hold each other up, and every client of one install takes the same lock
// however it spells the path's case. Local to the session, which is where the
// clients of one player are.
std::string LockNameFor(const std::string& path) {
	unsigned int hash = 2166136261u;
	std::string key = ToLower(path);
	for (unsigned int i = 0; i < key.size(); i++) {
		hash ^= (unsigned char)key[i];
		hash *= 16777619u;
	}

	char named[64];
	sprintf_s(named, "Local\\BH_accounts_%08x", hash);
	return std::string(named);
}

// Holds the named lock for as long as it is in scope, so that no path out of
// Modify can leave it held.
class ScopedLock {
	private:
		HANDLE held;
		bool taken;

	public:
		explicit ScopedLock(const std::string& name) : held(NULL), taken(false) {
			held = CreateMutexA(NULL, FALSE, name.c_str());
			if (!held)
				return;
			DWORD waited = WaitForSingleObject(held, kLockWaitMs);
			// A lock whose holder died without releasing it is abandoned rather
			// than lost, and is ours: a client killed mid-write must not leave
			// every other client unable to save.
			taken = (waited == WAIT_OBJECT_0 || waited == WAIT_ABANDONED);
		}

		~ScopedLock() {
			if (held) {
				if (taken)
					ReleaseMutex(held);
				CloseHandle(held);
			}
		}

		bool Taken() const { return taken; }
};

// Whether the file holds nothing but the whitespace something else left in it.
// Asked here rather than with Trim, which strips spaces and tabs only, so a file
// holding one newline would read as text and then fail to parse.
bool NothingButSpace(const std::string& text) {
	for (unsigned int i = 0; i < text.size(); i++) {
		if (!isspace((unsigned char)text[i]))
			return false;
	}
	return true;
}

bool ReadWholeFile(const std::string& path, std::string& into) {
	std::ifstream file(path.c_str(), std::ios::binary);
	if (!file.is_open())
		return false;
	std::stringstream held;
	held << file.rdbuf();
	into = held.str();
	return true;
}

// Written beside the file and moved over it, so that a client dying mid-write
// leaves the accounts it had rather than half of them.
bool WriteWholeFile(const std::string& path, const std::string& text) {
	std::string beside = path + ".tmp";
	{
		std::ofstream file(beside.c_str(), std::ios::binary | std::ios::trunc);
		if (!file.is_open())
			return false;
		file << text;
		if (!file.good())
			return false;
	}

	if (!MoveFileExA(beside.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING)) {
		DeleteFileA(beside.c_str());
		return false;
	}
	return true;
}

}  // namespace

AccountFile::AccountFile(const std::string& path) :
		path(path), lockName(LockNameFor(path)) {
}

// Reading takes no lock. A save is a whole file moved over the old one, so a
// reader sees one file or the other and never half of either, and the lock is
// only ever waited on by something about to write.
bool AccountFile::Read(AccountStore& into) const {
	std::string text;
	if (!ReadWholeFile(path, text)) {
		// Nothing kept yet, which is every player before their first save.
		into = AccountStore();
		return true;
	}

	// A file of nothing but whitespace is one something else truncated. Read as
	// nothing kept rather than refused, since there is nothing in it to lose.
	if (NothingButSpace(text)) {
		into = AccountStore();
		return true;
	}

	return into.FromJson(text);
}

bool AccountFile::Modify(const std::function<bool(AccountStore&)>& change) {
	ScopedLock lock(lockName);
	if (!lock.Taken())
		return false;

	AccountStore held;
	std::string text;
	if (ReadWholeFile(path, text) && !NothingButSpace(text)) {
		// A file that cannot be read is not written over. What is in it is a
		// player's accounts, and replacing them with one change made against
		// nothing is the loss this whole class exists to prevent.
		if (!held.FromJson(text))
			return false;
	}

	if (!change(held))
		return false;

	return WriteWholeFile(path, held.ToJson());
}
