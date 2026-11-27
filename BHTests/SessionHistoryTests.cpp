#include "doctest.h"
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include "Modules/Session/SessionFile.h"
#include "Modules/Session/SessionHistory.h"

/*
 * The sessions there are logs of, and the index kept of them.
 *
 * A real folder in the temporary directory, because what is being checked is
 * what a client finds in one: logs it has not read, one that has grown, one
 * that has gone.
 */

using namespace SessionEvents;

namespace {

class TempFolder {
	private:
		std::filesystem::path folder;

	public:
		explicit TempFolder(const char* named) {
			std::stringstream name;
			name << "BH_" << named << "_" << GetCurrentProcessId();
			folder = std::filesystem::temp_directory_path() / name.str();
			std::filesystem::remove_all(folder);
			std::filesystem::create_directories(folder);
		}

		~TempFolder() {
			std::error_code ignored;
			std::filesystem::remove_all(folder, ignored);
		}

		// As the history is given it: ending in its separator.
		std::string Path() const { return folder.string() + "\\"; }
		bool Has(const std::string& name) const { return std::filesystem::exists(folder / name); }
};

Started Someone(const std::string& character) {
	Started fact = {};
	fact.startedAt = 1795000000;
	fact.character = character;
	fact.classId = 1;
	return fact;
}

// A session of one game, in which so much experience was gained.
std::vector<Event> SessionOf(const std::string& character, unsigned long long gained) {
	return {
		{ 0, Someone(character) },
		{ 0, Joined{ "baal-1", 2, 87, 1000 } },
		{ 5000, Experience{ 1000 + gained } },
		{ 9000, Left() },
	};
}

void WriteLog(const TempFolder& folder, const std::string& name, const std::vector<Event>& events) {
	SessionFile file;
	REQUIRE(file.Open(folder.Path() + name));
	for (const Event& event : events)
		file.Append(event);
	file.Flush();
}

std::vector<std::string> Files(const SessionHistory& history) {
	std::vector<std::string> files;
	for (const SessionIndex::Entry& entry : history.Entries())
		files.push_back(entry.file);
	return files;
}

const std::set<std::string> kNone;

}

TEST_CASE("a session is summed up from its log") {
	Started who = Someone("Sorc_One");
	who.hardcore = true;
	const std::vector<Event> events = {
		{ 0, who },
		{ 1000, Joined{ "baal-1", 2, 87, 1000 } },
		{ 5000, Experience{ 1500 } },
		{ 6000, Killed{ 131, 1, "Minion of Destruction", TierNormal, 3 } },
		{ 7000, Dropped{ 5, "Ber Rune", "r30", 2, 8, 3, 131, "Throne of Destruction" } },
		{ 9000, Left() },
		{ 20000, Joined{ "baal-2", 2, 87, 1500 } },
		{ 21000, Died() },
		{ 22000, Experience{ 1400 } },
		{ 23000, LevelReached{ 88 } },
		{ 30000, Left() },
	};

	const SessionIndex::Entry entry = SessionIndex::Summarise("one.jsonl", 1234, events);
	CHECK(entry.file == "one.jsonl");
	CHECK(entry.size == 1234);
	CHECK(entry.startedAt == 1795000000);
	CHECK(entry.character == "Sorc_One");
	CHECK(entry.classId == 1);
	CHECK(entry.hardcore);
	CHECK(entry.level == 88);
	CHECK(entry.sessionMs == 30000);
	CHECK(entry.playedMs == 18000);
	CHECK(entry.games == 2);
	CHECK(entry.experience == 400);
	CHECK(entry.kills == 3);
	CHECK(entry.drops == 1);
	CHECK(entry.deaths == 1);
	CHECK(entry.runsFinished == 0);
}

TEST_CASE("only the runs that were finished are counted") {
	const std::vector<Event> events = {
		{ 0, Someone("Sorc_One") },
		{ 0, Joined{ "", 0, 1, 0 } },
		{ 0, RunStarted{ "act1", "Act 1", { "Andariel" } } },
		{ 1000, RunEnded{ false } },
		{ 1000, RunStarted{ "act1", "Act 1", { "Andariel" } } },
		{ 5000, SplitReached{ 0, false } },
		{ 5000, RunEnded{ true } },
	};
	CHECK(SessionIndex::Summarise("one.jsonl", 0, events).runsFinished == 1);
}

TEST_CASE("a log with nothing in it sums up to nothing") {
	const SessionIndex::Entry entry = SessionIndex::Summarise("empty.jsonl", 0, {});
	CHECK(entry.character.empty());
	CHECK(entry.games == 0);
	CHECK(entry.sessionMs == 0);
	CHECK(entry.experience == 0);
}

