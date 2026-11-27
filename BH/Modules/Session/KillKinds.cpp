#include "KillKinds.h"

using namespace SessionEvents;

namespace KillKinds {

namespace {
	// baaltentacle1 to baaltentacle5, and baalclone.
	const unsigned int kFirstTentacle = 562;
	const unsigned int kLastTentacle = 566;
	const unsigned int kBaalClone = 570;
}

bool Counts(const Kind& kind) {
	if (!kind.killable || kind.npc || kind.neverCount || kind.align != 0)
		return false;
	if (kind.classId >= kFirstTentacle && kind.classId <= kLastTentacle)
		return false;
	return kind.classId != kBaalClone;
}

Tier TierOf(const Marks& marks) {
	if (marks.primeEvil)
		return TierBoss;
	if (marks.superUnique)
		return TierSuperUnique;
	if (marks.unique)
		return TierUnique;
	if (marks.champion)
		return TierChampion;
	if (marks.minion)
		return TierMinion;
	return TierNormal;
}

}
