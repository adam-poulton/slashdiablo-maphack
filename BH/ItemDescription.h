#pragma once
#include <string>
#include <vector>
#include "Constants.h"
#include "Drawing/Advanced/Tooltiphook/TooltipLine.h"
#include "StatDescriptions.h"

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

	// A low and a high roll, as the tables give them. A range whose ends are
	// equal is the one number it is, and one with no high end is nothing at all.
	//
	// low and high rather than min and max: windows.h leaves those defined as
	// macros, and a member named either of them does not compile.
	struct Range {
		int low;
		int high;

		Range() : low(0), high(0) {};
		Range(int low, int high) : low(low), high(high) {};
		bool Any() const { return high > 0; };
	};

	// What a character needs to use an item. Zero where nothing is required.
	//
	// Strength and dexterity are ranges because a requirement modifier can roll,
	// and a rolled one leaves the item asking for a range. The level is not: no
	// modifier moves it.
	struct Requirements {
		int level;
		Range strength;
		Range dexterity;

		Requirements() : level(0) {};
	};

	// Which of the three tiers a base belongs to. A base names the other two in
	// its upgrade columns, and the tier it is itself by pointing one of them at
	// its own code.
	enum Tier {
		// A source made on no one base is built in no tier: a runeword names a
		// range of bases and a set's own bonus names none.
		TierNone,

		TierNormal,
		TierExceptional,
		TierElite
	};

	// What the tables say about a base item, before anything is made of it.
	//
	// The numbers are kept as numbers rather than as the lines they will become,
	// because what is made of a base moves them: an item's own bonuses are
	// folded in before the lines are worded.
	struct Base {
		std::string code;			// "uap"
		std::string name;			// "Shako"
		std::string type;			// its ItemTypes.txt code, "helm"
		std::string typeName;		// what that type is called, "Helm"

		Tier tier;
		bool weapon;				// out of Weapons.txt, so speed means something
		bool spawnable;				// whether the game ever drops it at all
		int level;					// the level from which it starts dropping

		// The quest the item belongs to, and 0 for anything that is not a quest
		// item. Which quest is of no interest; that it is one is.
		int quest;

		// The codes of the same item a tier up, and empty where the tables name
		// none. A base has its own code in these columns where it has no upgrade,
		// which reads here as having none.
		std::string exceptional;	// ubercode
		std::string elite;			// ultracode

		Requirements requirements;

		// Whichever of these the base carries; a weapon has damage, armour has
		// defense, and the jewellery and charms have neither.
		Range defense;
		Range oneHandDamage;
		Range twoHandDamage;
		Range throwDamage;

		// Zero where the game shows none: a bow, which carries a durability it
		// never prints, or a throwing weapon, which gives the line over to its
		// stack instead.
		int durability;

		// Weapons.txt holds speed as a modifier rather than as a rate, so it
		// counts down: a weapon at -30 swings faster than one at 0. Zero for
		// anything that is not a weapon.
		int speed;

		// The most sockets the base can ever roll, and 0 for one that takes
		// none. What an item actually rolls is capped again by its own level;
		// this is the cap at the highest of those bands.
		int maxSockets;

		Base() : tier(TierNormal), weapon(false), spawnable(false), level(0),
			quest(0), durability(0), speed(0), maxSockets(0) {};
	};

	// Every base the tables carry, in the order they are read: the weapons, then
	// the armour, then the miscellany, each in its own table's order. Empty until
	// the game data has loaded.
	const std::vector<const Base*>& AllBases();

	// The base item a code names, or NULL where the tables do not carry it.
	// Valid until the game data is reloaded.
	const Base* FindBase(const std::string& code);

	// The line of a string table entry that is an item's name. A few items carry
	// how to use them in the same string as their name, and the game draws the
	// name last with the rest above it, so the last line is the name and the
	// lines in front of it are not. Anything drawing a name in one line wants
	// this rather than the whole entry.
	std::string NameLine(const std::string& text);

	// The name a base item goes by, falling back to the code itself. Weapons,
	// Armor and Misc all key their name into the string table, which is both how
	// a localised client reads and how a few of them come out right at all:
	// Misc.txt calls a small charm "Charm Small".
	std::string BaseName(const std::string& code);

	// What an item type code is called ("armo" -> "Any Armor"), from
	// ItemTypes.txt. Falls back to the code.
	std::string TypeName(const std::string& code);

	// What a tier is called. The game never writes one out anywhere, so the
	// words are ours, and a tier of none has none.
	std::string TierName(Tier tier);

	// What an item's own always-on properties do to the numbers its base carries.
	//
	// Per level bonuses are left out. They need a character level the panel does
	// not have, so they keep to their own stat line and the numbers read as the
	// game reads them on an item nobody is wearing.
	struct Modifiers {
		Range defensePercent;		// item_armor_percent
		Range defenseFlat;			// armorclass

		// Enhanced damage reaches every way a weapon can be swung, and so do the
		// plain minimum and maximum. The secondary and throw stats reach only
		// the two handed and the thrown line.
		Range damagePercent;
		Range damageMinFlat;
		Range damageMaxFlat;
		Range twoHandMinFlat;
		Range twoHandMaxFlat;
		Range throwMinFlat;
		Range throwMaxFlat;

		Range requirementPercent;	// item_req_percent, negative to lower
		bool indestructible;

		Modifiers() : indestructible(false) {};
	};

	// What a set of collected totals comes to. Stats the item grants that do not
	// bear on its base numbers are ignored here; they are described in their own
	// right by the stat lines.
	Modifiers ReadModifiers(const std::vector<StatDescriptions::StatTotal>& totals);

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

		// The base item's numbers, worded, with the item's own modifiers already
		// folded in. AddBase fills these.
		std::vector<std::string> attributes;

		Requirements requirements;
		std::vector<Section> sections;

		void AddTitle(const std::string& text, TextColor color);

		// The base item's name, numbers and requirements in one go, which is
		// what anything describing a made item wants from its base. Nothing is
		// added for a code the tables do not carry.
		//
		// The modifiers are the item's own, and are folded into the numbers the
		// way the game folds them. Each still describes itself in a stat line of
		// its own, which is also what the game does.
		//
		// What is made of a base can ask for a higher level than the base does,
		// so a caller is free to raise that afterwards. Strength and dexterity
		// are already answered here, since only a modifier moves them.
		void AddBase(const std::string& code, TextColor nameColor,
			const Modifiers& modifiers = Modifiers());

		// How fast the base swings and how many sockets it can be given, under
		// the numbers AddBase worded. Kept apart from AddBase because a made
		// item answers both for itself: a unique carries the sockets it was
		// given rather than the six its base could have rolled.
		void AddBaseLimits(const std::string& code);

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
