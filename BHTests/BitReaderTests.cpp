#include "doctest.h"
#include "BitReader.h"

/*
 * BitReader is what reads an incoming 0x9c item packet field by field, so the
 * bit order it assumes is the one the whole item parse depends on.
 *
 * Within a byte it counts from the least significant bit up, and a multi-bit
 * read places the first bit it sees in the result's lowest position. A read of
 * eight aligned bits therefore comes back as the byte itself, and a read that
 * crosses a byte boundary takes the remaining high bits of the first byte
 * before the low bits of the next.
 */

// 0xAB is 1010 1011, so counting from the low bit gives 1,1,0,1,0,1,0,1.
static const unsigned char kTwoBytes[] = { 0xAB, 0xCD };

TEST_CASE("a byte read at an aligned offset is the byte itself") {
	BitReader reader(kTwoBytes);

	CHECK(reader.read(8) == 0xAB);
	CHECK(reader.read(8) == 0xCD);
}

TEST_CASE("reading advances the offset by the number of bits taken") {
	BitReader reader(kTwoBytes);

	CHECK(reader.offset == 0);
	reader.read(3);
	CHECK(reader.offset == 3);
	reader.read(5);
	CHECK(reader.offset == 8);
}

TEST_CASE("the low nibble of a byte is read before its high nibble") {
	BitReader reader(kTwoBytes);

	CHECK(reader.read(4) == 0xB);
	CHECK(reader.read(4) == 0xA);
}

TEST_CASE("a read spanning two bytes takes the first byte's high bits first") {
	BitReader reader(kTwoBytes);
	reader.offset = 4;

	// The high nibble of 0xAB followed by the low nibble of 0xCD.
	CHECK(reader.read(8) == 0xDA);
}

TEST_CASE("a multi-byte read places later bytes in the higher positions") {
	BitReader reader(kTwoBytes);

	CHECK(reader.read(16) == 0xCDAB);
}

TEST_CASE("readBool takes one bit and reports whether it was set") {
	BitReader reader(kTwoBytes);

	CHECK(reader.readBool() == true);	// bit 0 of 0xAB
	CHECK(reader.readBool() == true);	// bit 1
	CHECK(reader.readBool() == false);	// bit 2
	CHECK(reader.readBool() == true);	// bit 3
	CHECK(reader.offset == 4);
}

TEST_CASE("getBits reads without consuming") {
	BitReader reader(kTwoBytes);

	CHECK(reader.getBits(8) == 0xAB);
	CHECK(reader.offset == 0);
	CHECK(reader.getBits(8) == 0xAB);
	CHECK(reader.offset == 0);
}

TEST_CASE("getBit addresses bits across the whole buffer") {
	BitReader reader(kTwoBytes);

	CHECK(reader.getBit(0) == 1);
	CHECK(reader.getBit(1) == 1);
	CHECK(reader.getBit(2) == 0);
	CHECK(reader.getBit(3) == 1);
	// Bit 8 is the low bit of the second byte: 0xCD is 1100 1101.
	CHECK(reader.getBit(8) == 1);
	CHECK(reader.getBit(9) == 0);
}

TEST_CASE("a read of no bits yields zero and stays put") {
	BitReader reader(kTwoBytes);

	CHECK(reader.read(0) == 0);
	CHECK(reader.offset == 0);
}
