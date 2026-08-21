#pragma once
#include "Constants.h"

// How an item's name is drawn, which in the game follows its rarity.
//
// The game's own item qualities are here under their own names and with their
// own values, so a quality converts straight across. Alongside them are the
// classifications BH needs that the game does not count as qualities: a runeword
// and a rune. Both are drawn in a rarity's colour wherever BH shows them - a
// runeword's name like a unique's, a rune in the orange the game gives it - so it
// is simpler to treat them as rarities than to special case them at each place
// something is drawn.
enum ItemRarity {
	RarityNone = ITEM_QUALITY_NONE,
	RarityInferior = ITEM_QUALITY_INFERIOR,
	RarityNormal = ITEM_QUALITY_NORMAL,
	RaritySuperior = ITEM_QUALITY_SUPERIOR,
	RarityMagic = ITEM_QUALITY_MAGIC,
	RaritySet = ITEM_QUALITY_SET,
	RarityRare = ITEM_QUALITY_RARE,
	RarityUnique = ITEM_QUALITY_UNIQUE,
	RarityCrafted = ITEM_QUALITY_CRAFT,

	// Not item qualities, so they carry on past the last of them.
	RarityRuneword,
	RarityRune
};

// The colour the game draws an item of this rarity in.
TextColor RarityColor(ItemRarity rarity);

// The rarity of one of the game's ITEM_QUALITY_* values.
ItemRarity RarityFromQuality(unsigned int quality);
