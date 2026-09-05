#include "doctest.h"
#include <windows.h>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include "Modules/Accounts/AccountFile.h"

/*
 * The file the accounts are kept in.
 *
 * A real file in a temporary directory rather than a pretend one, because what
 * is being checked is what survives a client dying, a file arriving corrupted
 * and two clients saving, none of which a stand-in for the disk would tell the
 * truth about.
 */

namespace {

// A path of its own for each test, removed with whatever was written to it.
class TempFile {
	private:
		std::string path;

	public:
		explicit TempFile(const char* named) {
			char directory[MAX_PATH];
			DWORD length = GetTempPathA(MAX_PATH, directory);
			REQUIRE(length > 0);
			std::stringstream built;
			built << std::string(directory, length) << "BH_" << named << "_"
				<< GetCurrentProcessId() << ".json";
			path = built.str();
			DeleteFileA(path.c_str());
		}

		~TempFile() {
			DeleteFileA(path.c_str());
			DeleteFileA((path + ".tmp").c_str());
		}

		const std::string& Path() const { return path; }

		void Write(const std::string& text) const {
			std::ofstream file(path.c_str(), std::ios::binary | std::ios::trunc);
			REQUIRE(file.is_open());
			file << text;
		}

		std::string Read() const {
			std::ifstream file(path.c_str(), std::ios::binary);
			if (!file.is_open())
				return std::string();
			std::stringstream held;
			held << file.rdbuf();
			return held.str();
		}

		bool Exists() const {
			return GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES;
		}
};

}  // namespace

TEST_CASE("a file that is not there is nothing kept") {
	TempFile temp("missing");
	AccountFile file(temp.Path());

	AccountStore store;
	store.Save("stale", "stale");
	CHECK(file.Read(store));
	CHECK(store.Count() == 0);
	// Reading does not create it. The first save does.
	CHECK_FALSE(temp.Exists());
}

TEST_CASE("what was saved is what the file says afterwards") {
	TempFile temp("saved");
	AccountFile file(temp.Path());

	REQUIRE(file.Modify([](AccountStore& store) {
		return store.Save("main", "hunter2");
	}));

	AccountStore read;
	REQUIRE(file.Read(read));
	REQUIRE(read.Find("main") != NULL);
	CHECK(read.Find("main")->password == "hunter2");
}

TEST_CASE("a change is applied to what the file says, not to what was last drawn") {
	TempFile temp("merged");
	AccountFile file(temp.Path());

	// One client keeps an account.
	REQUIRE(file.Modify([](AccountStore& store) { return store.Save("first", "one"); }));

	// Another client, whose own copy knows nothing of that first account, keeps
	// a second one. It must not take the first away.
	AccountFile other(temp.Path());
	REQUIRE(other.Modify([](AccountStore& store) { return store.Save("second", "two"); }));

	AccountStore read;
	REQUIRE(file.Read(read));
	CHECK(read.Count() == 2);
	CHECK(read.Find("first") != NULL);
	CHECK(read.Find("second") != NULL);
}

TEST_CASE("a change that changed nothing writes nothing") {
	TempFile temp("unchanged");
	AccountFile file(temp.Path());

	CHECK_FALSE(file.Modify([](AccountStore& store) {
		// Refused by the store: nothing to sign in with.
		return store.Save("", "");
	}));
	CHECK_FALSE(temp.Exists());
}

TEST_CASE("a file that cannot be read is not written over") {
	TempFile temp("corrupt");
	temp.Write("{\"accounts\": [");
	AccountFile file(temp.Path());

	AccountStore store;
	CHECK_FALSE(file.Read(store));

	// The one thing that must not happen: a save against nothing, replacing
	// whatever the player's file really held.
	CHECK_FALSE(file.Modify([](AccountStore& into) {
		return into.Save("main", "hunter2");
	}));
	CHECK(temp.Read() == "{\"accounts\": [");
}

TEST_CASE("an empty file is nothing kept rather than something corrupted") {
	TempFile temp("empty");
	temp.Write("   \r\n");
	AccountFile file(temp.Path());

	AccountStore store;
	CHECK(file.Read(store));
	CHECK(store.Count() == 0);

	// And is saved over, there being nothing in it to lose.
	REQUIRE(file.Modify([](AccountStore& into) { return into.Save("main", "hunter2"); }));
	AccountStore read;
	REQUIRE(file.Read(read));
	CHECK(read.Count() == 1);
}

TEST_CASE("nothing is left beside the file once it is written") {
	TempFile temp("tidy");
	AccountFile file(temp.Path());

	REQUIRE(file.Modify([](AccountStore& store) { return store.Save("main", "hunter2"); }));

	CHECK(GetFileAttributesA((temp.Path() + ".tmp").c_str()) == INVALID_FILE_ATTRIBUTES);
}

TEST_CASE("forgetting an account is a change like any other") {
	TempFile temp("forgotten");
	AccountFile file(temp.Path());

	REQUIRE(file.Modify([](AccountStore& store) { return store.Save("main", "hunter2"); }));
	REQUIRE(file.Modify([](AccountStore& store) { return store.Forget("MAIN"); }));

	AccountStore read;
	REQUIRE(file.Read(read));
	CHECK(read.Count() == 0);
}

TEST_CASE("saves from several clients at once all survive") {
	TempFile temp("contended");

	// One AccountFile each, as separate clients have, all naming one path. Each
	// keeps one account of its own; the lock and the read before every write are
	// the only reason all of them are there at the end.
	const int clients = 8;
	std::vector<std::thread> running;
	std::vector<bool> saved(clients, false);
	for (int i = 0; i < clients; i++) {
		running.push_back(std::thread([&temp, &saved, i]() {
			AccountFile file(temp.Path());
			std::stringstream named;
			named << "account" << i;
			saved[i] = file.Modify([&named](AccountStore& store) {
				return store.Save(named.str(), "hunter2");
			});
		}));
	}
	for (unsigned int i = 0; i < running.size(); i++)
		running[i].join();

	AccountStore read;
	AccountFile file(temp.Path());
	REQUIRE(file.Read(read));
	for (int i = 0; i < clients; i++) {
		std::stringstream named;
		named << "account" << i;
		CHECK(saved[i]);
		CHECK(read.Find(named.str()) != NULL);
	}
	CHECK(read.Count() == clients);
}
