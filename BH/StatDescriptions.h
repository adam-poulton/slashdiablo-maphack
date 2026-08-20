#pragma once
#include <string>
#include <vector>

/*
 * Turns the property entries in the game's data tables (a code, a parameter and
 * a min/max range, as found in Runes.txt, Gems.txt, UniqueItems.txt and so on)
 * into the description lines the game itself would show on an item.
 *
 * This follows the same path the game does:
 *   Properties.txt   code  -> the stats it grants, and how the values are used
 *   ItemStatCost.txt stat  -> descfunc/descval and the string keys to use
 *   *.tbl            key   -> the localised text
 */
namespace StatDescriptions {
	// Loads the string tables out of the MPQ archives. Safe to call repeatedly;
	// only the first call does any work. Requires the MPQ data tables to be
	// initialised first.
	bool Initialize();
	bool IsInitialized();

	// Localised text for a string table key, or an empty string if unknown.
	std::string GetString(const std::string& key);

	// Localised name of a skill, given either its id or its internal name as
	// they appear in property parameters.
	std::string GetSkillName(const std::string& idOrName);

	// Appends the description lines for one property entry. Entries the tables
	// mark as having no description of their own contribute nothing.
	void DescribeProperty(const std::string& code, const std::string& param,
			int min, int max, std::vector<std::string>& lines);
};
