#pragma once
#include <string>
#include <vector>
#include "Source.h"

class Table;

/*
 * Everything the game's tables say about runewords, read once and kept.
 *
 * A runeword is not made on one base, so it is the first catalogue whose
 * sources carry variants: gems.txt gives every rune one set of bonuses in a
 * weapon, another in a helm or body armour and a third in a shield, so the same
 * runeword rolls differently depending on what it is made in. Which of the
 * three a type of base belongs to is settled by walking the Equiv chain in
 * ItemTypes.txt up to the root categories.
 *
 * Nothing here draws anything or knows what a window is, so the panel that
 * shows the runewords, the stat index and whatever else wants a runeword all
 * read the same sources and none of them reads runes.txt for itself.
 */
namespace RunewordCatalogue {

	// What this catalogue calls itself where a kind of source is asked for, as
	// in a stat index entry or a query scoped to one kind.
	extern const char* const Kind;

	// Every runeword, ordered by name for a list. Empty until the game data has
	// loaded.
	const std::vector<Catalogue::Source>& Sources();

	// The runeword a code names, or NULL where the tables carry no such
	// runeword. Valid until the game data is reloaded.
	const Catalogue::Source* Find(const std::string& code);

	// Whether the tables have been read. Sources() is empty both before the
	// game data has loaded and where the tables carry no runewords at all, and
	// this is what tells the two apart.
	bool Loaded();

	// Reads one runes table and the recipes a realm enables without shipping
	// them in it, ordered by name for a list. The catalogue reads the game's
	// table; a test reads a fixture. A realm's edited table is read the same
	// way, since what a row carries is all that decides whether it is a
	// runeword.
	std::vector<Catalogue::Source> Read(Table& table);

}
