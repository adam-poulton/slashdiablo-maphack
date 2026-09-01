#pragma once

/*
 * The one place that knows which catalogues there are.
 *
 * The stat index answers questions about sources without knowing what kind of
 * thing any of them is, which only holds if something else does the introducing.
 * That is this: every catalogue is read and registered here, and nowhere else
 * has to name them all.
 *
 * Adding a catalogue is one line in Load() and no change to the index or to
 * anything that queries it.
 */
namespace Catalogue {

	// Reads every catalogue and registers its sources into the stat index.
	// Later calls do nothing.
	//
	// Called by whatever read the game's data tables, on the thread that read
	// them rather than the drawing one, since the index has to hold what every
	// source grants and that cannot be worked out a row at a time. A call made
	// before the tables are in registers nothing and leaves the catalogues
	// unloaded, so the caller is the one place that knows they are ready.
	void Load();

	// Whether the catalogues have been read and registered. Tells an index
	// holding nothing yet from one whose catalogues carry no sources.
	bool Loaded();

}
