#pragma once
#include <string>
#include <vector>
#include "../ItemDescription.h"
#include "../ItemRarity.h"
#include "../PropertyStats.h"

/*
 * One thing a catalogue holds: a unique, a piece of a set, a set's own bonus, a
 * runeword, a cube recipe, an affix. What they have in common is granting
 * stats, which is what lets the stat index hold them together without knowing
 * which catalogue an answer came out of.
 *
 * A source is what the game's tables say, not what a panel draws. Both the raw
 * property entries and the lines they are worded into are kept: a reader after
 * the numbers takes the properties, a reader after the sentences takes the
 * lines, and neither has to word or re-read anything for itself.
 */
namespace Catalogue {

	// Fields a source has no answer for are left as they start: a runeword
	// names no one base, and a set bonus asks for no level of its own.
	struct Source {
		// What the table it was read from calls it, which is what a lookup
		// names it by. Independent of the language the client reads in, unlike
		// the name.
		std::string code;

		// What the player is called it, out of the string table where there is
		// an entry for it.
		std::string name;

		std::string baseCode;		// the base item it is made on, "uap"
		std::string baseName;		// "Shako"
		std::string itemType;		// "Helm", from the base's item type

		// The set it belongs to, for a piece of one. The code is what the sets
		// table calls the set, which is what a piece names it by; the name is
		// what the player is called it.
		std::string setCode;
		std::string setName;

		// The level the source itself asks for, which can be higher than the
		// one its base asks for. Zero where it asks for none.
		int requiredLevel;

		// How the game draws it, which is also what a rule matching on rarity
		// would have to ask for.
		ItemRarity rarity;

		// What the source's own always-on properties do to the numbers its base
		// carries. Worked out where the properties are read, so that nothing
		// describing a source has to know which of them bear on its base.
		ItemDescription::Modifiers modifiers;

		// What it grants whenever it is worn, and what a count of set pieces
		// unlocks, each of the latter carrying that count. The two are kept
		// apart because the game keeps them apart: they are separate lines on
		// the item, and only the always-on ones move the base's numbers.
		std::vector<PropertyStats::Property> properties;
		std::vector<PropertyStats::Property> partial;

		std::vector<std::string> lines;
		std::vector<std::string> partialLines;

		Source() : requiredLevel(0), rarity(RarityNone) {};
	};

}
