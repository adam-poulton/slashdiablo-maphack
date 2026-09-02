#include "ItemFactsCatalogue.h"
#include <string>
#include <vector>
#include "../../Constants.h"
#include "../../ItemDescription.h"
#include "../../ItemTables.h"
#include "../../PropertyStats.h"
#include "../../StatDescriptions.h"

namespace ItemFactsCatalogue {

namespace {

// What a stat comes to at this roll: the top of the range the source rolls it
// in, or the bottom. A stat granted as one number is that number at both ends.
int At(const StatDescriptions::StatTotal& total, Roll roll) {
	return (roll == BestRoll) ? total.high : total.low;
}

// The same reading of a range the base's own numbers are taken at.
int At(const ItemDescription::Range& range, Roll roll) {
	return (roll == BestRoll) ? range.high : range.low;
}

/*
 * Which of the game's own qualities the source is drawn as.
 *
 * A rarity is a quality wherever the game has one, and the two carry the same
 * values so they convert straight across. A runeword and a rune are the two BH
 * gives itself, and an item that is either has a plain item's quality: what
 * says an item is a runeword is the flag, which is also all a rule can ask
 * about one.
 */
unsigned int QualityOf(ItemRarity rarity) {
	switch (rarity) {
	case RarityRuneword:
	case RarityRune:
		return ITEM_QUALITY_NORMAL;
	default:
		return (unsigned int)rarity;
	}
}

}  // namespace

CatalogueStats::CatalogueStats(const ItemFacts& facts,
		const Catalogue::Source& source, Roll roll) : facts(facts) {
	std::vector<StatDescriptions::StatTotal> totals =
		PropertyStats::Totals(source.properties);
	for (unsigned int i = 0; i < totals.size(); i++) {
		// A stat the tables do not carry is one nothing can ask about, since a
		// rule names a stat by the number the tables give it.
		int id = StatDescriptions::StatId(totals[i].stat);
		if (id < 0)
			continue;

		StatEntry entry;
		entry.stat = (unsigned short)id;
		entry.sub = 0;
		entry.value = At(totals[i], roll);
		entries.push_back(entry);
	}
}

int CatalogueStats::Stat(unsigned int stat, unsigned int sub) const {
	/*
	 * Defence is not the item's own grants added up. It is what its base rolls
	 * with the source's bonuses folded into it, which is the number the finished
	 * item carries and what the source's armour properties have already been
	 * spent on. Answered off the item for that reason, the way an item from a
	 * packet answers it.
	 */
	if (stat == STAT_DEFENSE)
		return facts.defense;

	int total = 0;
	for (unsigned int i = 0; i < entries.size(); i++) {
		if (entries[i].stat != stat || entries[i].sub != sub)
			continue;
		total += entries[i].value;
	}
	return total;
}

const std::vector<StatEntry>& CatalogueStats::Stats() const {
	return entries;
}

CatalogueItem::CatalogueItem(const Catalogue::Source& source,
		ItemAttributes* attrs, Roll roll)
		: facts(), stats(facts, source, roll), known(false) {
	facts.stats = &stats;

	/*
	 * A source is not an item, so it has no price and no level it can be used
	 * at. Saying so rather than answering either is what leaves a rule that
	 * asks unjudged rather than answered wrongly, which is ADR 0002.
	 */
	facts.liveOnly = NULL;

	// A source that names a range of bases grants different things in each, and
	// which of them it is made in is not something the source decides. Nothing
	// here picks one, so there is no item to say.
	if (source.baseCode.length() == 0 || !attrs)
		return;

	facts.attrs = attrs;
	for (int i = 0; i < 3; i++) {
		// Padded with spaces where the code is shorter, as the game pads the
		// code it keeps on an item.
		facts.code[i] = (i < (int)source.baseCode.length())
			? source.baseCode[i] : ' ';
	}
	facts.code[3] = 0;
	facts.name = source.name;

	facts.quality = QualityOf(source.rarity);
	facts.runeword = source.rarity == RarityRuneword;

	/*
	 * Which unique or which piece of a set this is, filled the way an item in
	 * the world fills it: the one the quality calls for and no other, so that
	 * asked whether it is a particular set item a unique says no.
	 */
	if (source.id >= 0) {
		if (facts.quality == ITEM_QUALITY_UNIQUE)
			facts.uniqueCode = (unsigned int)source.id;
		else if (facts.quality == ITEM_QUALITY_SET)
			facts.setCode = (unsigned int)source.id;
	}

	// Described down to what it rolled, which is what being identified means.
	facts.identified = true;

	/*
	 * The level from which the game starts dropping the source, which is the
	 * lowest level an item of it exists at. A particular drop's item level is
	 * the level of whatever dropped it and is not something a source says, so
	 * this stands in for it.
	 */
	facts.level = (unsigned char)source.level;

	const ItemDescription::Base* base =
		ItemDescription::FindBase(source.baseCode);
	if (base) {
		facts.defense = At(ItemDescription::Defense(*base, source.modifiers),
			roll);
	}

	// What the source grants sockets, said as a field of the item as well,
	// since that is where an item keeps them.
	facts.sockets = (unsigned char)stats.Stat(STAT_SOCKETS, 0);
	facts.hasSockets = facts.sockets > 0;

	known = true;
}

}
