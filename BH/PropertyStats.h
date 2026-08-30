#pragma once
#include <string>
#include <vector>
#include "StatDescriptions.h"

/*
 * The one path from the property entries the game's tables hold to what a
 * player reads and to what those properties add up to.
 *
 * Every catalogue's properties come through here, so that a wording fix reaches
 * uniques, set items, runewords and cube recipes at once and so that nothing
 * reading the totals has to know which table they were read out of.
 *
 * How a line is worded is StatDescriptions' business. This is where a list of
 * properties becomes one, which is the step every reader of the tables was
 * writing out for itself.
 */
namespace PropertyStats {
	// A property entry as it appears in the game's tables: what it grants, the
	// parameter that says which of it, and the range it rolls in. itemCount is
	// how many pieces of a set have to be worn for it to apply, and 0 wherever
	// that does not arise.
	//
	// Defaults are declared inline rather than in a constructor: windows.h
	// leaves min and max defined as macros, and an initializer list naming them
	// does not compile.
	struct Property {
		std::string code;
		std::string param;
		int min = 0;
		int max = 0;
		int itemCount = 0;
	};

	// The description lines a list of properties comes to, added up, grouped and
	// ordered the way the game shows them. Properties the tables give no wording
	// to contribute nothing.
	std::vector<std::string> Lines(const std::vector<Property>& properties);

	// The same, followed by bonuses that arrive as ready made text: what the
	// tables can word, and then what they cannot.
	std::vector<std::string> Lines(const std::vector<Property>& properties,
			const std::vector<std::string>& extraLines);

	// The lines a set of counted properties comes to, a count at a time and in
	// ascending order of it, each already carrying the count that unlocks it.
	//
	// Rendered a count at a time rather than all at once because a count is
	// its own line on the item: one pass would add a piece's "+10 to Strength
	// (2 Items)" to its "+20 to Strength (4 Items)" and show a single +30.
	// Properties no count unlocks are not part of this and are left out.
	std::vector<std::string> CountedLines(const std::vector<Property>& properties);

	// What a list of properties adds to each stat it writes, for whatever reads
	// the numbers rather than the words.
	std::vector<StatDescriptions::StatTotal> Totals(
			const std::vector<Property>& properties);
};
