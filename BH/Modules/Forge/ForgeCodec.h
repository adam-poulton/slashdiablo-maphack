#pragma once
#include <string>
#include "nlohmann/json.hpp"

// How the parts of a marker's style are written in the Forge's file, for
// whatever writes one there: a marker's own fields, and a list that holds styles.
namespace ForgeCodec {
	// Ordered, so fields are written in the order they are given.
	typedef nlohmann::ordered_json Json;

	std::string Lower(std::string text);
	std::string Trimmed(const std::string& text);

	// A palette entry, written "0x97", or "none" for no colour.
	Json EncodeColor(unsigned int color);

	// A palette entry as a number, or as text in hex or decimal. No colour is
	// "none", where the colour can be none.
	bool DecodeColor(const Json& stored, bool allowNone, unsigned int& color);

	// A whole number held to a range: one past it is the nearest end.
	bool DecodeCount(const Json& stored, unsigned int min, unsigned int max,
		unsigned int& count);

	// An icon by its config name, whatever its case.
	Json EncodeIcon(unsigned int icon);
	bool DecodeIcon(const Json& stored, unsigned int& icon);
}
