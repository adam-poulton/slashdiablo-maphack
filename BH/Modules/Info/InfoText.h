#pragma once
#include <string>
#include <vector>

// The string handling shared by the Info window's tabs. Case folding used to be
// here too; it went to Common once the settings window wanted it, being nothing
// to do with this window in particular. What the codes in the game's data tables
// are called is ItemDescription's to answer, not this.
namespace InfoText {
	std::string Join(const std::vector<std::string>& parts, const std::string& separator);
};
