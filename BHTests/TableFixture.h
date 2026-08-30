#pragma once

/*
 * Stands the game's data tables and its string table text up from fixtures, so
 * that the code reading them can be tested with no game running. Afterwards the
 * Tables statics and StatDescriptions hold what the game would have loaded out
 * of its MPQ archives.
 *
 * The fixtures are the game's own tables. The ones a property is looked up in
 * are held whole, because trimming one changes what the reading code can find
 * and a test passing against a table the game does not have is worth nothing.
 * The tables holding the items a test is about are trimmed to those items with
 * their header row kept. Both, and the string table text, are written by
 * tools/mpq/make_fixtures.py.
 */
namespace TableFixture {
	// Loads every fixture once. Later calls do nothing.
	void Load();
}
