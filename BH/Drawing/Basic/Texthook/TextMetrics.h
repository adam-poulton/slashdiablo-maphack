#pragma once

namespace Drawing {

// How far below the top of a line of text in each of the game's fonts it is
// drawn from: D2WIN_DrawText places text by its baseline.
inline unsigned int BaselineOffset(unsigned int font) {
	static const unsigned int offsets[] = {10, 11, 18, 24, 10, 13, 7, 13, 10, 12, 8, 8, 7, 12};
	return font < sizeof(offsets) / sizeof(offsets[0]) ? offsets[font] : 0;
}

}
