#pragma once
#include <string>
#include "ForgeStore.h"

// The file the Forge's styles are kept in, beside BH_settings.cfg, and the only
// thing that writes it.
//
// Several clients of one install can run at once, each with its own values, so
// a save is made onto what the file says at that moment, under a lock every
// client takes, and changes only the markers this client changed.
class ForgeFile {
	private:
		std::string path;
		std::string name;		// the file's name without its folder, for saying so
		ForgeStore store;
		std::string problem;

	public:
		explicit ForgeFile(const std::string& path);

		ForgeStore& GetStore() { return store; }

		// Reads the file into the store, before the modules bind their values
		// to it. A file that is not there is every marker at its default.
		bool Load();

		// Writes what changed. Nothing is written where nothing changed, and
		// nothing over a file that has stopped being the file's shape, which is
		// a player's own edit to be fixed rather than lost.
		bool Save();

		// What stops the file being saved, said for the window it is edited in.
		// Empty while nothing does.
		const std::string& GetProblem() const { return problem; }
};
