#pragma once
#include <vector>
#include "../../Catalogue/Source.h"
#include "../../StatDescriptions.h"
#include "ItemFacts.h"

struct ItemAttributes;

/*
 * A catalogue source said as item facts: an item nobody has dropped and nobody
 * will, described from the game's tables alone.
 *
 * This is the third way an item reaches the filter, beside an item already in
 * the world and one a packet has just described, and it is here so that a rule
 * can be walked against an item that does not exist. A filter builder is seeded
 * from one of these, and previews the label and the automap mark by walking the
 * real rules against one, which is what keeps a preview from drifting away from
 * what a game shows. ADR 0004 is why the stat index does not read one.
 *
 * Two things a source leaves open have to be settled before it is an item. The
 * stat index asks what a source could grant and answers with the range it
 * rolls, where an item carries one number, so the roll is chosen here. A source
 * that names a range of bases grants different things in each, so the base is
 * chosen by whoever is asking.
 *
 * What no source says is left unsaid rather than stood in for. An item's level
 * is the level of whatever dropped it, so ILVL, and the affix and craft levels
 * read off it, are answered from zero and say nothing true about a source. A
 * stat the game stores with a sub index is left out of the item's stats
 * entirely, for the reason CatalogueStats gives.
 *
 * Nothing here reaches into the game, so a rule can be walked against a source
 * with no game running.
 */
namespace ItemFactsCatalogue {

	// Which of the rolls a source can take the item is said at. A property that
	// grants one number grants it either way.
	enum Roll {
		BestRoll,	// every property at the top of the range it rolls
		WorstRoll	// every property at the bottom of it
	};

	/*
	 * The stats a source grants, at the roll the item was said at.
	 *
	 * Read out of the source's own property entries, by the one path every
	 * catalogue words and adds up its properties through. What that path leaves
	 * out is left out here too: amounts granted per character level, poison
	 * damage, and the properties the tables give no stat to, all of which
	 * ADR 0005 sets out.
	 *
	 * A stat the game stores with a sub index alongside the value is left out
	 * altogether. A total says how much a source grants and not which skill or
	 * which class it granted it to, and writing such a stat with a sub index of
	 * nothing would answer a rule asking about the first skill or the first
	 * class with every skill the source grants added together.
	 *
	 * Holds the item rather than a copy of it, so it must not outlive what it
	 * was made from, and is not copied for the same reason.
	 */
	class CatalogueStats : public StatSource {
	public:
		explicit CatalogueStats(const ItemFacts& facts) : facts(facts) {}

		// Reads what a source's properties add up to. Added up by whatever is
		// building the item, since which of a source's properties it grants
		// depends on the base it is being made on, and the item's own numbers
		// are read off the same totals.
		void Read(const std::vector<StatDescriptions::StatTotal>& totals,
				Roll roll);

		int Stat(unsigned int stat, unsigned int sub) const override;
		const std::vector<StatEntry>& Stats() const override;

	private:
		CatalogueStats(const CatalogueStats&);
		CatalogueStats& operator=(const CatalogueStats&);

		const ItemFacts& facts;
		std::vector<StatEntry> entries;
	};

	/*
	 * One source as an item, owning everything the facts point at.
	 *
	 * The item's own facts point back at what this holds, so it is not copied:
	 * a copy would leave its stats reading the item it was copied from.
	 */
	class CatalogueItem {
	public:
		/*
		 * The base to make the source on is the caller's, handed in as the
		 * attributes the game keeps for it in ItemAttributeMap. The caller
		 * looks up the base a source names, or picks one of the bases a source
		 * that names a range of them is allowed in.
		 *
		 * Asked for rather than looked up because that map is filled from the
		 * game's archives, and a catalogue item is meant to be built without
		 * them.
		 */
		CatalogueItem(const Catalogue::Source& source, ItemAttributes* attrs,
				Roll roll = BestRoll);

		// False where no base was handed in, where the tables carry no such
		// base, and where the base is not of a kind the source can be made on.
		// Nothing is filled in any of those cases, which is the answer an item
		// in the world gives to the same question.
		bool Known() const { return known; }

		const ItemFacts& Facts() const { return facts; }

	private:
		CatalogueItem(const CatalogueItem&);
		CatalogueItem& operator=(const CatalogueItem&);

		ItemFacts facts;
		CatalogueStats stats;
		bool known;
	};

}
