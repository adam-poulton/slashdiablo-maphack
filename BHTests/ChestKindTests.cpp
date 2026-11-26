#include "doctest.h"
#include "Modules/Maphack/ChestKind.h"

/*
 * Which marker an object on the map gets.
 *
 * The objects are as objects.txt has them: a plain chest is OperateFn 4 with
 * InitFn 3, a sparkly chest InitFn 57, an urn OperateFn 3 with InitFn 2, a barrel
 * OperateFn 5 with nothing rolled for it. The interact byte is what the server
 * rolled: a trap in the low seven bits, the lock in the top one.
 */

using namespace ChestKind;

namespace {
	ObjectFacts Object(unsigned int operateFn, unsigned int initFn,
			unsigned int interact = 0, bool selectable = true) {
		return ObjectFacts{ selectable, operateFn, initFn, interact };
	}

	const ObjectFacts PlainChest = Object(OperateChest, InitChest);
	const ObjectFacts SparklyChest = Object(OperateChest, InitSparklyChest);
	const ObjectFacts Urn = Object(OperateUrn, InitTrapped);
	const ObjectFacts Barrel = Object(OperateBarrel, 0);
	const ObjectFacts ExplodingBarrel = Object(OperateExplodingBarrel, 0);
}

TEST_CASE("a plain chest is a chest") {
	CHECK(Classify(PlainChest, false, LowerKurastOnly) == Chest);
}

TEST_CASE("a chest with its lock rolled is locked") {
	CHECK(Classify(Object(OperateChest, InitChest, InteractLocked), false,
		LowerKurastOnly) == LockedChest);
}

TEST_CASE("a chest with a trap rolled is trapped, even when it is locked too") {
	CHECK(Classify(Object(OperateChest, InitChest, 3), false, LowerKurastOnly) == Trapped);
	CHECK(Classify(Object(OperateChest, InitChest, InteractLocked | 3), false,
		LowerKurastOnly) == Trapped);
}

TEST_CASE("a barrel, an urn and the like are containers") {
	CHECK(Classify(Barrel, false, LowerKurastOnly) == Container);
	CHECK(Classify(Urn, false, LowerKurastOnly) == Container);
	CHECK(Classify(Object(OperateStash, InitTrapped), false, LowerKurastOnly) == Container);
	CHECK(Classify(Object(OperateBody, 0), false, LowerKurastOnly) == Container);
}

TEST_CASE("an urn with a trap rolled is trapped") {
	CHECK(Classify(Object(OperateUrn, InitTrapped, 5), false, LowerKurastOnly) == Trapped);
}

TEST_CASE("an exploding barrel is always trapped") {
	CHECK(Classify(ExplodingBarrel, false, LowerKurastOnly) == Trapped);
}

TEST_CASE("the interact byte means nothing for an object that never rolls it") {
	// A barrel's byte is not a trap or a lock, whatever it holds.
	CHECK(Classify(Object(OperateBarrel, 0, 0xFF), false, LowerKurastOnly) == Container);
	CHECK(Classify(Object(OperateChest, 0, 0xFF), false, LowerKurastOnly) == Chest);
}

TEST_CASE("an urn's top bit is not a lock") {
	CHECK(Classify(Object(OperateUrn, InitTrapped, InteractLocked), false,
		LowerKurastOnly) == Container);
}

TEST_CASE("a chest by a Lower Kurast fire is a super chest") {
	CHECK(Classify(PlainChest, true, LowerKurastOnly) == SuperChest);
	CHECK(Classify(PlainChest, true, Everywhere) == SuperChest);
}

TEST_CASE("a super chest is marked as one ahead of its trap or lock") {
	CHECK(Classify(Object(OperateChest, InitChest, InteractLocked | 2), true,
		LowerKurastOnly) == SuperChest);
}

TEST_CASE("what else stands by a Lower Kurast fire is not the super chest") {
	CHECK(Classify(Urn, true, LowerKurastOnly) == Container);
	CHECK(Classify(Object(OperateStash, InitTrapped), true, LowerKurastOnly) == Container);
}

TEST_CASE("a sparkly chest is a super chest only when they are marked everywhere") {
	CHECK(Classify(SparklyChest, false, Everywhere) == SuperChest);
	CHECK(Classify(SparklyChest, false, LowerKurastOnly) == Chest);
	CHECK(Classify(Object(OperateChest, InitSparklyChest, InteractLocked), false,
		LowerKurastOnly) == LockedChest);
}

TEST_CASE("what cannot be clicked, or holds nothing, is not marked") {
	CHECK(Classify(Object(OperateChest, InitChest, 0, false), false, Everywhere) == NotMarked);
	// A shrine: OperateFn 2.
	CHECK(Classify(Object(2, 0), false, Everywhere) == NotMarked);
	// A door: OperateFn 8.
	CHECK(Classify(Object(8, 0), false, Everywhere) == NotMarked);
}

TEST_CASE("a position is near an anchor within reach on both axes") {
	std::vector<Point> fires = { { 100, 100 }, { 300, 50 } };

	CHECK(NearAny(Point{ 108, 92 }, fires, 8));
	CHECK(NearAny(Point{ 300, 50 }, fires, 8));
	CHECK_FALSE(NearAny(Point{ 109, 100 }, fires, 8));
	CHECK_FALSE(NearAny(Point{ 100, 91 }, fires, 8));
	CHECK_FALSE(NearAny(Point{ 100, 100 }, {}, 8));
}
