#include "ItemRarity.h"

// Indexed by ItemRarity. Its first entries are the game's own quality values, so
// this doubles as the quality colour table.
static const TextColor kRarityColors[] = {
	White,		// none
	White,		// inferior
	White,		// normal
	White,		// superior
	Blue,		// magic
	Green,		// set
	Yellow,		// rare
	Gold,		// unique
	Orange,		// crafted
	Gold,		// runeword, drawn as a unique is
	Orange		// rune
};

TextColor RarityColor(ItemRarity rarity) {
	if (rarity < 0 || rarity >= (int)(sizeof(kRarityColors) / sizeof(kRarityColors[0])))
		return White;
	return kRarityColors[rarity];
}

TextColor NameColor(ItemRarity rarity) {
	TextColor color = RarityColor(rarity);
	return (color == White) ? Gold : color;
}

ItemRarity RarityFromQuality(unsigned int quality) {
	// Anything the game has added since is drawn as an ordinary item rather than
	// read off the end of the table.
	if (quality > ITEM_QUALITY_CRAFT)
		return RarityNormal;
	return (ItemRarity)quality;
}
