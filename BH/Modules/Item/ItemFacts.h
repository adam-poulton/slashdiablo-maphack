#pragma once
#include <string>
#include <vector>

struct ItemAttributes;
struct ItemFacts;
struct UnitAny;

/*
 * What the item filter knows about an item.
 *
 * An item reaches the filter in one of three ways, and each brings its own
 * shape. An item already in the world is a game unit, and UnitItemInfo is
 * little more than a pointer to it: the rest is read from the game as each
 * condition asks for it. An item arriving in a packet is not a unit yet, so
 * everything about it is read out of the packet in one go and kept in
 * ItemFacts. A catalogue source is neither: it is said as ItemFacts from the
 * game's tables alone, so that a rule can be walked against an item nobody has
 * dropped, which ADR 0004 records.
 *
 * The three are not equivalent, and the conditions reading a game unit carry an
 * implementation of their own because of it. ItemFacts is the shape they are
 * heading towards.
 *
 * Nothing here reaches into the game, so what depends only on this header can
 * be built and tested without one.
 */

// One entry of an item's stat list, as the game stores it.
struct StatEntry {
	unsigned short stat;
	// What the stat is about where it needs saying: which skill, which class,
	// which skill tab. Several stats pack more than one thing into it.
	unsigned short sub;
	int value;
};

/*
 * An item's stats, asked for in the two ways the conditions ask for them.
 *
 * Both are here because the game itself answers both, and not always with the
 * same number. A condition that compares a total asks for one; a condition that
 * needs to look at how a stat was stored, such as the level a charged skill is
 * held at, reads the entries. Answering the first by adding up the second would
 * be tidier and is not obviously the same thing, so the choice is left where it
 * already was. This is what ADR 0001 records.
 */
struct StatSource {
	virtual ~StatSource() {}

	// A stat's value, added up the way the game adds it. sub selects among the
	// stats that carry more than one value.
	virtual int Stat(unsigned int stat, unsigned int sub) const = 0;

	// The entries as stored, for the conditions that read a sub index rather
	// than compare a total.
	virtual const std::vector<StatEntry>& Stats() const = 0;
};

/*
 * What an item has to exist to be asked.
 *
 * A packet describes an item; it does not create one. These two questions are
 * put to the game about a thing that is already there, so an item still on its
 * way cannot answer them at all. Rather than answering wrongly, it says so, and
 * ADR 0002 has what happens then: the rule that wanted to know does not match.
 */
struct LiveOnlyFacts {
	virtual ~LiveOnlyFacts() {}

	// What a vendor would pay, which depends on the difficulty being played.
	virtual unsigned int Price(unsigned int difficulty) const = 0;

	// The lowest level any class needs to use the item.
	virtual unsigned int RequiredLevel() const = 0;
};

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

	// The same item said the way an item from a packet says it. Conditions that
	// have been collapsed onto one body read this and never the unit, which is
	// how they came to have one body. Filled by whoever built this.
	const ItemFacts *facts;
};

// Everything a rule reads about an item, whichever of the three ways it
// reached the filter. Shaped after what an incoming 0x9c packet describes,
// which is the one of the three that says the most in one go.
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

	// The item's stats. Not held by value because what answers depends on where
	// the item came from: an item in a packet answers out of the properties
	// above, an item in the world answers by asking the game, and a catalogue
	// source answers out of the property entries the tables hold for it.
	const StatSource *stats;

	/*
	 * What can only be asked of an item that exists, or null for an item a
	 * packet has only just described.
	 *
	 * Asked for rather than worked out in advance because both are dear: a
	 * price is a question put to the game, and the level an item takes to use
	 * is found by asking about each character class in turn. Most items are
	 * judged without either being wanted.
	 */
	const LiveOnlyFacts *liveOnly;
};
