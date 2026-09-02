#pragma once
#include <string>
#include <vector>
#include "../ItemDescription.h"
#include "Source.h"

/*
 * Every base item the game can drop before anything is made of it, read once
 * and kept.
 *
 * A base grants nothing, which makes it the one kind of source whose properties
 * are empty. It is in the catalogues anyway: an answer about a unique wants to
 * name the base under it, and a filter rule seeded from one starts at its base.
 * A source granting nothing simply answers no criterion about a stat.
 *
 * What the game shows about a base - its damage, its defense, its durability
 * and what a character needs to use it - stays in ItemDescription, which is
 * where the three tables it is spread across are read. A source names the base
 * so that all of it is one lookup away, rather than copying it.
 */
namespace BaseItemCatalogue {

	// What this catalogue calls itself where a kind of source is asked for, as
	// in a stat index entry or a query scoped to one kind.
	extern const char* const Kind;

	// Every base, ordered for a list: by item type in the order the game's
	// tables reach them, then by tier, then by the level it starts dropping
	// from. Empty until the game data has loaded.
	const std::vector<Catalogue::Source>& Sources();

	// The base a code names, or NULL where the tables carry no such base.
	// Valid until the game data is reloaded.
	const Catalogue::Source* Find(const std::string& code);

	// Whether the tables have been read. Sources() is empty both before the
	// game data has loaded and where the tables carry no bases at all, and this
	// is what tells the two apart.
	bool Loaded();

	// Reads a list of bases into sources, ordered for a list. The catalogue
	// reads the game's; a test reads a fixture's.
	std::vector<Catalogue::Source> Read(
			const std::vector<const ItemDescription::Base*>& bases);

}
