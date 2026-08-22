#pragma once
#include <string>
#include <vector>
#include "Constants.h"
#include "Drawing.h"

/*
 * Builds the panel of text that describes an item, in the order and the wording
 * the game itself uses: the names at the top, then what the base item is worth,
 * then what a character needs to use it, then the bonuses it carries.
 *
 * StatDescriptions turns the property entries in the data tables into stat
 * lines. This puts those lines into an item's shape, so that every panel
 * describing an item reads the same however it was built, and so that surfacing
 * a new kind of item is a matter of filling in a Description rather than
 * repeating the layout.
 *
 * Everything shown comes from the game's own tables and string tables, down to
 * the labels, so a localised client reads in its own language and a realm that
 * has edited a table is described by what it edited it to.
 */
namespace ItemDescription {

	// What a character needs to use an item. Zero where nothing is required.
	struct Requirements {
		int level;
		int strength;
		int dexterity;

		Requirements() : level(0), strength(0), dexterity(0) {};
	};

	// What the tables say about a base item, before anything is made of it.
	struct Base {
		std::string code;			// "uap"
		std::string name;			// "Shako"
		std::string type;			// its ItemTypes.txt code, "helm"
		std::string typeName;		// what that type is called, "Helm"
		Requirements requirements;

		// The numbers the game prints under an item's name: its defense, its
		// damage and its durability, worded as the game words them. Empty for
		// the jewellery and charms that have none of them.
		std::vector<std::string> attributes;
	};

	// The base item a code names, or NULL where the tables do not carry it.
	// Valid until the game data is reloaded.
	const Base* FindBase(const std::string& code);

	// The name a base item goes by, falling back to the code itself. Weapons,
	// Armor and Misc all key their name into the string table, which is both how
	// a localised client reads and how a few of them come out right at all:
	// Misc.txt calls a small charm "Charm Small".
	std::string BaseName(const std::string& code);

	// What an item type code is called ("armo" -> "Any Armor"), from
	// ItemTypes.txt. Falls back to the code.
	std::string TypeName(const std::string& code);

	// One block of stat lines drawn in a single colour, under an optional
	// heading. Blocks stay separate because the game keeps them separate: an
	// item's own bonuses and its set's are not one list.
	struct Section {
		std::string heading;
		TextColor headingColor;
		std::vector<std::string> lines;
		TextColor color;
		bool spaced;			// a blank line above it, to break the panel up

		Section() : headingColor(Gold), color(Blue), spaced(false) {};
	};

	// An item to describe. Titles are the lines above the rest: the item's name,
	// and whatever else names it, such as its base item.
	struct Description {
		std::vector<Drawing::TooltipLine> titles;

		// The base item's own numbers, as Base::attributes gives them.
		std::vector<std::string> attributes;

		Requirements requirements;
		std::vector<Section> sections;

		void AddTitle(const std::string& text, TextColor color);

		// The base item's name, numbers and requirements in one go, which is
		// what anything describing a made item wants from its base. Nothing is
		// added for a code the tables do not carry.
		//
		// What is made of a base can ask for more than the base does, so a
		// caller is free to raise the requirements afterwards.
		void AddBase(const std::string& code, TextColor nameColor);

		// A block of stat lines with no heading of its own.
		void AddStats(const std::vector<std::string>& lines,
			TextColor color, bool spaced = false);

		// A block under a heading, such as a set's "Complete Set Bonus".
		void AddSection(const std::string& heading,
			TextColor headingColor,
			const std::vector<std::string>& lines, TextColor color,
			bool spaced = false);
	};

	// A recipe: what it makes, what it can be made on, and what it is made from.
	// The runewords panel is one of these; a cube recipe is the same shape.
	//
	// A recipe names a range of bases rather than one, so it carries no base
	// item numbers: what it is worth depends on what it is made on.
	struct Recipe {
		std::string name;
		TextColor nameColor;
		std::string appliesTo;		// "Any Armor", the bases it can be made on
		std::string ingredients;	// "Jah + Ith + Ber"
		TextColor ingredientColor;
		Requirements requirements;
		std::vector<Section> sections;

		Recipe() : nameColor(White), ingredientColor(White) {};

		void AddStats(const std::vector<std::string>& lines,
			TextColor color, bool spaced = false);
	};

	// The finished lines, ready to hand to a Tooltiphook.
	std::vector<Drawing::TooltipLine> Build(const Description& item);
	std::vector<Drawing::TooltipLine> Build(const Recipe& recipe);
};
