#include "doctest.h"
#include "CaptureFormat.h"

/*
 * The capture format is what stands between a session of play and a fixture the
 * filter can be replayed against, so what it has to survive is what an item
 * packet and an item's name actually contain: raw bytes, colour codes, and the
 * comparison operators a filter rule is written with.
 */

using CaptureFormat::Escape;
using CaptureFormat::Fields;
using CaptureFormat::ParseLine;
using CaptureFormat::Record;
using CaptureFormat::Unescape;

TEST_CASE("printable text is left as it is") {
	CHECK(Escape("ILVL>85") == "ILVL>85");
	CHECK(Unescape("ILVL>85") == "ILVL>85");
}

TEST_CASE("a tab cannot reach the file, since a record is one line") {
	CHECK(Escape("a\tb") == "a\\tb");
	CHECK(Unescape("a\\tb") == "a\tb");
}

TEST_CASE("a line ending cannot reach the file either") {
	CHECK(Escape("a\nb") == "a\\nb");
	CHECK(Escape("a\r\nb") == "a\\r\\nb");
	CHECK(Unescape("a\\r\\nb") == "a\r\nb");
}

TEST_CASE("a backslash stands for itself once escaped") {
	CHECK(Escape("a\\tb") == "a\\\\tb");
	// Without escaping the backslash the tab would come back as a real one.
	CHECK(Unescape(Escape("a\\tb")) == "a\\tb");
}

TEST_CASE("the colour codes in an item's name survive a round trip") {
	// An item name carries colour as byte 0xFF followed by a code.
	std::string name = "\xFF" "c4Ring of the Leech";

	CHECK(Escape(name) == "\\xffc4Ring of the Leech");
	CHECK(Unescape(Escape(name)) == name);
}

TEST_CASE("every byte value survives a round trip") {
	std::string all;
	for (int i = 0; i < 256; i++)
		all += (char)i;

	CHECK(Unescape(Escape(all)) == all);
	CHECK(Unescape(Escape(all)).length() == 256);
}

TEST_CASE("text that is not escaping this module wrote is kept as it stands") {
	// A lone backslash, and one before a letter that means nothing here.
	CHECK(Unescape("a\\") == "a\\");
	CHECK(Unescape("a\\q") == "a\\q");
	// \x needs two hex digits to be a byte.
	CHECK(Unescape("a\\xzz") == "a\\xzz");
	CHECK(Unescape("a\\x4") == "a\\x4");
}

TEST_CASE("a record renders as its type followed by named fields") {
	Record record("drop");
	record.Add("code", std::string("rin"));
	record.Add("blocked", true);
	record.Add("keepIndex", (long long)0);

	CHECK(record.Line() == "drop\tcode=rin\tblocked=1\tkeepIndex=0");
}

TEST_CASE("zero and false are written rather than left out") {
	Record record("drop");
	record.Add("blocked", false);
	record.Add("color", (long long)0);
	record.Add("name", std::string());

	CHECK(record.Line() == "drop\tblocked=0\tcolor=0\tname=");
}

TEST_CASE("a record reads back as the fields it was written with") {
	Record record("drop");
	record.Add("code", std::string("rin"));
	record.Add("blocked", false);
	record.Add("keepIndex", (long long)7);

	Fields fields = ParseLine(record.Line());

	CHECK(fields.type == "drop");
	CHECK(fields.Text("code") == "rin");
	CHECK(fields.Boolean("blocked") == false);
	CHECK(fields.Number("keepIndex") == 7);
}

TEST_CASE("a value may contain an equals sign") {
	// Filter rules are written with comparisons, so this is the common case.
	Record record("rule");
	record.Add("condition", std::string("QUALITY=4 ILVL>85"));

	Fields fields = ParseLine(record.Line());

	CHECK(fields.Text("condition") == "QUALITY=4 ILVL>85");
}

TEST_CASE("an absent field reads as empty, zero and false") {
	Fields fields = ParseLine("drop\tcode=rin");

	CHECK(fields.Has("code"));
	CHECK_FALSE(fields.Has("blocked"));
	CHECK(fields.Text("blocked") == "");
	CHECK(fields.Number("blocked") == 0);
	CHECK(fields.Boolean("blocked") == false);
}

TEST_CASE("a blank line has no type") {
	CHECK(ParseLine("").type == "");
	CHECK(ParseLine("   ").type == "");
	CHECK(ParseLine("\r").type == "");
}

TEST_CASE("a field carrying no value is passed over") {
	Fields fields = ParseLine("drop\tstray\tcode=rin");

	CHECK(fields.type == "drop");
	CHECK_FALSE(fields.Has("stray"));
	CHECK(fields.Text("code") == "rin");
}

TEST_CASE("a negative number reads back as one") {
	Record record("drop");
	record.Add("pingLevel", (long long)-1);

	CHECK(ParseLine(record.Line()).Number("pingLevel") == -1);
}

TEST_CASE("an index at the top of its range reads back intact") {
	// NO_RULE_MATCH is stored as an unsigned int at its maximum, so the field
	// has to carry a value larger than a signed int holds.
	Record record("drop");
	record.Add("ignoreIndex", (long long)0xFFFFFFFFu);

	CHECK(ParseLine(record.Line()).Number("ignoreIndex") == 0xFFFFFFFFll);
}

TEST_CASE("a packet of raw bytes survives a round trip") {
	std::string packet;
	packet += (char)0x9C;
	packet += (char)0x04;
	packet += (char)0x1B;
	packet += (char)0x00;
	packet += (char)0xFF;

	Record record("drop");
	record.Add("packet", packet);

	CHECK(ParseLine(record.Line()).Text("packet") == packet);
}
