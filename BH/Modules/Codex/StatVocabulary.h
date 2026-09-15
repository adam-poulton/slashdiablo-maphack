#pragma once
#include <string>
#include <vector>

/*
 * The stats the condition builder offers, for one kind of source.
 *
 * There is no list of stats written here and none anywhere else. What can be
 * offered is asked of the stat index: every stat the sources of that kind
 * write, which is exactly the set a criterion can answer. A realm shipping a
 * unique that grants something new puts that something in the picker without a
 * line of this file changing, and nothing in the picker can come back empty for
 * want of a source that grants it.
 *
 * What each stat reads as comes from the game's own description of it, with the
 * value's placeholder taken out. Those words were written to follow a number
 * rather than to label a control, so two stats can come out reading alike -
 * flat and percentage cold absorb are both "Cold Absorb" - and a handful have
 * no words in the tables at all. Both are answered here, so that every label
 * offered is one a player can read and no two of them are the same.
 *
 * The stat's own name is carried alongside because a criterion is written with
 * it, not because anything shows it. docs/adr/0010 is where this is reasoned
 * about and the alternatives are recorded.
 */
namespace StatVocabulary {
	struct Entry {
		std::string stat;	// as ItemStatCost.txt names it
		std::string label;	// what the player reads

		Entry() {};
		Entry(const std::string& stat, const std::string& label) :
			stat(stat), label(label) {};
	};

	// Every stat the sources of one kind can grant, ordered by label. Worked out
	// once per kind and kept, since the catalogues are built once and the answer
	// cannot change afterwards.
	//
	// Empty until the catalogues and the string tables are both in: the stats
	// come from the first and the words from the second, and a label worked out
	// before the strings arrive would be the stat's own name for the rest of the
	// session.
	const std::vector<Entry>& For(const std::string& kind);

	// The entries whose label carries the text, in the vocabulary's own order.
	// Matched case insensitively against both the label and the stat name, so a
	// player can type either. Empty text is carried by every entry.
	std::vector<Entry> Matching(const std::string& kind, const std::string& text);

	// The label one stat reads as, or the stat's own name where the kind does
	// not grant it - which is what a condition kept across a change of tab
	// shows.
	std::string LabelFor(const std::string& kind, const std::string& stat);
};
