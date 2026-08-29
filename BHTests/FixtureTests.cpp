#include "doctest.h"
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "CaptureFormat.h"

/*
 * The fixtures are recordings of the game deciding what to do with real items,
 * and everything later built on them assumes they can be read and that what
 * they say holds together. These check that much: that the files parse, that a
 * record carries the fields it is supposed to, and that a recorded packet is
 * the length it claims. A fixture failing here is a broken recording rather
 * than a broken filter.
 *
 * Paths are relative to the working directory, which is the repository root
 * both in CI and when the test is run by hand from there.
 */

using CaptureFormat::Fields;
using CaptureFormat::ParseLine;
using CaptureFormat::Unescape;

namespace {

std::vector<Fields> ReadFixture(const std::string& name) {
	std::vector<Fields> records;
	std::ifstream file("BHTests/fixtures/" + name);
	REQUIRE_MESSAGE(file.is_open(),
		"fixture not found, run the tests from the repository root: " << name);

	std::string line;
	while (std::getline(file, line)) {
		if (!line.empty() && line[line.length() - 1] == '\r')
			line.erase(line.length() - 1);
		if (line.empty())
			continue;
		records.push_back(ParseLine(line));
	}
	return records;
}

}  // namespace

TEST_CASE("every parse case carries a packet of the length it states") {
	std::vector<Fields> records = ReadFixture("parse-cases.txt");
	CHECK(records.size() > 500);

	std::set<std::string> codes;
	int large = 0;
	for (unsigned int i = 0; i < records.size(); i++) {
		const Fields& r = records[i];
		REQUIRE(r.type == "parse");
		CHECK(r.Has("code"));
		CHECK(r.Has("packet"));
		CHECK(r.Has("packetSize"));

		std::string packet = r.Text("packet");
		CHECK(packet.length() == (std::size_t)r.Number("packetSize"));
		// Every one of these was read from a 0x9c item packet.
		REQUIRE(packet.length() > 0);
		CHECK((unsigned char)packet[0] == 0x9C);

		codes.insert(r.Text("code"));
		if (r.Number("packetSize") >= 30)
			large++;
	}

	// The fixtures are meant to stand for the variety a session sees, so a
	// curation that quietly collapsed to one item should fail here.
	CHECK(codes.size() > 100);
	CHECK(large > 50);
}

TEST_CASE("escaping survived being written to and read from a fixture") {
	std::vector<Fields> records = ReadFixture("parse-cases.txt");

	// Unescaping is what turned the file's text back into these bytes, so
	// escaping them again has to reproduce the file's text exactly.
	for (unsigned int i = 0; i < records.size(); i++) {
		std::string packet = records[i].Text("packet");
		CHECK(Unescape(CaptureFormat::Escape(packet)) == packet);
	}
}

TEST_CASE("every filter case belongs to a rule set that is present") {
	std::vector<Fields> records = ReadFixture("filter-cases.txt");
	CHECK(records.size() > 1000);

	std::map<std::string, int> ruleCounts;
	int headers = 0, drops = 0;
	std::string currentSet;

	for (unsigned int i = 0; i < records.size(); i++) {
		const Fields& r = records[i];
		if (r.type == "rule") {
			CHECK(r.Has("condition"));
			ruleCounts[r.Text("set")]++;
		} else if (r.type == "header") {
			headers++;
			currentSet = r.Text("ruleSet");
			// The rules a session was played against have to have been read
			// before it, or its decisions mean nothing.
			CHECK(ruleCounts.count(currentSet) == 1);
			CHECK(ruleCounts[currentSet] > 100);
		} else if (r.type == "drop") {
			drops++;
			CHECK_FALSE(currentSet.empty());
		}
	}

	CHECK(headers >= 2);
	CHECK(drops > 100);
	CHECK(ruleCounts.size() >= 1);
}

TEST_CASE("every recorded item carries the world it was judged in") {
	std::vector<Fields> records = ReadFixture("filter-cases.txt");

	std::set<long long> areas, areaLevels, charLevels;
	int drops = 0;
	for (unsigned int i = 0; i < records.size(); i++) {
		const Fields& r = records[i];
		if (r.type != "drop")
			continue;
		drops++;

		// Without these a drop cannot be replayed: they are what the
		// conditions reading the character and the area compare against.
		CHECK(r.Has("areaId"));
		CHECK(r.Has("areaLevel"));
		CHECK(r.Has("charLevel"));
		CHECK(r.Has("charClass"));
		CHECK(r.Has("difficulty"));
		// And these are the decision itself.
		CHECK(r.Has("blocked"));
		CHECK(r.Has("keepIndex"));
		CHECK(r.Has("ignoreIndex"));

		areas.insert(r.Number("areaId"));
		areaLevels.insert(r.Number("areaLevel"));
		charLevels.insert(r.Number("charLevel"));
	}

	CHECK(drops > 100);
	// A capture from a single spot would replay a great deal less than it
	// appears to, so the spread is worth asserting rather than assuming.
	CHECK(areas.size() >= 10);
	CHECK(areaLevels.size() >= 8);
	// The character level a capture was excluded for must not have returned.
	CHECK(charLevels.count(84) == 0);
}
