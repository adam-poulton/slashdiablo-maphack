#include "ListRows.h"
#include <cctype>

namespace ListRows {

namespace {
	std::string Lower(std::string text) {
		for (char& c : text)
			c = (char)std::tolower((unsigned char)c);
		return text;
	}

	bool Has(const Row& row, const std::string& lowered) {
		for (const std::string& cell : row.cells) {
			if (Lower(cell).find(lowered) != std::string::npos)
				return true;
		}
		return false;
	}
}

Row Heading(const std::string& text) {
	return { { text }, true, false };
}

bool KeyRest(const std::string& key, const char* prefix, std::string& rest) {
	const std::string start = prefix;
	if (key.compare(0, start.size(), start) != 0)
		return false;
	rest = key.substr(start.size());
	return true;
}

std::vector<Row> Matching(const std::vector<Row>& rows, const std::string& text) {
	if (text.empty())
		return rows;
	const std::string lowered = Lower(text);

	std::vector<Row> matching;
	const Row* heading = NULL;		// the one the row in hand stands under
	bool headingShown = false;
	bool wholeSection = false;		// the heading itself has the text
	for (const Row& row : rows) {
		if (row.heading) {
			heading = &row;
			wholeSection = Has(row, lowered);
			headingShown = wholeSection;
			if (wholeSection)
				matching.push_back(row);
			continue;
		}
		if (row.summary || (!wholeSection && !Has(row, lowered)))
			continue;
		if (heading && !headingShown) {
			matching.push_back(*heading);
			headingShown = true;
		}
		matching.push_back(row);
	}
	return matching;
}

}
