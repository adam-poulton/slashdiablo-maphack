#include "AgentScreen.h"

#include <algorithm>
#include <tuple>

#include "../../D2Structs.h"
#include "../../Constants.h"
#include "../../StringUtil.h"

namespace Agent {

namespace {

struct Button {
	int x, y, width, height;	// top-left, as ShapeOf reports it

	bool operator<(const Button& other) const {
		return std::tie(x, y, width, height) < std::tie(other.x, other.y, other.width, other.height);
	}
	bool operator==(const Button& other) const {
		return std::tie(x, y, width, height) == std::tie(other.x, other.y, other.width, other.height);
	}
};

// Each screen's buttons, from the control lists of a 1.13c client.
const std::vector<Button> kMainMenu = {
	{264, 289, 272, 35},	// Single Player
	{264, 331, 272, 35},	// Battle.net
	{264, 366, 272, 25},	// Gateway
	{264, 398, 272, 35},	// Other Multiplayer
	{264, 503, 135, 25},	// Credits
	{402, 503, 135, 25},	// Cinematics
	{264, 533, 272, 35},	// Exit Diablo II
};
const std::vector<Button> kCharacterList = {
	{627, 537, 128, 35},	// OK
	{33, 537, 128, 35},		// Exit
	{33, 468, 168, 60},		// Create New Character
	{233, 468, 168, 60},	// Convert to Expansion
	{433, 468, 168, 60},	// Delete Character
};
const std::vector<Button> kDifficulty = {
	{264, 300, 272, 35},	// Normal
	{264, 385, 272, 35},	// Nightmare
	{264, 470, 272, 35},	// Hell
	{33, 537, 128, 35},		// Cancel
};
// The class choice, which is the same screen once a class is picked.
const std::vector<Button> kNewCharacter = {
	{33, 537, 128, 35},		// Exit
	{627, 537, 128, 35},	// OK
	{319, 524, 15, 16},		// Expansion Character
	{319, 544, 15, 16},		// Hardcore
};

std::vector<Button> Sorted(std::vector<Button> buttons) {
	std::sort(buttons.begin(), buttons.end());
	return buttons;
}

}  // namespace

ControlShape ShapeOf(const Control* control) {
	ControlShape shape;
	shape.type = control->dwType;
	shape.x = (int)control->dwPosX;
	shape.y = (int)control->dwPosY - (int)control->dwSizeY;
	shape.width = (int)control->dwSizeX;
	shape.height = (int)control->dwSizeY;
	shape.disabled = control->dwDisabled != 0;
	if (control->dwType == CONTROL_EDITBOX) {
		const wchar_t* text = control->wText;
		size_t length = 0;
		while (length < sizeof(control->wText) / sizeof(wchar_t) && text[length])
			length++;
		shape.text.assign(text, length);
	}
	return shape;
}

std::vector<ControlShape> ShapesOf(const Control* first) {
	std::vector<ControlShape> shapes;
	for (const Control* control = first; control; control = control->pNext)
		shapes.push_back(ShapeOf(control));
	return shapes;
}

const char* ControlTypeName(unsigned int type) {
	switch (type) {
		case CONTROL_EDITBOX: return "editbox";
		case CONTROL_IMAGE: return "image";
		case CONTROL_TEXTBOX: return "textbox";
		case CONTROL_SCROLLBAR: return "scrollbar";
		case CONTROL_BUTTON: return "button";
		case CONTROL_LIST: return "list";
		case CONTROL_TIMER: return "timer";
		case CONTROL_SMACK: return "smack";
		case CONTROL_PROGRESSBAR: return "progressbar";
		case CONTROL_POPUP: return "popup";
		case CONTROL_ACCOUNTLIST: return "accountlist";
		default: return "unknown";
	}
}

nlohmann::json ToJson(const ControlShape& control) {
	nlohmann::json json = {
		{"type", ControlTypeName(control.type)},
		{"rect", {control.x, control.y, control.width, control.height}},
		{"center", {control.x + control.width / 2, control.y + control.height / 2}},
		{"disabled", control.disabled},
	};
	if (control.type == CONTROL_EDITBOX)
		json["text"] = ToUtf8(control.text);
	return json;
}

std::string NameScreen(const std::vector<ControlShape>& controls) {
	std::vector<Button> buttons;
	for (const ControlShape& control : controls) {
		if (control.type == CONTROL_BUTTON)
			buttons.push_back({control.x, control.y, control.width, control.height});
	}
	buttons = Sorted(buttons);
	if (buttons == Sorted(kMainMenu))
		return "main_menu";
	if (buttons == Sorted(kCharacterList))
		return "character_list";
	if (buttons == Sorted(kDifficulty))
		return "difficulty";
	if (buttons == Sorted(kNewCharacter))
		return "new_character";
	return "menu";
}

}