TEST_CASE("the index is written and read back as it was") {
	std::vector<SessionIndex::Entry> entries;
	entries.push_back(SessionIndex::Summarise("b.jsonl", 200, SessionOf("Sorc_One", 500)));
	entries.push_back(SessionIndex::Summarise("a.jsonl", 100, SessionOf("Barb_Two", 0)));
	entries[1].hardcore = true;
	entries[1].experience = -250;

	std::vector<SessionIndex::Entry> read;
	REQUIRE(SessionIndex::Read(SessionIndex::Write(entries), read));
	REQUIRE(read.size() == 2);
	CHECK(read[0].file == "b.jsonl");
	CHECK(read[0].size == 200);
	CHECK(read[0].startedAt == 1795000000);
	CHECK(read[0].character == "Sorc_One");
	CHECK(read[0].level == 87);
	CHECK(read[0].sessionMs == 9000);
	CHECK(read[0].playedMs == 9000);
	CHECK(read[0].games == 1);
	CHECK(read[0].experience == 500);
	CHECK(read[1].character == "Barb_Two");
	CHECK(read[1].hardcore);
	CHECK(read[1].experience == -250);
}

TEST_CASE("an index that is not one is refused") {
	std::vector<SessionIndex::Entry> read;
	CHECK_FALSE(SessionIndex::Read("", read));
	CHECK_FALSE(SessionIndex::Read("{\"sessions\":", read));
	CHECK_FALSE(SessionIndex::Read("[1,2]", read));
	CHECK(read.empty());
}

TEST_CASE("an entry that names no log is left out of the index") {
	std::vector<SessionIndex::Entry> read;
	REQUIRE(SessionIndex::Read("{\"sessions\":[{\"char\":\"x\"},{\"file\":\"a.jsonl\"}]}", read));
	REQUIRE(read.size() == 1);
	CHECK(read[0].file == "a.jsonl");
	CHECK(read[0].games == 0);
}

TEST_CASE("the oldest logs past the number kept are the ones to go") {
	const std::vector<std::string> logs = { "20261103-c.jsonl", "20261101-a.jsonl",
		"20261104-d.jsonl", "20261102-b.jsonl" };
	CHECK(SessionIndex::Expired(logs, 2, kNone) ==
		std::vector<std::string>{ "20261101-a.jsonl", "20261102-b.jsonl" });
	CHECK(SessionIndex::Expired(logs, 4, kNone).empty());
	CHECK(SessionIndex::Expired(logs, 9, kNone).empty());
}

TEST_CASE("keeping none is keeping them all") {
	CHECK(SessionIndex::Expired({ "a.jsonl", "b.jsonl" }, 0, kNone).empty());
}

TEST_CASE("a log that is spared stays, and counts towards what is kept") {
	const std::vector<std::string> logs = { "1.jsonl", "2.jsonl", "3.jsonl", "4.jsonl" };
	CHECK(SessionIndex::Expired(logs, 2, { "1.jsonl" }) ==
		std::vector<std::string>{ "2.jsonl", "3.jsonl" });
	// Sparing all of them leaves nothing that can go.
	CHECK(SessionIndex::Expired(logs, 1, { "1.jsonl", "2.jsonl", "3.jsonl", "4.jsonl" }).empty());
}

TEST_CASE("the logs of a folder are read into the index, newest first") {
	TempFolder folder("history_reconcile");
	WriteLog(folder, "20261101-100000-Sorc_One-1.jsonl", SessionOf("Sorc_One", 100));
	WriteLog(folder, "20261103-100000-Barb_Two-1.jsonl", SessionOf("Barb_Two", 300));
	WriteLog(folder, "20261102-100000-Sorc_One-1.jsonl", SessionOf("Sorc_One", 200));

	SessionHistory history(folder.Path());
	history.Load();
	CHECK(history.Entries().empty());

	CHECK(history.Reconcile(10, kNone) == 0);
	CHECK(Files(history) == std::vector<std::string>{ "20261103-100000-Barb_Two-1.jsonl",
		"20261102-100000-Sorc_One-1.jsonl", "20261101-100000-Sorc_One-1.jsonl" });
	CHECK(history.Entries()[0].experience == 300);
	CHECK(history.Entries()[2].experience == 100);
}

TEST_CASE("no more logs are read at once than asked for, the newest first") {
	TempFolder folder("history_budget");
	WriteLog(folder, "1.jsonl", SessionOf("a", 1));
	WriteLog(folder, "2.jsonl", SessionOf("b", 2));
	WriteLog(folder, "3.jsonl", SessionOf("c", 3));

	SessionHistory history(folder.Path());
	CHECK(history.Reconcile(2, kNone) == 1);
	CHECK(Files(history) == std::vector<std::string>{ "3.jsonl", "2.jsonl" });
	CHECK(history.Reconcile(2, kNone) == 0);
	CHECK(Files(history).size() == 3);
}

