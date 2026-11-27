#include "ForgeCodec.h"
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include "../../Drawing/MapIcon/MapIcon.h"

namespace ForgeCodec {
	std::string Lower(std::string text) {
		for (char& c : text)
			c = (char)std::tolower((unsigned char)c);
		return text;
	}

	std::string Trimmed(const std::string& text) {
		size_t first = text.find_first_not_of(" \t\r\n");
		if (first == std::string::npos)
			return std::string();
		size_t last = text.find_last_not_of(" \t\r\n");
		return text.substr(first, last - first + 1);
	}

	Json EncodeColor(unsigned int color) {
		if (color == MapIcon::NoColor)
			return "none";
		char hex[8];
		sprintf_s(hex, "0x%02X", color % MapIcon::PaletteSize);
		return hex;
	}

	bool DecodeColor(const Json& stored, bool allowNone, unsigned int& color) {
		if (stored.is_number_unsigned()) {
			unsigned long long entry = stored.get<unsigned long long>();
			if (entry >= MapIcon::PaletteSize)
				return false;
			color = (unsigned int)entry;
			return true;
		}
		if (!stored.is_string())
			return false;
		std::string text = Lower(Trimmed(stored.get<std::string>()));
		if (text.compare("none") == 0) {
			if (!allowNone)
				return false;
			color = MapIcon::NoColor;
			return true;
		}
		if (text.empty() || text[0] == '-' || text[0] == '+')
			return false;
		char* end = NULL;
		unsigned long entry = std::strtoul(text.c_str(), &end, 0);
		if (*end != '\0' || entry >= MapIcon::PaletteSize)
			return false;
		color = (unsigned int)entry;
		return true;
	}

	bool DecodeCount(const Json& stored, unsigned int min, unsigned int max,
			unsigned int& count) {
		if (stored.is_number_unsigned()) {
			unsigned long long read = stored.get<unsigned long long>();
			count = read < min ? min : read > max ? max : (unsigned int)read;
			return true;
		}
		// Whole and not unsigned is below zero, and so below any range.
		if (stored.is_number_integer()) {
			count = min;
			return true;
		}
		return false;
	}

	Json EncodeIcon(unsigned int icon) {
		return MapIcon::Name(icon < MapIcon::IconCount ? (MapIcon::Icon)icon : MapIcon::None);
	}

	bool DecodeIcon(const Json& stored, unsigned int& icon) {
		if (!stored.is_string())
			return false;
		MapIcon::Icon parsed = MapIcon::None;
		if (!MapIcon::Parse(Trimmed(stored.get<std::string>()), &parsed))
			return false;
		icon = parsed;
		return true;
	}
}
