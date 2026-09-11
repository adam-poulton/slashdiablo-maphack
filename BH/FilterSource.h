#pragma once
#include <string>
#include <vector>
#include "Config.h"

// Which file the item display rules are read from.
//
// The rules BH ships with live in BH.cfg. Anything else the player wants to use
// goes in a filters folder beside it, one .cfg per filter, named by its file name
// without the extension. BH.cfg is what is read whenever the chosen file cannot
// be, so there is always a filter in effect.
namespace FilterSource {
	// What the shipped filter is called, in the config and in the dropdown.
	extern const char* const Default;

	// The shipped filter first, then whatever the filters folder holds, in
	// alphabetical order. A selection naming a file that is not there is kept in
	// the list, so the dropdown shows what the config says even while the file is
	// missing.
	std::vector<std::string> Options(const std::string& selected);

	// Reads the named filter into the config, falling back to the shipped one if
	// it cannot be read or holds no rules. The selection is left alone either way:
	// a file that is missing today is not a reason to forget which one was asked
	// for. Returns whether the selection itself was read.
	bool Load(Config* config, const std::string& selected);
};
