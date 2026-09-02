#pragma once
#include <string>
#include <vector>
#include "Source.h"

class Table;

/*
 * Everything the game's tables say about the Horadric Cube's recipes, read once
 * and kept.
 *
 * Nothing here draws anything or knows what a window is, so the panel that
 * shows the recipes, the stat index and whatever else wants a recipe all read
 * the same sources and none of them reads CubeMain.txt for itself.
 *
 * Almost all of the work is wording. CubeMain.txt says what goes in and what
 * comes out in codes and qualifiers, and says nothing at all about what a
 * recipe should be called or which heading it belongs under, so both are read
 * out of the row. What the result is drawn in follows from its rarity, which is
 * the one thing about the colour a catalogue has any business knowing.
 */
namespace RecipeCatalogue {

	// What this catalogue calls itself where a kind of source is asked for, as
	// in a stat index entry or a query scoped to one kind.
	extern const char* const Kind;

	// Every recipe, ordered for a list. The order CubeMain.txt gives them is
	// kept, since it walks the cube from the quest recipes through the potions,
	// the gems and the runes to the crafting and the upgrades and sorting them
	// would break those chains apart. The file reaches the same kind of recipe
	// at several points, though, so the headings are gathered up: each keeps
	// the place the file first reaches it, and the recipes under it keep their
	// own order. Empty until the game data has loaded.
	const std::vector<Catalogue::Source>& Sources();

	// The recipe a code names, or NULL where the tables carry no such recipe.
	// A recipe's code is the description CubeMain.txt carries for it, that
	// being the only name the table gives a row. Valid until the game data is
	// reloaded.
	const Catalogue::Source* Find(const std::string& code);

	// Whether the tables have been read. Sources() is empty both before the
	// game data has loaded and where the tables carry no recipes at all, and
	// this is what tells the two apart.
	bool Loaded();

	// Reads one CubeMain table, in the order it holds its rows, with the two
	// affix tables a forced prefix or suffix is named by row in. The catalogue
	// reads the game's; a test reads a fixture. A realm's edited table is read
	// the same way, since what a row carries is all that decides whether it is
	// a recipe.
	std::vector<Catalogue::Source> Read(Table& recipes, Table& prefixes,
			Table& suffixes);

}
