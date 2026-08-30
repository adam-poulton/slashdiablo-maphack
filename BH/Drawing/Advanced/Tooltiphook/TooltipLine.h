#pragma once

#include <string>
#include "../../../Constants.h"

/*
 * Kept apart from Tooltiphook.h so that code which only describes something in
 * lines of text can say so without pulling in the panel that draws them, and
 * with it the game's own drawing routines.
 */
namespace Drawing {
	// One line of a tooltip, with the colour it is drawn in.
	struct TooltipLine {
		std::string text;
		TextColor color;

		TooltipLine() : color(White) {};
		TooltipLine(std::string text, TextColor color = White) :
			text(text), color(color) {};
	};
};
