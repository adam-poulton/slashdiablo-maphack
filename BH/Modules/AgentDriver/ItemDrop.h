#pragma once
#include <string>
#include <vector>

namespace Agent {

// Rewrites an item packet the server sent as an item landed on the ground
// (0x9C), recorded in one game, to land in another: as a unit of its own, so it
// cannot be taken for an item already in the world, and at a place on the ground
// in map coordinates. Returns what is wrong with the packet, or nothing once it
// has been rewritten.
std::string PlaceDrop(std::vector<unsigned char>& packet, unsigned int id,
		unsigned int x, unsigned int y);

// The bytes a string of hex digits spells, two to a byte. False, with the bytes
// unspecified, for anything else.
bool BytesFromHex(const std::string& hex, std::vector<unsigned char>& bytes);

}
