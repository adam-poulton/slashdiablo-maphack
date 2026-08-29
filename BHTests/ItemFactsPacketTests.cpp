#include "doctest.h"
#include <fstream>
#include <string>
#include <vector>
#include "CaptureFormat.h"
#include "ItemFacts.h"
#include "ItemFactsPacket.h"
#include "ItemTables.h"

/*
 * Reading real item packets, with no game running and no archives loaded.
 *
 * How far a packet can be read without the game's data tables is decided by the
 * packet itself: what an item is, what happened to it and what kind of thing it
 * is are all written before anything whose width the tables have to supply. So
 * those are what these check, against every packet a session recorded.
 *
 * Going further needs the widths themselves, which a capture does not yet
 * record. Until it does, the tables below stand in: enough for reading to reach
 * the item's code and stop there rather than walk off into the packet.
 */

using CaptureFormat::Fields;
using CaptureFormat::ParseLine;

namespace {

// Attributes for any code asked for, so that reading gets as far as the code
// without meeting a null. Nothing past the code is asserted, so what these say
// does not matter, only that they are there.
class AnyItemTables : public ItemFactsPacket::Tables {
public:
	AnyItemTables() {
		attributes.name = "test item";
		attributes.width = 1;
		attributes.height = 1;
		attributes.stackable = 0;
		attributes.useable = 0;
	}

	ItemAttributes* Attributes(const char* code) const override {
		return const_cast<ItemAttributes*>(&attributes);
	}

	// No stat has a width here, so reading stops at the first one rather than
	// guessing and running on through the rest of the packet.
	StatProperties* Stat(unsigned int stat) const override {
		return NULL;
	}

private:
	ItemAttributes attributes;
};

class CountingDiagnostics : public ItemFactsPacket::Diagnostics {
public:
	CountingDiagnostics() : unknownCodes(0), unreadableStats(0), failures(0) {}
	void UnknownItemCode(const char* code) override { unknownCodes++; }
	void UnreadableStat(unsigned int stat, const char* code) override {
		unreadableStats++;
	}
	void Failed(const char* code, const std::string& reason) override {
		failures++;
	}

	int unknownCodes;
	int unreadableStats;
	int failures;
};

struct PacketCase {
	std::string packet;
	std::string code;
	long long action;
	long long size;
};

std::vector<PacketCase> ReadPacketCases() {
	std::vector<PacketCase> cases;
	std::ifstream file("BHTests/fixtures/parse-cases.txt");
	REQUIRE_MESSAGE(file.is_open(),
		"fixture not found, run the tests from the repository root");

	std::string line;
	while (std::getline(file, line)) {
		if (!line.empty() && line[line.length() - 1] == '\r')
			line.erase(line.length() - 1);
		if (line.empty())
			continue;
		Fields fields = ParseLine(line);
		PacketCase one;
		one.packet = fields.Text("packet");
		one.code = fields.Text("code");
		one.action = fields.Number("action");
		one.size = fields.Number("packetSize");
		cases.push_back(one);
	}
	return cases;
}

/*
 * A packet with room after it.
 *
 * BitReader is given a pointer and nothing else, so it reads wherever it is
 * pointed. Reading stops at the first stat, which is well inside every recorded
 * packet, but the buffer is padded so that a packet which does not stop where
 * it is expected to reads zeroes rather than whatever follows in memory.
 */
std::vector<unsigned char> Padded(const std::string& packet) {
	std::vector<unsigned char> buffer(packet.length() + 256, 0);
	for (std::size_t i = 0; i < packet.length(); i++)
		buffer[i] = (unsigned char)packet[i];
	return buffer;
}

}  // namespace

TEST_CASE("every recorded packet says which item it is") {
	std::vector<PacketCase> cases = ReadPacketCases();
	CHECK(cases.size() > 500);

	AnyItemTables tables;
	CountingDiagnostics diagnostics;
	ItemFactsPacket::Reader reader(tables, diagnostics, true);

	int checked = 0;
	for (unsigned int i = 0; i < cases.size(); i++) {
		std::vector<unsigned char> buffer = Padded(cases[i].packet);

		ItemFacts item = {};
		reader.Read(&buffer[0], &item);

		// The code and what happened to the item are written before anything
		// the tables have a say in, so both hold whatever the tables answer.
		CHECK(std::string(item.code, 3) == cases[i].code);
		CHECK((long long)item.action == cases[i].action);
		checked++;
	}
	CHECK(checked == (int)cases.size());
}

TEST_CASE("reading stops rather than running on past a stat it cannot place") {
	std::vector<PacketCase> cases = ReadPacketCases();

	AnyItemTables tables;
	CountingDiagnostics diagnostics;
	ItemFactsPacket::Reader reader(tables, diagnostics, true);

	for (unsigned int i = 0; i < cases.size(); i++) {
		std::vector<unsigned char> buffer = Padded(cases[i].packet);
		ItemFacts item = {};
		reader.Read(&buffer[0], &item);
	}

	// Nothing here knows a stat's width, so any packet carrying one says so
	// rather than reading past it in silence.
	CHECK(diagnostics.unreadableStats > 0);
	// An item whose code the tables answer for is never an unknown code.
	CHECK(diagnostics.unknownCodes == 0);
}

TEST_CASE("an item code the tables do not know abandons the item") {
	// The tables answer for nothing at all.
	class NoTables : public ItemFactsPacket::Tables {
		ItemAttributes* Attributes(const char* code) const override {
			return NULL;
		}
		StatProperties* Stat(unsigned int stat) const override { return NULL; }
	} tables;

	CountingDiagnostics diagnostics;
	ItemFactsPacket::Reader reader(tables, diagnostics, true);

	std::vector<PacketCase> cases = ReadPacketCases();
	// An ear is described by the packet rather than by the tables and takes
	// another path through the reader, so a plain item is what is wanted here.
	std::vector<unsigned char> buffer = Padded(cases[0].packet);

	ItemFacts item = {};
	CHECK(reader.Read(&buffer[0], &item) == false);
	CHECK(diagnostics.unknownCodes == 1);
	// The code is still read: it is what the lookup was made with.
	CHECK(std::string(item.code, 3) == cases[0].code);
}
