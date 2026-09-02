#pragma once
#include <string>
#include <vector>
#include "Source.h"

class Table;

/*
 * Everything the game's tables say about set items, read once and kept.
 *
 * A set grants in two places, so it is read as two kinds of source. A piece is
 * one: what it grants on its own, and what it adds once enough of its set is
 * worn. Its set's own bonuses are the other, and they are a source in their own
 * right rather than a footnote on a piece, so that a stat which arrives only
 * with four pieces worn is found by a search that never names a piece.
 *
 * Why a set's bonuses are their own kind of source rather than a field on each
 * of its pieces, and what a criterion can see of a bonus a count of pieces
 * unlocks, is docs/adr/0006.
 *
 * Nothing here draws anything or knows what a window is, so the panel that
 * shows the set items, the stat index and whatever else wants a piece all read
 * the same sources and none of them reads SetItems.txt for itself.
 */
namespace SetCatalogue {

	// What this catalogue calls its two kinds of source where a kind is asked
	// for, as in a stat index entry or a query scoped to one kind.
	extern const char* const Kind;			// a piece of a set
	extern const char* const BonusKind;		// a set's own bonuses

	// Every piece, ordered for a list: by set, in the order the bonuses are
	// listed, and within a set in the order the table holds them, which is the
	// game's own head to toe order. A piece whose set the tables do not carry
	// comes last. Empty until the game data has loaded.
	const std::vector<Catalogue::Source>& Pieces();

	// Every set's own bonuses, one source a set, ordered by name.
	const std::vector<Catalogue::Source>& Bonuses();

	// The bonuses of the set a code names, or NULL where the tables carry no
	// such set. Valid until the game data is reloaded.
	const Catalogue::Source* FindBonus(const std::string& setCode);

	// Whether the tables have been read. Pieces() is empty both before the game
	// data has loaded and where the tables carry no set items at all, and this
	// is what tells the two apart.
	bool Loaded();

	// Reads one Sets table and one SetItems table, in the order they hold their
	// rows. The catalogue reads the game's; a test reads a fixture. A realm's
	// edited table is read the same way, since what a row carries is all that
	// decides what it grants.
	std::vector<Catalogue::Source> ReadBonuses(Table& table);
	std::vector<Catalogue::Source> ReadPieces(Table& table);

}
