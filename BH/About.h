#pragma once
#include <string>
#include <vector>

// What BH is: its own version, the game's, and whether cGuard is loaded.
namespace About {
	// Just the version.
	std::string Version();

	// Which build of BH this is.
	std::string Branch();

	// Everything worth quoting when reporting a problem, a line at a time.
	std::vector<std::string> Lines();
};
