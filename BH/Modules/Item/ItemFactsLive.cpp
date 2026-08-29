#include "ItemFactsLive.h"
#include "../../D2Ptrs.h"
#include "../../D2Structs.h"
#include "../../Constants.h"
#include "../../MPQInit.h"

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

unsigned int GetUsedSockets(UnitAny *item) {
	unsigned int used = 0;
	if (item == NULL || item->pInventory == NULL) {
		return 0;
	}
	for (UnitAny *sItem = item->pInventory->pFirstItem; sItem; sItem = sItem->pItemData->pNextInvItem) {
		used++;
	}
	return used;
}

LiveItem::LiveItem(UnitAny* item) : stats(item), unit(), facts(), known(false) {
	unit.item = item;
	unit.attrs = NULL;
	unit.facts = &facts;
	facts.attrs = NULL;
	facts.stats = &stats;

	const char* code = D2COMMON_GetItemText(item->dwTxtFileNo)->szCode;
	for (int i = 0; i < 3; i++) {
		unit.itemCode[i] = code[i];
		facts.code[i] = code[i];
	}
	unit.itemCode[3] = 0;
	facts.code[3] = 0;

	std::map<std::string, ItemAttributes*>::const_iterator found =
		ItemAttributeMap.find(unit.itemCode);
	if (found == ItemAttributeMap.end())
		return;
	unit.attrs = found->second;
	facts.attrs = found->second;
	known = true;

	// Only what a collapsed condition asks for. The rest of an item in the world
	// is still read from the game at the moment it is wanted.
	facts.quality = item->pItemData->dwQuality;
	facts.level = (unsigned char)item->pItemData->dwItemLevel;
	facts.usedSockets = (unsigned char)GetUsedSockets(item);

	/*
	 * The flags a rule may ask about, as the named things a packet describes
	 * rather than as bits. Only these three can be asked for: they are the only
	 * flags a rule's text can name.
	 */
	DWORD flags = item->pItemData->dwFlags;
	facts.ethereal = (flags & ITEM_ETHEREAL) > 0;
	facts.identified = (flags & ITEM_IDENTIFIED) > 0;
	facts.runeword = (flags & ITEM_RUNEWORD) > 0;

	/*
	 * Which unique or which set piece this is.
	 *
	 * The game keeps one number for it and says which kind it is separately,
	 * where a packet carries a field for each. Filling only the one the item's
	 * quality calls for is what makes the two agree: asked whether a rare is a
	 * particular unique, both say no.
	 */
	if (facts.quality == ITEM_QUALITY_UNIQUE)
		facts.uniqueCode = item->pItemData->dwFileIndex;
	else if (facts.quality == ITEM_QUALITY_SET)
		facts.setCode = item->pItemData->dwFileIndex;
}
