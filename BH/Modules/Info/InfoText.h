#pragma once
#include <string>
#include <vector>

// Small text helpers shared by the Info window's tabs: the naming lookups every
// panel needs to turn the codes in the game's data tables into something
// readable, and the string handling that goes with building a row out of them.
namespace InfoText {
	std::string ToLower(const std::string& text);
	std::string Join(const std::vector<std::string>& parts, const std::string& separator);

	// Readable name for an item type code ("armo" -> "Any Armor"), from
	// ItemTypes.txt. Falls back to the code itself.
	std::string ItemTypeName(const std::string& code);

	// Readable name for an item code ("7ws" -> "Legend Sword"), from the item
	// data already parsed out of the MPQ archives. Falls back to the code.
	std::string ItemName(const std::string& code);
};