TEST_CASE("the index is kept beside the logs, and is what the next client starts from") {
	TempFolder folder("history_saved");
	WriteLog(folder, "1.jsonl", SessionOf("a", 1));
	{
		SessionHistory history(folder.Path());
		history.Reconcile(10, kNone);
	}
	CHECK(folder.Has("index.json"));

	SessionHistory later(folder.Path());
	later.Load();
	REQUIRE(later.Entries().size() == 1);
	CHECK(later.Entries()[0].character == "a");
	// The index is no log of a session.
	CHECK(later.Reconcile(10, kNone) == 0);
	CHECK(later.Entries().size() == 1);
}

TEST_CASE("a log that has grown is read again") {
	TempFolder folder("history_grown");
	WriteLog(folder, "1.jsonl", SessionOf("a", 100));
	SessionHistory history(folder.Path());
	history.Reconcile(10, kNone);
	REQUIRE(history.Entries().size() == 1);
	CHECK(history.Entries()[0].games == 1);

	{
		SessionFile file;
		REQUIRE(file.Open(folder.Path() + "1.jsonl"));
		file.Append({ 20000, Joined{ "baal-2", 2, 87, 1100 } });
		file.Append({ 30000, Left() });
	}
	history.Reconcile(10, kNone);
	REQUIRE(history.Entries().size() == 1);
	CHECK(history.Entries()[0].games == 2);
}

TEST_CASE("a log that has gone is forgotten") {
	TempFolder folder("history_gone");
	WriteLog(folder, "1.jsonl", SessionOf("a", 1));
	WriteLog(folder, "2.jsonl", SessionOf("b", 2));
	SessionHistory history(folder.Path());
	history.Reconcile(10, kNone);
	REQUIRE(history.Entries().size() == 2);

	std::filesystem::remove(folder.Path() + "1.jsonl");
	history.Reconcile(10, kNone);
	CHECK(Files(history) == std::vector<std::string>{ "2.jsonl" });
}

TEST_CASE("a log still being written is left out of the index") {
	TempFolder folder("history_skip");
	WriteLog(folder, "1.jsonl", SessionOf("a", 1));
	WriteLog(folder, "2.jsonl", SessionOf("b", 2));
	SessionHistory history(folder.Path());
	history.Reconcile(10, kNone);
	REQUIRE(history.Entries().size() == 2);

	CHECK(history.Reconcile(10, { "2.jsonl" }) == 0);
	CHECK(Files(history) == std::vector<std::string>{ "1.jsonl" });
}

TEST_CASE("a folder that is not there has no history") {
	TempFolder folder("history_missing");
	SessionHistory history(folder.Path() + "none\\");
	history.Load();
	CHECK(history.Reconcile(10, kNone) == 0);
	CHECK(history.Entries().empty());
	CHECK(history.Prune(1, kNone) == 0);
}

TEST_CASE("a log is read by its name, and by nothing that leads out of the folder") {
	TempFolder folder("history_read");
	WriteLog(folder, "1.jsonl", SessionOf("a", 1));
	SessionHistory history(folder.Path());

	std::vector<Event> events;
	REQUIRE(history.ReadLog("1.jsonl", events));
	CHECK(events.size() == 4);

	CHECK_FALSE(history.ReadLog("..\\1.jsonl", events));
	CHECK_FALSE(history.ReadLog("sub/1.jsonl", events));
	CHECK_FALSE(history.ReadLog("none.jsonl", events));
}

TEST_CASE("pruning deletes the oldest logs and nothing that is spared") {
	TempFolder folder("history_prune");
	WriteLog(folder, "1.jsonl", SessionOf("a", 1));
	WriteLog(folder, "2.jsonl", SessionOf("b", 2));
	WriteLog(folder, "3.jsonl", SessionOf("c", 3));
	WriteLog(folder, "4.jsonl", SessionOf("d", 4));
	SessionHistory history(folder.Path());
	history.Reconcile(10, kNone);

	CHECK(history.Prune(2, { "1.jsonl" }) == 2);
	CHECK(folder.Has("1.jsonl"));
	CHECK_FALSE(folder.Has("2.jsonl"));
	CHECK_FALSE(folder.Has("3.jsonl"));
	CHECK(folder.Has("4.jsonl"));
	CHECK(folder.Has("index.json"));

	history.Reconcile(10, kNone);
	CHECK(Files(history) == std::vector<std::string>{ "4.jsonl", "1.jsonl" });
}
