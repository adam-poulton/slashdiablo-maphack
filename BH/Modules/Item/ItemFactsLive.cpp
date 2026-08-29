#include "ItemFactsLive.h"
#include "../../D2Ptrs.h"
#include "../../D2Structs.h"

int LiveStats::Stat(unsigned int stat, unsigned int sub) const {
	if (!item)
		return 0;
	return D2COMMON_GetUnitStat(item, stat, sub);
}

const std::vector<StatEntry>& LiveStats::Stats() const {
	if (built)
		return entries;
	built = true;
	if (!item)
		return entries;

	/*
	 * Copied into a buffer of a fixed size because that is the only way the
	 * game offers the list. The size and the flag were taken from d2bs, where
	 * four separate conditions each carried their own copy of this before it
	 * was written down once.
	 */
	// Qualified: inside this class Stat is the member above, not the game's.
	::Stat copied[256] = { NULL };
	StatList* list = D2COMMON_GetStatList(item, NULL, 0x40);
	if (!list)
		return entries;

	DWORD count = D2COMMON_CopyStatList(list, (::Stat*)copied, 256);
	entries.reserve(count);
	for (DWORD i = 0; i < count; i++) {
		StatEntry entry;
		entry.stat = copied[i].wStatIndex;
		entry.sub = copied[i].wSubIndex;
		entry.value = (int)copied[i].dwStatValue;
		entries.push_back(entry);
	}
	return entries;
}
