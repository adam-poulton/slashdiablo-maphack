#include "CaptureFormat.h"
#include <cstdlib>

namespace {

const char* kHexDigits = "0123456789abcdef";

// The value of a single hex digit, or -1 for anything else.
int HexValue(char c) {
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

}  // namespace

namespace CaptureFormat {

std::string Escape(const std::string& value) {
	std::string out;
	out.reserve(value.length());
	for (std::size_t i = 0; i < value.length(); i++) {
		unsigned char c = (unsigned char)value[i];
		switch (c) {
		case '\\':
			out += "\\\\";
			break;
		case '\t':
			out += "\\t";
			break;
		case '\n':
			out += "\\n";
			break;
		case '\r':
			out += "\\r";
			break;
		default:
			// Anything outside printable ASCII goes out as a byte, which is how
			// the colour codes in an item's name are kept.
			if (c < 0x20 || c >= 0x7F) {
				out += "\\x";
				out += kHexDigits[c >> 4];
				out += kHexDigits[c & 0x0F];
			} else {
				out += (char)c;
			}
			break;
		}
	}
	return out;
}

std::string Unescape(const std::string& value) {
	std::string out;
	out.reserve(value.length());
	for (std::size_t i = 0; i < value.length(); i++) {
		if (value[i] != '\\') {
			out += value[i];
			continue;
		}

		// A backslash ending the text is not an escape and stands for itself.
		if (i + 1 >= value.length()) {
			out += '\\';
			break;
		}

		char marker = value[i + 1];
		if (marker == '\\') {
			out += '\\';
			i++;
		} else if (marker == 't') {
			out += '\t';
			i++;
		} else if (marker == 'n') {
			out += '\n';
			i++;
		} else if (marker == 'r') {
			out += '\r';
			i++;
		} else if (marker == 'x' && i + 3 < value.length() &&
				HexValue(value[i + 2]) >= 0 && HexValue(value[i + 3]) >= 0) {
			out += (char)(HexValue(value[i + 2]) * 16 + HexValue(value[i + 3]));
			i += 3;
		} else {
			// Not escaping this module wrote, so it is kept as it stands.
			out += '\\';
		}
	}
	return out;
}

void Record::Add(const std::string& key, const std::string& value) {
	out += "\t" + key + "=" + Escape(value);
}

void Record::Add(const std::string& key, long long value) {
	out += "\t" + key + "=" + std::to_string(value);
}

void Record::Add(const std::string& key, bool value) {
	out += "\t" + key + "=";
	out += value ? "1" : "0";
}

bool Fields::Has(const std::string& key) const {
	return values.find(key) != values.end();
}

std::string Fields::Text(const std::string& key) const {
	auto found = values.find(key);
	return (found == values.end()) ? std::string() : found->second;
}

long long Fields::Number(const std::string& key) const {
	auto found = values.find(key);
	if (found == values.end())
		return 0;
	return std::strtoll(found->second.c_str(), NULL, 10);
}

bool Fields::Boolean(const std::string& key) const {
	return Number(key) != 0;
}

Fields ParseLine(const std::string& line) {
	Fields fields;

	std::size_t start = 0;
	bool readType = false;
	while (start <= line.length()) {
		std::size_t end = line.find('\t', start);
		if (end == std::string::npos)
			end = line.length();
		std::string part = line.substr(start, end - start);
		start = end + 1;

		if (!readType) {
			// Leading whitespace is not part of a type, and a line holding only
			// whitespace has none at all.
			std::size_t first = part.find_first_not_of(" \r\n");
			fields.type = (first == std::string::npos) ?
				std::string() : part.substr(first);
			readType = true;
			continue;
		}

		// A field with no equals sign carries no value and is passed over. The
		// first equals sign ends the name, so a value may contain one.
		std::size_t split = part.find('=');
		if (split == std::string::npos)
			continue;
		fields.values[part.substr(0, split)] =
			Unescape(part.substr(split + 1));
	}

	return fields;
}

}  // namespace CaptureFormat
