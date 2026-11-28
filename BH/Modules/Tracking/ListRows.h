#pragma once
#include <string>
#include <vector>

// What a tab lists, as rows of text worked out without the game. A tab lays
// them out and draws them.
namespace ListRows {

struct Row {
	std::vector<std::string> cells;
	bool heading;		// names the rows under it
	bool summary;		// says something of the rows around it, and is no one of them

	// A text colour for each cell that has one of its own, as the game numbers
	// them, and -1 for a cell that takes its column's. Shorter than cells, or
	// empty, where the rest take their column's.
	std::vector<int> colors;

	// What the row stands for, where a tab does something when it is clicked.
	std::string key;
};

Row Heading(const std::string& text);

// The rest of a key that starts as given. False where it starts otherwise.
bool KeyRest(const std::string& key, const char* prefix, std::string& rest);

// The rows that have the text in them, whatever its case, under the headings
// they stood under. A heading that has it keeps every row under it. A summary
// goes: it is of rows that are no longer all there.
std::vector<Row> Matching(const std::vector<Row>& rows, const std::string& text);

}
