#pragma once
#include <string>
#include <vector>

#include "nlohmann/json.hpp"

struct Control;

namespace Agent {

// One of the game's out-of-game controls, as the driver reports it: a
// rectangle in window pixels with its top-left corner, which is not how the
// game keeps it (see ShapeOf).
struct ControlShape {
	unsigned int type;
	int x;
	int y;
	int width;
	int height;
	bool disabled;
	std::wstring text;	// what an edit box holds; empty for every other kind
};

// The game keeps a control's position as its bottom-left corner.
ControlShape ShapeOf(const Control* control);
std::vector<ControlShape> ShapesOf(const Control* first);

const char* ControlTypeName(unsigned int type);

// With the rectangle's centre, which is where a click on it is aimed.
nlohmann::json ToJson(const ControlShape& control);

// Which out-of-game screen the controls are: "main_menu", "character_list" or
// "difficulty" where the list is exactly that screen and nothing is open over
// it, and "menu" for anything else, a screen with a popup over it included.
// Told by the buttons alone, measured at the 800x600 the menus always run at,
// so that a screen showing a popup, which adds a button of its own, is never
// mistaken for one that will take a click.
std::string NameScreen(const std::vector<ControlShape>& controls);

}
