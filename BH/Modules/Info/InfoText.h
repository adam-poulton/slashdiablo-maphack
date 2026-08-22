#pragma once
#include <string>
#include <vector>

// The string handling shared by the Info window's tabs: folding a search key
// and building a row out of several pieces. What the codes in the game's data
// tables are called is ItemDescription's to answer, not this.
namespace InfoText {
	std::string ToLower(const std::string& text);
	std::string Join(const std::vector<std::string>& parts, const std::string& separator);
};
