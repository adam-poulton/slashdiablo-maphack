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
	bool disabled;		// shown, but takes no clicks
	bool inDialog;		// part of a dialog open over the screen
	std::wstring text;	// what an edit box holds; empty for every other kind
};

// The game keeps a control's position as its bottom-left corner.
ControlShape ShapeOf(const Control* control);
std::vector<ControlShape> ShapesOf(const Control* first);

const char* ControlTypeName(unsigned int type);

// With the rectangle's centre, which is where a click on it is aimed.
nlohmann::json ToJson(const ControlShape& control);

// Which out-of-game screen the controls are, told by the buttons that take its
// clicks, measured at the 800x600 the menus always run at: "main_menu",
// "character_list", "new_character" or "difficulty" where those buttons are
// exactly that screen's, and "menu" for anything else. While a dialog is open
// its buttons are the ones that take clicks, so the difficulty dialog over the
// character list is "difficulty", and a popup the game draws as part of the
// screen beneath, which adds a button of its own, leaves the screen unnamed.
std::string NameScreen(const std::vector<ControlShape>& controls);

}
