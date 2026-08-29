#include "doctest.h"
#include <fstream>
#include <map>
#include <string>
#include <vector>
#include "CaptureFormat.h"
#include "ItemFacts.h"
#include "ItemFactsPacket.h"
#include "ItemTables.h"

/*
 * Reading whole items out of real packets, against the game's own tables.
 *
 * A capture carries three things that together make a worked example: the bytes
 * that arrived, the table rows they were read against, and what the game made
 * of them. Given the first two, reading has to arrive at the third.
 *
 * This is the check that matters for the reader, because of how a packet is
 * written. Nothing in it is aligned or labelled; each field begins where the
 * last one ended. A field read one bit too wide does not produce one wrong
 * value, it produces wrong values from there to the end of the item, and the
 * count of properties at the end is where that shows up most plainly.
 */

using CaptureFormat::Fields;
using CaptureFormat::ParseLine;

namespace {

// The tables a capture recorded, standing in for the ones the archives fill.
class FixtureTables : public ItemFactsPacket::Tables {
public:
	void AddStat(unsigned int at, const StatProperties& stat) {
		if (widths.size() <= at)
			widths.resize(at + 1);
		widths[at] = stat;
		present.resize(widths.size(), false);
		present[at] = true;
	}

	void AddItem(const std::string& code, const ItemAttributes& item) {
		items[code] = item;
	}

	ItemAttributes* Attributes(const char* code) const override {
		std::map<std::string, ItemAttributes>::const_iterator found =
			items.find(code);
		return (found == items.end()) ? NULL
			: const_cast<ItemAttributes*>(&found->second);
	}

	StatProperties* Stat(unsigned int stat) const override {
		if (stat >= widths.size() || !present[stat])
			return NULL;
		return const_cast<StatProperties*>(&widths[stat]);
	}

	std::size_t StatCount() const { return widths.size(); }
	std::size_t ItemCount() const { return items.size(); }

private:
	std::vector<StatProperties> widths;
	std::vector<bool> present;
	std::map<std::string, ItemAttributes> items;
};

class SilentDiagnostics : public ItemFactsPacket::Diagnostics {};

std::vector<Fields> ReadFixture(const std::string& name) {
	std::vector<Fields> records;
	std::ifstream file("BHTests/fixtures/" + name);
	REQUIRE_MESSAGE(file.is_open(),
		"fixture not found, run the tests from the repository root: " << name);

	std::string line;
	while (std::getline(file, line)) {
		if (!line.empty() && line[line.length() - 1] == '\r')
			line.erase(line.length() - 1);
		if (!line.empty())
			records.push_back(ParseLine(line));
	}
	return records;
}

void LoadTables(FixtureTables& tables) {
	std::vector<Fields> records = ReadFixture("tables.txt");
	for (unsigned int i = 0; i < records.size(); i++) {
		const Fields& r = records[i];
		if (r.type == "statwidths") {
			StatProperties stat;
			stat.name = r.Text("name");
			stat.saveBits = (unsigned char)r.Number("saveBits");
			stat.saveParamBits = (unsigned char)r.Number("saveParamBits");
			stat.saveAdd = (unsigned char)r.Number("saveAdd");
			stat.op = (unsigned char)r.Number("op");
			stat.sendParamBits = (unsigned char)r.Number("sendParamBits");
			stat.ID = (unsigned short)r.Number("at");
			tables.AddStat((unsigned int)r.Number("at"), stat);
		} else if (r.type == "itemattrs") {
			ItemAttributes item;
			std::string code = r.Text("code");
			item.name = r.Text("name");
			item.category = r.Text("category");
			for (int c = 0; c < 4; c++)
				item.code[c] = (c < (int)code.length()) ? code[c] : 0;
			item.width = (unsigned char)r.Number("width");
			item.height = (unsigned char)r.Number("height");
			item.stackable = (unsigned char)r.Number("stackable");
			item.useable = (unsigned char)r.Number("useable");
			item.throwable = (unsigned char)r.Number("throwable");
			item.itemLevel = (unsigned char)r.Number("itemLevel");
			item.unusedFlags = 0;
			item.flags = (unsigned int)r.Number("flags");
			item.flags2 = (unsigned int)r.Number("flags2");
			item.qualityLevel = (unsigned char)r.Number("qualityLevel");
			item.magicLevel = (unsigned char)r.Number("magicLevel");
			tables.AddItem(code, item);
		}
	}
}

// BitReader is given a pointer and nothing else, so a packet is copied out with
// room after it: a reading that does not stop where it should meets zeroes
// rather than whatever happens to follow in memory.
std::vector<unsigned char> Padded(const std::string& packet) {
	std::vector<unsigned char> buffer(packet.length() + 256, 0);
	for (std::size_t i = 0; i < packet.length(); i++)
		buffer[i] = (unsigned char)packet[i];
	return buffer;
}

}  // namespace

