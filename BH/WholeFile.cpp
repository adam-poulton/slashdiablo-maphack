#include "WholeFile.h"
#include "StringUtil.h"
#include <cctype>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace WholeFile {
	namespace {
		const DWORD kLockWaitMs = 5000;

		// Local to the session, which is where the clients of one player are.
		std::string LockName(const std::string& tag, const std::string& path) {
			unsigned int hash = 2166136261u;
			std::string key = ToLower(path);
			for (unsigned int i = 0; i < key.size(); i++) {
				hash ^= (unsigned char)key[i];
				hash *= 16777619u;
			}

			char named[96];
			sprintf_s(named, "Local\\BH_%s_%08x", tag.c_str(), hash);
			return std::string(named);
		}
	}

	bool Read(const std::string& path, std::string& into) {
		std::ifstream file(path.c_str(), std::ios::binary);
		if (!file.is_open())
			return false;
		std::stringstream held;
		held << file.rdbuf();
		into = held.str();
		return true;
	}

	bool Write(const std::string& path, const std::string& text) {
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

	// Asked here rather than with Trim, which strips spaces and tabs only, so a
	// file holding one newline would read as text.
	bool IsBlank(const std::string& text) {
		for (unsigned int i = 0; i < text.size(); i++) {
			if (!isspace((unsigned char)text[i]))
				return false;
		}
		return true;
	}

	Lock::Lock(const std::string& tag, const std::string& path) :
			held(NULL), taken(false) {
		held = CreateMutexA(NULL, FALSE, LockName(tag, path).c_str());
		if (!held)
			return;
		DWORD waited = WaitForSingleObject(held, kLockWaitMs);
		// A lock whose holder died without releasing it is abandoned rather than
		// lost, and is ours: a client killed mid-write must not leave every other
		// client unable to save.
		taken = (waited == WAIT_OBJECT_0 || waited == WAIT_ABANDONED);
	}

	Lock::~Lock() {
		if (held) {
			if (taken)
				ReleaseMutex(held);
			CloseHandle(held);
		}
	}
}
