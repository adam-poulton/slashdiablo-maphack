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

	// The same source made on a different kind of base. Where a source can be
	// made on a range of bases rather than on one, what it grants differs by
	// kind: each rune of a runeword gives one set of bonuses in a weapon,
	// another in a helm or body armour, and a third in a shield.
	struct Variant {
		// The kind of base, named as gems.txt names the columns of rune
		// bonuses it gives each: "weapon", "helm", "shield".
		std::string baseKind;

		// What the bases of that kind are called where the source lists them,
		// "Any Armor", which is how a line only this kind grants is labelled.
		std::string label;

		// The source's own properties and the kind's together, so a variant is
		// the whole of what the source grants on a base of that kind.
		std::vector<PropertyStats::Property> properties;
		std::vector<std::string> lines;
	};

	// Fields a source has no answer for are left as they start: a runeword
	// names no one base, a set bonus asks for no level of its own, and a unique
	// is made from nothing and sits under no heading.
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
		// What kind of base it is made on: "Helm" from the base's item type
		// where it names one base, and every type it is allowed in where it
		// names a range of them, down to the ones it is not allowed in:
		// "Sword, Axe, Mace" or "Any Weapon (not Bow, Crossbow)".
		std::string itemType;

		// The set it belongs to, for a piece of one. The code is what the sets
		// table calls the set, which is what a piece names it by; the name is
		// what the player is called it.
		std::string setCode;
		std::string setName;

		// Which of the three the base under it is built in, and none for a
		// source made on no one base.
		ItemDescription::Tier tier;

		// The level the source itself asks for, which can be higher than the
		// one its base asks for. Zero where it asks for none.
		int requiredLevel;

		// The level from which the game starts dropping it. What a character
		// needs to use it is requiredLevel; this is where it begins to appear.
		int level;

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

		// What the source is made from, in the order the sockets take it: the
		// rune codes of a runeword. Empty for anything made from nothing.
		std::vector<std::string> ingredientCodes;

		// What those are called, as the game lists a recipe: "Jah + Ith + Ber",
		// "3 Flawless Ruby". Empty for anything made from nothing.
		std::string ingredients;

		// Every kind of base the source can be made on and what it grants in
		// each. Empty for a source made on one base, where the properties are
		// the whole of what it grants.
		std::vector<Variant> variants;

		// The heading it sits under where its catalogue gathers its sources up,
		// as "Gem" gathers the recipes that make one. Empty where a catalogue
		// lists its sources flat, and where what gathers them is a thing in its
		// own right instead, as a set is for its pieces.
		std::string heading;

		// What it does beyond granting the stats its lines word: the sockets it
		// adds, the levels it costs, the conditions it is only allowed under.
		// Said in words because the game says them in the shape of the item
		// instead, so there is no stat line for them to be part of.
		std::vector<std::string> notes;

		Source() : tier(ItemDescription::TierNone), requiredLevel(0), level(0),
			rarity(RarityNone) {};
	};

}
