#pragma once
#include "ItemFacts.h"

/*
 * What a rule reads that is not about the item.
 *
 * Several conditions ask about the character or the world rather than the thing
 * being judged: which class is playing, how far they have got, what difficulty
 * this is, how much the filter has been asked to hide. None of that belongs to
 * an item, and keeping it apart is what stopped the item's own facts from
 * becoming a bag of everything a rule might want.
 *
 * It is the same for an item lying in the world and an item a packet has just
 * described, because it was always read from the running game for both. The two
 * halves each condition used to carry were never about this.
 */
struct FilterContext {
	unsigned int charClass;
	unsigned int charLevel;

	// Recorded whole rather than unpacked, because PLAYERTYPE reads a bit out
	// of it.
	unsigned int charFlags;

	unsigned int difficulty;

	// How much of what drops the filter has been asked to hide.
	unsigned int filterLevel;

	// The character's own stats, which CHARSTAT asks about. Held the same way
	// an item's are, since the question is the same one.
	const StatSource *charStats;
};
