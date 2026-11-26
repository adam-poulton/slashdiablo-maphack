#include "ItemDrop.h"

namespace Agent {

namespace {

// Where the fields sit, in bits from the start of the packet and least
// significant bit of each byte first, as ItemFactsPacket reads them.
const unsigned char kItemAction = 0x9C;
const unsigned int kIdBit = 32;
const unsigned int kDestinationBit = 106;
const unsigned int kGroundXBit = 109;
const unsigned int kGroundYBit = 125;
const unsigned int kCoordinateBits = 16;
const unsigned int kGround = 3;

unsigned int ReadBits(const std::vector<unsigned char>& data, unsigned int at, unsigned int count) {
	unsigned int value = 0;
	for (unsigned int i = 0; i < count; i++) {
		unsigned int bit = at + i;
		if ((data[bit >> 3] >> (bit & 7)) & 1)
			value |= 1u << i;
	}
	return value;
}

void WriteBits(std::vector<unsigned char>& data, unsigned int at, unsigned int count, unsigned int value) {
	for (unsigned int i = 0; i < count; i++) {
		unsigned int bit = at + i;
		unsigned char mask = (unsigned char)(1u << (bit & 7));
		if ((value >> i) & 1)
			data[bit >> 3] |= mask;
		else
			data[bit >> 3] &= (unsigned char)~mask;
	}
}

int HexDigit(char c) {
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

}  // namespace

std::string PlaceDrop(std::vector<unsigned char>& packet, unsigned int id,
		unsigned int x, unsigned int y) {
	if (packet.size() * 8 < kGroundYBit + kCoordinateBits)
		return "too short to be an item on the ground";
	if (packet[0] != kItemAction)
		return "not an item packet (0x9C)";
	if (packet[2] != packet.size())
		return "its length byte says " + std::to_string(packet[2]) + " bytes, not " +
			std::to_string(packet.size());
	if (ReadBits(packet, kDestinationBit, 3) != kGround)
		return "not an item on the ground";
	if (x >> kCoordinateBits || y >> kCoordinateBits)
		return "a place on the ground is two 16-bit coordinates";

	WriteBits(packet, kIdBit, 32, id);
	WriteBits(packet, kGroundXBit, kCoordinateBits, x);
	WriteBits(packet, kGroundYBit, kCoordinateBits, y);
	return "";
}

bool BytesFromHex(const std::string& hex, std::vector<unsigned char>& bytes) {
	if (hex.empty() || hex.size() % 2)
		return false;
	bytes.clear();
	for (size_t i = 0; i < hex.size(); i += 2) {
		int high = HexDigit(hex[i]);
		int low = HexDigit(hex[i + 1]);
		if (high < 0 || low < 0)
			return false;
		bytes.push_back((unsigned char)(high * 16 + low));
	}
	return true;
}

}
