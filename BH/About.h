#pragma once
#include <string>
#include <vector>

// What BH is: its own version, the game's, and whether cGuard is loaded.
namespace About {
	// Just the version.
	std::string Version();

	// The version without the "BH" in front of it, for wherever the name is
	// already on screen.
	std::string VersionNumber();

	// The release on its own, without the build it was cut from. What a user
	// answers "which version?" with; the build is for a bug report.
	std::string ReleaseNumber();

	// Which build of BH this is.
	std::string Branch();

	// Everything worth quoting when reporting a problem, a line at a time.
	std::vector<std::string> Lines();
};
