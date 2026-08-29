#pragma once
#include <string>

/*
 * Rows of the game's own data tables, as the parts of BH that read them need
 * them.
 *
 * These are records and nothing more. They are kept apart from the code that
 * fills them because loading a table means the archives, and the archives mean
 * the game, while reading an item packet needs only the widths and attributes
 * these carry. Anything depending on this header alone can be built and tested
 * with neither.
 */

// Item attributes from ItemTypes.txt and Weapon/Armor/Misc.txt
struct ItemAttributes {
	std::string name;
	char code[4];
	std::string category;
	unsigned char width;
	unsigned char height;
	unsigned char stackable;
	unsigned char useable;
	unsigned char throwable;
	unsigned char itemLevel;	// 1=normal, 2=exceptional, 3=elite
	unsigned char unusedFlags;
	unsigned int flags;
	unsigned int flags2;
	unsigned char qualityLevel;
	unsigned char magicLevel;
};

// Properties from ItemStatCost.txt that we need for parsing incoming 0x9c
// packets, among other things
struct StatProperties {
	std::string name;
	unsigned char saveBits;
	unsigned char saveParamBits;
	unsigned char saveAdd;
	unsigned char op;
	unsigned char sendParamBits;
	unsigned short ID;
};

struct CharStats {
	int toHitFactor;
};
