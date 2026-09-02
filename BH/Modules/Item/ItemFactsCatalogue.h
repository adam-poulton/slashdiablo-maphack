#pragma once
#include <vector>
#include "../../Catalogue/Source.h"
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
 * The stat index asks what a source could grant and answers with the range it
 * rolls. An item carries one number for a stat, not a range, so which roll the
 * item is said at is chosen here.
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
	 * A stat the game stores with a sub index alongside the value is written
	 * with none, because a total says how much a source grants and not which
	 * skill or which class it granted it to. So a condition naming one of
	 * those, a single skill or a class's skill tab, is answered from nothing
	 * rather than from a total belonging to some other skill.
	 *
	 * Holds the item rather than a copy of it, so it must not outlive what it
	 * was made from.
	 */
	class CatalogueStats : public StatSource {
	public:
		CatalogueStats(const ItemFacts& facts, const Catalogue::Source& source,
				Roll roll);

		int Stat(unsigned int stat, unsigned int sub) const override;
		const std::vector<StatEntry>& Stats() const override;

	private:
		const ItemFacts& facts;
		std::vector<StatEntry> entries;
	};

	/*
	 * One source as an item, owning everything the facts point at.
	 *
	 * The item is said on the source's own base, so a source that names a range
	 * of bases rather than one cannot be said at all: which of them a runeword
	 * is made in decides what it grants, and nothing here picks one. Such a
	 * source is not known.
	 */
	class CatalogueItem {
	public:
		/*
		 * attrs is the base item's attributes, as ItemAttributeMap holds them,
		 * and null for a source whose base the tables do not describe. Handed
		 * in rather than looked up because that map is filled from the game's
		 * archives, and a catalogue item is meant to be built without them.
		 */
		CatalogueItem(const Catalogue::Source& source, ItemAttributes* attrs,
				Roll roll = BestRoll);

		// False where the source names no one base, or where the tables do not
		// describe the base it names. Nothing is filled in either case, which
		// is the answer an item in the world gives to the same question.
		bool Known() const { return known; }

		const ItemFacts& Facts() const { return facts; }

	private:
		ItemFacts facts;
		CatalogueStats stats;
		bool known;
	};

}
