#pragma once
#include <vector>

// Which kind of marker an object on the map gets, if any: chests, the barrels and
// other containers that can be opened, and which of them are locked, trapped or a
// super chest.
//
// Pure, so it can be tested without the game. What it is told about an object is
// read off the unit and its row of objects.txt by the caller.
namespace ChestKind {
	// In the order they are offered in.
	enum Kind {
		Chest,
		LockedChest,
		SuperChest,
		Container,
		Trapped,
		KindCount,
		NotMarked = KindCount
	};

	// Where super chests are marked as such. Lower Kurast's are the ones players
	// have always had marked. Everywhere adds the super chest presets of other
	// levels and the sparkly chests.
	enum SuperScope {
		LowerKurastOnly,
		Everywhere,
		SuperScopeCount
	};

	struct ObjectFacts {
		bool selectable;		// objects.txt Selectable0
		unsigned int operateFn;	// objects.txt OperateFn
		unsigned int initFn;	// objects.txt InitFn
		unsigned int interact;	// ObjectData::Type, as the server sent it
	};

	// objects.txt OperateFn values for what is opened by clicking it and has
	// something in it.
	const unsigned int OperateCasket = 1;		// beds, caskets, sarcophagi
	const unsigned int OperateUrn = 3;			// urns, jars, baskets
	const unsigned int OperateChest = 4;		// chests, and corpses that drop like one
	const unsigned int OperateBarrel = 5;
	const unsigned int OperateExplodingBarrel = 7;
	const unsigned int OperateBody = 14;		// corpses, cocoons, rock piles
	const unsigned int OperateArmorStand = 19;
	const unsigned int OperateWeaponRack = 20;
	const unsigned int OperateWrit = 33;
	const unsigned int OperateTrappedSoul = 48;
	const unsigned int OperateStash = 51;		// jungle stashes, boxes, altars
	const unsigned int OperateEvilUrn = 68;

	// objects.txt InitFn values that roll the byte the server sends as the
	// object's interact type. InitFn 2 rolls a trap into its low seven bits
	// (D2Game 0x2EA40); InitFn 3, every ordinary chest, rolls the trap and then
	// the lock into its top bit (D2Game 0x2ECB0); InitFn 57, the sparkly chest,
	// is InitFn 3 with the sparkle switched on (D2Game 0x2EF50). Anything else
	// leaves the byte meaning something else or nothing.
	const unsigned int InitTrapped = 2;
	const unsigned int InitChest = 3;
	const unsigned int InitSparklyChest = 57;

	const unsigned int InteractLocked = 0x80;
	const unsigned int InteractTrap = 0x7F;

	// The preset a super chest is placed from. It has no row in objects.txt: the
	// server turns it into one of the act's chests and flags that chest (D2Game
	// 0x331B0). Lower Kurast's huts hold them, and that handler has cases of its
	// own for the Forgotten Tower and the Maggot Lair.
	const unsigned int SuperChestPreset = 580;

	// The kind of marker the object gets, NotMarked for none. atSuperChestPreset
	// is whether it stands where a super chest preset placed something, which only
	// the caller can tell; only a chest standing there is the super chest.
	//
	// A super chest is marked as one ahead of whatever else it is, since that is
	// what makes it worth the detour; a trap ahead of a lock, since it is what
	// costs you to open it.
	Kind Classify(const ObjectFacts& facts, bool atSuperChestPreset, SuperScope scope);

	// Whether the object is one of the kinds marked at all.
	bool IsMarkable(const ObjectFacts& facts);

	// Whether the object is a chest the sparkle is switched on for.
	bool IsSparkly(const ObjectFacts& facts);

	struct Point {
		int x, y;
	};

	// Whether a position is within reach of any of the anchors, measured as the
	// game measures it on the ground: the larger of the two distances.
	bool NearAny(Point position, const std::vector<Point>& anchors, int reach);
}
