#pragma once
#include <string>
#include <vector>

// The string handling shared by the Info window's tabs.
namespace InfoText {
	std::string Join(const std::vector<std::string>& parts, const std::string& separator);
};
