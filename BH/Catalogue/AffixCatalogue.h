#pragma once
#include <vector>
#include "Source.h"

class Table;

/*
 * Every magic prefix and suffix the game can roll, read once and kept.
 *
 * The affixes are two kinds of source rather than one, because a player looking
 * for what grants a stat wants to know whether the answer goes in front of the
 * item's name or behind it. Both are read the same way: MagicPrefix.txt and
 * MagicSuffix.txt carry the same columns, so one reader answers for both and
 * the kind is decided by which table was handed to it.
 *
 * There is no panel for the affixes. This catalogue exists to be searched: the
 * stat index answers for a prefix and a suffix under the same criteria it
 * answers for a unique, knowing nothing of any of the three.
 *
 * An affix name is not an identity: the tables spell "Serpent's" out on eight
 * rows, one per band of levels and kinds of base it rolls in, and each is its
 * own affix with its own range. So there is no lookup by code here, and each
 * row a table spawns is one source.
 */
namespace AffixCatalogue {

	// What this catalogue calls itself where a kind of source is asked for, as
	// in a stat index entry or a query scoped to one kind. Two words for two
	// kinds, since neither is the other's variation.
	extern const char* const PrefixKind;
	extern const char* const SuffixKind;

	// Every prefix and every suffix, in the order their tables reach them,
	// which steps up through the levels within each group of affixes that
	// grant the same thing. Empty until the game data has loaded.
	const std::vector<Catalogue::Source>& Prefixes();
	const std::vector<Catalogue::Source>& Suffixes();

	// Whether the tables have been read. Prefixes() and Suffixes() are empty
	// both before the game data has loaded and where the tables carry no
	// affixes at all, and this is what tells the two apart.
	bool Loaded();

	// Reads one affix table into sources, in table order. The catalogue reads
	// the game's two; a test reads a fixture. Nothing in a row says whether it
	// is a prefix or a suffix, so the caller is what decides that.
	std::vector<Catalogue::Source> Read(Table& table);

	// One affix, by the row it sits on in its table, or false where the row
	// carries no affix for want of a name or of anything to grant. This is the
	// one path from a row to what it grants and to what the player reads for
	// it, and it is addressed by row because CubeMain.txt names a forced prefix
	// or suffix that way. Read() leaves out the rows the game never rolls, so
	// its own order says nothing about where a row sits.
	bool ReadRow(Table& table, int row, Catalogue::Source& source);

}
