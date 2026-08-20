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
 *
 * Properties are collected before they are rendered so that several sources of
 * the same stat can be added together first, the way the game adds them up on
 * the finished item.
 */
namespace StatDescriptions {
	enum StatKind {
		StatKindNormal = 0,
		StatKindDamage,		// a value against a ready made label
		StatKindPoison,		// poison damage, held per frame and shown as a total
		StatKindText		// a plain flag with no value, such as Indestructible
	};

	struct Stat {
		std::string id;		// identity used when adding equal stats together
		std::string stat;	// the ItemStatCost stat, empty for damage lines
		std::string param;
		std::string text;	// pre-resolved description, for damage and skill tabs
		int low;
		int high;
		int kind;
		bool perLevel;		// granted per character level, held in eighths
		bool percent;		// the value is a percentage

		Stat() : low(0), high(0), kind(StatKindNormal), perLevel(false),
			percent(false) {};
	};

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

	// Appends the stats one property entry grants. Entries the tables mark as
	// having no description of their own contribute nothing.
	void CollectProperty(const std::string& code, const std::string& param,
			int min, int max, std::vector<Stat>& stats);

	// Adds equal stats together in place, keeping the order they first appear.
	void MergeStats(std::vector<Stat>& stats);

	// The description line for one stat.
	std::string Render(const Stat& stat);
};