TEST_CASE("the tables a capture recorded are complete enough to read with") {
	FixtureTables tables;
	LoadTables(tables);

	CHECK(tables.StatCount() > 300);
	CHECK(tables.ItemCount() > 200);
	// Strength is stat zero and is the one every other width is counted from.
	REQUIRE(tables.Stat(0) != NULL);
	CHECK(tables.Stat(0)->name == "strength");
}

TEST_CASE("a recorded packet reads back as the item the game made of it") {
	FixtureTables tables;
	LoadTables(tables);
	SilentDiagnostics diagnostics;
	ItemFactsPacket::Reader reader(tables, diagnostics, true);

	std::vector<Fields> cases = ReadFixture("parse-cases.txt");

	int checked = 0, withProperties = 0, large = 0;
	for (unsigned int i = 0; i < cases.size(); i++) {
		const Fields& c = cases[i];
		// Captures taken before the reading was recorded have nothing here to
		// check against, and only the packet itself.
		if (!c.Has("quality"))
			continue;

		std::vector<unsigned char> buffer = Padded(c.Text("packet"));
		ItemFacts item = {};
		reader.Read(&buffer[0], &item);

		INFO("item code " << c.Text("code") << ", packet " << i);
		CHECK(std::string(item.code, 3) == c.Text("code"));
		CHECK(item.name == c.Text("name"));
		CHECK((long long)item.action == c.Number("action"));
		CHECK((long long)item.quality == c.Number("quality"));
		CHECK((long long)item.level == c.Number("level"));
		CHECK((long long)item.sockets == c.Number("sockets"));
		CHECK((long long)item.usedSockets == c.Number("usedSockets"));
		CHECK((long long)item.defense == c.Number("defense"));
		CHECK((long long)item.durability == c.Number("durability"));
		CHECK((long long)item.maxDurability == c.Number("maxDurability"));
		CHECK((long long)item.amount == c.Number("amount"));
		CHECK((long long)item.prefix == c.Number("prefix"));
		CHECK((long long)item.suffix == c.Number("suffix"));
		CHECK((long long)item.setCode == c.Number("setCode"));
		CHECK((long long)item.uniqueCode == c.Number("uniqueCode"));
		CHECK((long long)item.runewordId == c.Number("runewordId"));
		CHECK(item.identified == c.Boolean("identified"));
		CHECK(item.ethereal == c.Boolean("ethereal"));
		CHECK(item.runeword == c.Boolean("runeword"));
		CHECK(item.personalized == c.Boolean("personalized"));
		CHECK(item.isGold == c.Boolean("isGold"));
		CHECK(item.ear == c.Boolean("ear"));
		CHECK(item.simpleItem == c.Boolean("simpleItem"));
		CHECK(item.hasSockets == c.Boolean("hasSockets"));

		// The stat list is read last and its widths come from the tables, so
		// anything misplaced earlier arrives here as the wrong count.
		CHECK((long long)item.properties.size() == c.Number("properties"));

		checked++;
		if (c.Number("properties") > 0)
			withProperties++;
		if (c.Number("packetSize") >= 30)
			large++;
	}

	// Worth asserting rather than assuming: a fixture set that quietly stopped
	// carrying what it read out as would pass every check above by skipping.
	CHECK(checked > 300);
	CHECK(withProperties > 50);
	CHECK(large > 100);
}
