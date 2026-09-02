#include "ItemFactsCatalogue.h"
#include <string>
#include <vector>
#include "../../Catalogue/RunewordCatalogue.h"
#include "../../Constants.h"
#include "../../ItemDescription.h"
#include "../../ItemRarity.h"
#include "../../ItemTables.h"
#include "../../PropertyStats.h"
#include "../../StatDescriptions.h"

namespace ItemFactsCatalogue {

namespace {

// Whichever end of a range the roll asks for. A range whose ends are equal is
// the one number it is either way.
int At(int low, int high, Roll roll) {
	return (roll == BestRoll) ? high : low;
}

// The stats the game stores with something alongside the value: which skill,
// which class, which of a class's skill tabs. What a total says is how much,
// so none of these can be said at all. CatalogueStats has why.
bool CarriesSubIndex(unsigned int stat) {
	switch (stat) {
	case STAT_CHARGED:
	case STAT_NONCLASSSKILL:
	case STAT_SINGLESKILL:
	case STAT_CLASSSKILLS:
	case STAT_SKILLTAB:
		return true;
	default:
		return false;
	}
}

/*
 * What the source grants on the base it is being made on.
 *
 * A source made on one base grants its own properties, which are the whole of
 * what it grants. A source that names a range of bases grants what its variant
 * for that kind of base does, since each of a runeword's runes gives one set of
 * bonuses in a weapon, another in a helm or body armour and a third in a
 * shield.
 *
 * Null where the source has variants and none of them is of that base's kind,
 * which is a base the source cannot be made on.
 */
const std::vector<PropertyStats::Property>* GrantsOn(
		const Catalogue::Source& source, const ItemDescription::Base& base) {
	if (source.variants.empty())
		return &source.properties;

	std::string kind = RunewordCatalogue::BaseKind(base.type);
	for (unsigned int i = 0; i < source.variants.size(); i++) {
		if (source.variants[i].baseKind.compare(kind) == 0)
			return &source.variants[i].properties;
	}
	return NULL;
}

}  // namespace

void CatalogueStats::Read(
		const std::vector<StatDescriptions::StatTotal>& totals, Roll roll) {
	for (unsigned int i = 0; i < totals.size(); i++) {
		// A stat the tables do not carry is one nothing can ask about, since a
		// rule names a stat by the number the tables give it.
		int id = StatDescriptions::StatId(totals[i].stat);
		if (id < 0 || CarriesSubIndex((unsigned int)id))
			continue;

		StatEntry entry;
		entry.stat = (unsigned short)id;
		entry.sub = 0;
		entry.value = At(totals[i].low, totals[i].high, roll);
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
	 *
	 * Sockets are answered off the item for the same reason. A source grants
	 * them as a property, but a runeword's are the sockets its runes fill and
	 * are no property of its own, so the item's own count is the one answer that
	 * holds for both.
	 */
	if (stat == STAT_DEFENSE)
		return facts.defense;
	if (stat == STAT_SOCKETS)
		return facts.sockets;

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
		: facts(), stats(facts), known(false) {
	facts.stats = &stats;

	/*
	 * A source is not an item, so it has no price and no level it can be used
	 * at. Saying so rather than answering either is what leaves a rule that
	 * asks unjudged rather than answered wrongly, which is ADR 0002.
	 */
	facts.liveOnly = NULL;

	if (!attrs)
		return;
	const ItemDescription::Base* base = ItemDescription::FindBase(attrs->code);
	if (!base)
		return;

	const std::vector<PropertyStats::Property>* properties =
		GrantsOn(source, *base);
	if (!properties)
		return;

	// Added up once, since the item's stats, its defence and its sockets are all
	// read off the same totals.
	std::vector<StatDescriptions::StatTotal> totals =
		PropertyStats::Totals(*properties);
	stats.Read(totals, roll);

	facts.attrs = attrs;
	for (int i = 0; i < 4; i++)
		facts.code[i] = attrs->code[i];
	facts.name = source.name;

	facts.quality = QualityFromRarity(source.rarity);
	facts.runeword = source.rarity == RarityRuneword;

	/*
	 * Which unique or which piece of a set this is, filled the way an item in
	 * the world fills it: the one the quality calls for and no other, so that
	 * asked whether it is a particular set item a unique says no. A runeword is
	 * numbered by nothing here, since the flag above is the whole of what a rule
	 * can ask about one.
	 */
	if (source.fileIndex >= 0) {
		if (facts.quality == ITEM_QUALITY_UNIQUE)
			facts.uniqueCode = (unsigned int)source.fileIndex;
		else if (facts.quality == ITEM_QUALITY_SET)
			facts.setCode = (unsigned int)source.fileIndex;
	}

	// Described down to what it rolled, which is what being identified means.
	facts.identified = true;

	/*
	 * What the base is worth with the source's own always-on bonuses folded in,
	 * which is the number the finished item carries. Worked out from the
	 * properties granted on this base rather than read off the source, so that
	 * a source made on a range of bases is worth what it is worth on this one.
	 */
	ItemDescription::Modifiers modifiers =
		ItemDescription::ReadModifiers(totals);
	ItemDescription::Range defense = ItemDescription::Defense(*base, modifiers);
	facts.defense = At(defense.low, defense.high, roll);

	/*
	 * A runeword fills one socket per rune, that being the whole of what it is
	 * made from, and grants none as a property of its own. Anything else
	 * carries the sockets its properties grant it, and carries them empty: a
	 * source says what an item rolls, not what has since been put in it.
	 */
	if (source.rarity == RarityRuneword) {
		facts.sockets = (unsigned char)source.ingredientCodes.size();
		facts.usedSockets = facts.sockets;
	} else {
		int low = 0, high = 0;
		StatDescriptions::TotalFor(totals, "item_numsockets", low, high);
		facts.sockets = (unsigned char)At(low, high, roll);
	}
	facts.hasSockets = facts.sockets > 0;

	known = true;
}

}  // namespace ItemFactsCatalogue
