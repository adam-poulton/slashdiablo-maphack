#pragma once
#include <string>
#include <vector>

struct ItemAttributes;
struct UnitAny;

/*
 * What the item filter knows about an item.
 *
 * An item reaches the filter in one of two ways, and each brings its own shape.
 * An item already in the world is a game unit, and UnitItemInfo is little more
 * than a pointer to it: the rest is read from the game as each condition asks
 * for it. An item arriving in a packet is not a unit yet, so everything about
 * it is read out of the packet in one go and kept in ItemFacts.
 *
 * The two are not equivalent, and the conditions carry two implementations
 * apiece because of it. ItemFacts is the shape both are heading towards.
 *
 * Nothing here reaches into the game, so what depends only on this header can
 * be built and tested without one.
 */

// A property an item carries, as one is written into a packet.
struct ItemProperty {
	unsigned int stat;
	long value;

	unsigned int minimum;
	unsigned int maximum;
	unsigned int length;

	unsigned int level;
	unsigned int characterClass;
	unsigned int skill;
	unsigned int tab;

	unsigned int monster;

	unsigned int charges;
	unsigned int maximumCharges;

	unsigned int skillChance;

	unsigned int perLevel;
};

// An item that already exists in the world, which is to say a game unit.
struct UnitItemInfo {
	UnitAny *item;
	char itemCode[4];
	ItemAttributes *attrs;
};

// An item as an incoming 0x9c packet described it.
struct ItemFacts {
	ItemAttributes *attrs;
	char code[4];
	std::string name;
	std::string earName;
	std::string personalizedName;
	unsigned int id;
	unsigned int x;
	unsigned int y;
	unsigned int amount;
	unsigned int prefix;
	unsigned int suffix;
	unsigned int setCode;
	unsigned int uniqueCode;
	unsigned int runewordId;
	unsigned int defense;
	unsigned int action;
	unsigned int category;
	unsigned int version;
	unsigned int directory;
	unsigned int container;
	unsigned int earLevel;
	unsigned int width;
	unsigned int height;
	unsigned int quality;
	unsigned int graphic;
	unsigned int color;
	unsigned int superiority;
	unsigned int runewordParameter;
	unsigned int maxDurability;
	unsigned int durability;
	unsigned char usedSockets;
	unsigned char level;
	unsigned char earClass;
	unsigned char sockets;
	bool equipped;
	bool inSocket;
	bool identified;
	bool switchedIn;
	bool switchedOut;
	bool broken;
	bool potion;
	bool hasSockets;
	bool inStore;
	bool notInSocket;
	bool ear;
	bool startItem;
	bool simpleItem;
	bool ethereal;
	bool personalized;
	bool gambling;
	bool runeword;
	bool ground;
	bool unspecifiedDirectory;
	bool isGold;
	bool hasGraphic;
	bool hasColor;
	bool isArmor;
	bool isWeapon;
	bool indestructible;
	std::vector<unsigned long> prefixes;
	std::vector<unsigned long> suffixes;
	std::vector<ItemProperty> properties;
};
