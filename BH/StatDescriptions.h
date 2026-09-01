#pragma once
#include <iosfwd>
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
 * the finished item. They are then grouped and ordered the way the game does,
 * so a rendered set of lines reads as the item's own description does.
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
		int priority;		// descpriority, which is what orders the lines
		bool perLevel;		// granted per character level, held in eighths
		bool percent;		// the value is a percentage

		Stat() : low(0), high(0), kind(StatKindNormal), priority(0),
			perLevel(false), percent(false) {};
	};

	// Loads the string tables out of the MPQ archives. Safe to call repeatedly;
	// only the first call does any work. Requires the MPQ data tables to be
	// initialised first. Lives in StatDescriptionsFromMPQ.cpp, which is the only
	// part of this module that knows the archives exist.
	bool Initialize();

	// Whether any string table text is in, which is what a line needs to be
	// worded at all.
	bool IsInitialized();

	// Reads key and text pairs, one "key<TAB>text" to a line. This is how
	// anything without a game running supplies what Initialize would otherwise
	// read out of the archives.
	//
	// A key whose text carries a tab or a line ending cannot be written this
	// way; whatever produces the stream leaves those out.
	void LoadStrings(std::istream& stream);

	// Localised text for a string table key, or an empty string if unknown.
	std::string GetString(const std::string& key);

	// Localised name of a skill, given either its id or its internal name as
	// they appear in property parameters.
	std::string GetSkillName(const std::string& idOrName);

	// Appends the stats one property entry grants. Entries the tables mark as
	// having no description of their own contribute nothing.
	void CollectProperty(const std::string& code, const std::string& param,
			int min, int max, std::vector<Stat>& stats);

	// What a property adds to the stats it writes, with nothing said about how
	// it reads.
	struct StatTotal {
		std::string stat;
		int low;
		int high;

		StatTotal() : low(0), high(0) {};
	};

	// Appends what one property entry adds to each stat it writes.
	//
	// This asks a different question of the same table rows than
	// CollectProperty does, which is why it is a separate walk rather than a
	// second reading of the stats that one collects. Describing an item throws
	// away exactly what adding it up needs: stats the tables give no wording to
	// are dropped, a minimum and a maximum are folded into one line that no
	// longer names either, and the properties the game hardcodes are answered
	// from a stand in stat rather than from what they really grant.
	//
	// Two kinds of grant are left out, both because the tables hold them in
	// units no caller could compare against a number of points. Amounts granted
	// per character level are held in eighths in the parameter. Poison damage is
	// held per frame in 256ths and needs the duration to come to a total. Each
	// is a stat of its own, so a caller adding up any other stat never meets
	// one.
	//
	// The two halves of a damage line are each the whole of what they grant
	// rather than a range: "Adds 10-40 Fire Damage" writes exactly ten of the
	// minimum and exactly forty of the maximum.
	void CollectTotals(const std::string& code, const std::string& param,
			int min, int max, std::vector<StatTotal>& totals);

	// The range a stat comes to across a set of totals. Absent stats come back
	// as zero, which is what a caller wants: an item that does not grant it
	// changes nothing.
	//
	// Answers whether the stat was written at all, which is what tells a zero
	// granted apart from a stat nothing granted. A caller comparing the range
	// against a value has to know the difference; one adding it to a number
	// does not.
	bool TotalFor(const std::vector<StatTotal>& totals, const std::string& stat,
			int& low, int& high);

	// Adds equal stats together in place, keeping the order they first appear.
	void MergeStats(std::vector<Stat>& stats);

	// Folds the stats the game shows as a single line into that line: the four
	// resistances into "All Resistances", the four attributes into "to all
	// Attributes". Which stats group, and how the combined line reads, come from
	// the dgrp columns of ItemStatCost.txt rather than from a rule written here,
	// and a group only folds when the item grants every one of its stats at the
	// same value, which is the condition the game applies.
	void GroupStats(std::vector<Stat>& stats);

	// Puts the stats in the order an item shows them, which is the descpriority
	// ItemStatCost.txt gives each stat, highest first. Stats of equal priority
	// keep the order they were collected in.
	void SortStats(std::vector<Stat>& stats);

	// The description lines for a set of collected properties: added up, grouped,
	// ordered and rendered, which is what a caller wants unless it needs to step
	// in between. Lines a stat has no description for are left out.
	std::vector<std::string> BuildLines(std::vector<Stat> stats);

	// The description line for one stat.
	std::string Render(const Stat& stat);
};
