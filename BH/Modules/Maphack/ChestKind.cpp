#include "ChestKind.h"
#include <cstdlib>

namespace ChestKind {
	bool IsMarkable(const ObjectFacts& facts) {
		if (!facts.selectable)
			return false;
		switch (facts.operateFn) {
			case OperateCasket:
			case OperateUrn:
			case OperateChest:
			case OperateBarrel:
			case OperateExplodingBarrel:
			case OperateBody:
			case OperateArmorStand:
			case OperateWeaponRack:
			case OperateWrit:
			case OperateTrappedSoul:
			case OperateStash:
			case OperateEvilUrn:
				return true;
			default:
				return false;
		}
	}

	bool IsSparkly(const ObjectFacts& facts) {
		return facts.initFn == InitSparklyChest;
	}

	static bool RollsTrap(unsigned int initFn) {
		return initFn == InitTrapped || initFn == InitChest || initFn == InitSparklyChest;
	}

	static bool RollsLock(unsigned int initFn) {
		return initFn == InitChest || initFn == InitSparklyChest;
	}

	Kind Classify(const ObjectFacts& facts, bool lowerKurastSuper, SuperScope scope) {
		if (!IsMarkable(facts))
			return NotMarked;
		// The huts hold baskets and stashes as well, as near the fire as the chest.
		if ((lowerKurastSuper && facts.operateFn == OperateChest) ||
				(scope == Everywhere && IsSparkly(facts)))
			return SuperChest;
		// An exploding barrel is a trap whatever its byte says: what it does when
		// opened is go off.
		if (facts.operateFn == OperateExplodingBarrel ||
				(RollsTrap(facts.initFn) && (facts.interact & InteractTrap) != 0))
			return Trapped;
		if (RollsLock(facts.initFn) && (facts.interact & InteractLocked) != 0)
			return LockedChest;
		return facts.operateFn == OperateChest ? Chest : Container;
	}

	bool NearAny(Point position, const std::vector<Point>& anchors, int reach) {
		for (const Point& anchor : anchors) {
			if (std::abs(position.x - anchor.x) <= reach &&
					std::abs(position.y - anchor.y) <= reach)
				return true;
		}
		return false;
	}
}
