#include "doctest.h"
#include <windows.h>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include "Modules/Session/SessionFile.h"

/*
 * A session's log on disk.
 *
 * A real folder in the temporary directory rather than a pretend one, because
 * what is being checked is what a client finds there afterwards: a folder that
 * was not there, a log added to across two openings, a last line a dying client
 * never finished.
 */

using namespace PlayEvents;

namespace {

// A folder of its own for each test, removed with whatever was written into it.
class TempFolder {
	private:
		std::filesystem::path folder;

	public:
		explicit TempFolder(const char* named) {
			std::stringstream name;
			name << "BH_" << named << "_" << GetCurrentProcessId();
			folder = std::filesystem::temp_directory_path() / name.str();
			std::filesystem::remove_all(folder);
		}

		~TempFolder() {
			std::error_code ignored;
			std::filesystem::remove_all(folder, ignored);
		}

		bool Exists() const { return std::filesystem::exists(folder); }
		std::string File(const char* name) const { return (folder / name).string(); }
};

std::string TextOf(const std::string& path) {
	std::ifstream file(path.c_str(), std::ios::binary);
	std::stringstream held;
	held << file.rdbuf();
	return held.str();
}

void AddText(const std::string& path, const std::string& text) {
	std::ofstream file(path.c_str(), std::ios::binary | std::ios::app);
	REQUIRE(file.is_open());
	file << text;
}

std::tm Moment(int year, int month, int day, int hour, int minute, int second) {
	std::tm moment = {};
	moment.tm_year = year - 1900;
	moment.tm_mon = month - 1;
	moment.tm_mday = day;
	moment.tm_hour = hour;
	moment.tm_min = minute;
	moment.tm_sec = second;
	return moment;
}

}

TEST_CASE("a log is written a line per event and read back in order") {
	TempFolder folder("session_roundtrip");
	const std::string path = folder.File("one.jsonl");

	{
		SessionFile file;
		REQUIRE(file.Open(path));
		file.Append({ 0, Joined{ "baal-1", 2, 87, 1000 } });
		file.Append({ 5000, Experience{ 1500 } });
		file.Append({ 9000, Left() });
		file.Flush();
	}

	CHECK(TextOf(path) ==
		"{\"t\":0,\"e\":\"join\",\"game\":\"baal-1\",\"diff\":2,\"level\":87,\"exp\":1000}\n"
		"{\"t\":5000,\"e\":\"exp\",\"exp\":1500}\n"
		"{\"t\":9000,\"e\":\"leave\"}\n");

	std::vector<Event> read;
	REQUIRE(SessionFile::Read(path, read));
	REQUIRE(read.size() == 3);
	CHECK(std::holds_alternative<Joined>(read[0].fact));
	CHECK(read[1].at == 5000);
	CHECK(std::get<Experience>(read[1].fact).total == 1500);
	CHECK(std::holds_alternative<Left>(read[2].fact));
}

TEST_CASE("the log's folder is made when it is not there") {
	TempFolder folder("session_folder");
	REQUIRE_FALSE(folder.Exists());

	SessionFile file;
	CHECK(file.Open(folder.File("one.jsonl")));
	CHECK(file.IsOpen());
	CHECK(folder.Exists());
}

TEST_CASE("a flushed line is on disk while the log is still open") {
	TempFolder folder("session_flush");
	const std::string path = folder.File("one.jsonl");

	SessionFile file;
	REQUIRE(file.Open(path));
	file.Append({ 100, Died() });
	file.Flush();

	std::vector<Event> read;
	REQUIRE(SessionFile::Read(path, read));
	CHECK(read.size() == 1);
}

TEST_CASE("a log opened again is added to, not started over") {
	TempFolder folder("session_append");
	const std::string path = folder.File("one.jsonl");

	{
		SessionFile file;
		REQUIRE(file.Open(path));
		file.Append({ 0, Joined{ "a", 0, 1, 0 } });
	}
	{
		SessionFile file;
		REQUIRE(file.Open(path));
		file.Append({ 100, Left() });
	}

	std::vector<Event> read;
	REQUIRE(SessionFile::Read(path, read));
	CHECK(read.size() == 2);
}

TEST_CASE("a last line the client never finished is skipped, and the rest is read") {
	TempFolder folder("session_truncated");
	const std::string path = folder.File("one.jsonl");

	{
		SessionFile file;
		REQUIRE(file.Open(path));
		file.Append({ 0, Joined{ "a", 0, 1, 0 } });
		file.Append({ 100, Experience{ 50 } });
	}
	AddText(path, "{\"t\":200,\"e\":\"ex");

	std::vector<Event> read;
	REQUIRE(SessionFile::Read(path, read));
	REQUIRE(read.size() == 2);
	CHECK(read[1].at == 100);
}

TEST_CASE("a line this build cannot read is skipped where it stands") {
	TempFolder folder("session_unknown");
	const std::string path = folder.File("one.jsonl");

	{
		SessionFile file;
		REQUIRE(file.Open(path));
		file.Append({ 0, Joined{ "a", 0, 1, 0 } });
	}
	AddText(path, "{\"t\":50,\"e\":\"from-a-later-build\"}\n");
	AddText(path, "{\"t\":100,\"e\":\"leave\"}\r\n");

	std::vector<Event> read;
	REQUIRE(SessionFile::Read(path, read));
	REQUIRE(read.size() == 2);
	CHECK(std::holds_alternative<Left>(read[1].fact));
}

TEST_CASE("reading a log that is not there says so") {
	TempFolder folder("session_missing");
	std::vector<Event> read;
	CHECK_FALSE(SessionFile::Read(folder.File("none.jsonl"), read));
	CHECK(read.empty());
}

TEST_CASE("nothing is written to a log that could not be opened") {
	SessionFile file;
	CHECK_FALSE(file.IsOpen());
	file.Append({ 0, Left() });
	file.Flush();
	CHECK_FALSE(file.IsOpen());
}

TEST_CASE("a log is named for when it began, whose it is and the client writing it") {
	CHECK(SessionFile::NameFor(Moment(2026, 11, 27, 9, 5, 3), "Sorc_One", 4321) ==
		"20261127-090503-Sorc_One-4321.jsonl");
}

TEST_CASE("a character's name is cut down to what is safe in a file name") {
	CHECK(SessionFile::NameFor(Moment(2026, 1, 2, 3, 4, 5), "a/b\\c:d.e f", 1) ==
		"20260102-030405-abcdef-1.jsonl");
	CHECK(SessionFile::NameFor(Moment(2026, 1, 2, 3, 4, 5), "..", 1) ==
		"20260102-030405-character-1.jsonl");
}
