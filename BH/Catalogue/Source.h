#pragma once
#include <string>
#include <vector>
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

		// The level the source itself asks for, which can be higher than the
		// one its base asks for. Zero where it asks for none.
		int requiredLevel;

		// How the game draws it, which is also what a rule matching on rarity
		// would have to ask for.
		ItemRarity rarity;

		std::vector<PropertyStats::Property> properties;
		std::vector<std::string> lines;

		Source() : requiredLevel(0), rarity(RarityNone) {};
	};

}
