#pragma once
#include <string>
#include <vector>
#include "Source.h"

class Table;

/*
 * Everything the game's tables say about unique items, read once and kept.
 *
 * Nothing here draws anything or knows what a window is, so the panel that
 * shows the uniques, the stat index and whatever else wants a unique all read
 * the same sources and none of them reads UniqueItems.txt for itself.
 */
namespace UniqueCatalogue {

	// What this catalogue calls itself where a kind of source is asked for, as
	// in a stat index entry or a query scoped to one kind.
	extern const char* const Kind;

	// Every unique, ordered for a list: by name, and by the base under it where
	// several uniques share a name, as the facet jewels do. Empty until the
	// game data has loaded.
	const std::vector<Catalogue::Source>& Sources();

	// The unique a code names, or NULL where the tables carry no such unique.
	// Valid until the game data is reloaded.
	const Catalogue::Source* Find(const std::string& code);

	// Whether the tables have been read. Sources() is empty both before the
	// game data has loaded and where the tables carry no uniques at all, and
	// this is what tells the two apart.
	bool Loaded();

	// Reads one UniqueItems table, in the order it holds its rows. The
	// catalogue reads the game's; a test reads a fixture. A realm's edited
	// table is read the same way, since what a row carries is all that decides
	// whether it is a unique.
	std::vector<Catalogue::Source> Read(Table& table);

}
