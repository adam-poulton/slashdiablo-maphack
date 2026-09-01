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
	// Called once the game's data tables are in, off the drawing thread, since
	// the index has to hold what every source grants and cannot be worked out a
	// row at a time. Safe to call before the tables are in, in which case it
	// does nothing and the next call tries again.
	void Load();

	// Whether the catalogues have been read and registered. Tells an index
	// holding nothing yet from one whose catalogues carry no sources.
	bool Loaded();

}
