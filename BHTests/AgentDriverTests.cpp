#include "doctest.h"
#include <cstring>
#include <memory>
#include <vector>
#include <Windows.h>
#include "Modules/AgentDriver/AgentKeys.h"
#include "Modules/AgentDriver/AgentScreen.h"
#include "Modules/AgentDriver/DrawLog.h"
#include "Drawing/Basic/Framehook/FrameArt.h"
#include "Modules/AgentDriver/ItemDrop.h"
#include "BitReader.h"
#include "D2Structs.h"
#include "Constants.h"

/*
 * What the agent driver tells a harness about a sandboxed game.
 *
 * The screens are built from the control lists of a 1.13c client at the 800x600
 * the menus run at, as the game keeps them: positioned by their bottom-left
 * corner. A screen is named only when nothing is open over it, because a harness
 * that clicks on a named screen has to have its click land.
 */

namespace {

class Screen {
	private:
		std::vector<std::unique_ptr<Control>> built;

	public:
		// An ordinary control unless given a state, as the client's lists show it.
		Control* Add(DWORD type, DWORD posX, DWORD posY, DWORD sizeX, DWORD sizeY, DWORD state = 0x05) {
			std::unique_ptr<Control> control(new Control());
			std::memset(control.get(), 0, sizeof(Control));
			control->dwType = type;
			control->dwState = state;
			control->dwPosX = posX;
			control->dwPosY = posY;
			control->dwSizeX = sizeX;
			control->dwSizeY = sizeY;
			if (!built.empty())
				built.back()->pNext = control.get();
			built.push_back(std::move(control));
			return built.back().get();
		}

		std::vector<Agent::ControlShape> Shapes() const {
			return Agent::ShapesOf(built.empty() ? NULL : built[0].get());
		}
};

void BuildMainMenu(Screen& screen) {
	screen.Add(CONTROL_IMAGE, 0, 599, 800, 600);
	screen.Add(CONTROL_IMAGE, 400, 120, 181, 170);
	screen.Add(CONTROL_IMAGE, 400, 120, 188, 177);
	screen.Add(CONTROL_BUTTON, 264, 391, 272, 25);
	screen.Add(CONTROL_BUTTON, 264, 366, 272, 35);
	screen.Add(CONTROL_BUTTON, 264, 433, 272, 35);
	screen.Add(CONTROL_BUTTON, 264, 324, 272, 35);
	screen.Add(CONTROL_BUTTON, 264, 528, 135, 25);
	screen.Add(CONTROL_BUTTON, 402, 528, 135, 25);
	screen.Add(CONTROL_BUTTON, 264, 568, 272, 35);
	screen.Add(CONTROL_TEXTBOX, 0, 599, 200, 40);
}

void BuildCharacterList(Screen& screen) {
	screen.Add(CONTROL_IMAGE, 0, 599, 800, 600);
	screen.Add(CONTROL_BUTTON, 627, 572, 128, 35);
	screen.Add(CONTROL_BUTTON, 33, 572, 128, 35);
	screen.Add(CONTROL_BUTTON, 33, 528, 168, 60);
	screen.Add(CONTROL_BUTTON, 233, 528, 168, 60);
	screen.Add(CONTROL_BUTTON, 433, 528, 168, 60);
	screen.Add(CONTROL_TEXTBOX, 85, 78, 466, 42);
	screen.Add(CONTROL_SCROLLBAR, 585, 457, 10, 363);
}

// The box the game opens over a screen to say something, with its one button.
void AddPopup(Screen& screen) {
	screen.Add(CONTROL_IMAGE, 268, 350, 264, 176);
	screen.Add(CONTROL_BUTTON, 351, 337, 96, 32);
	screen.Add(CONTROL_TEXTBOX, 268, 300, 264, 100);
}

// The difficulty dialog single player opens over the character list, for a
// character that has finished Normal and not Nightmare.
void AddDifficultyDialog(Screen& screen) {
	screen.Add(CONTROL_IMAGE, 237, 400, 326, 200, 0x0D);
	screen.Add(CONTROL_TEXTBOX, 264, 260, 272, 35, 0x0D);
	screen.Add(CONTROL_BUTTON, 264, 297, 272, 35, 0x0D);
	screen.Add(CONTROL_BUTTON, 264, 340, 272, 35, 0x0D);
	screen.Add(CONTROL_BUTTON, 264, 383, 272, 35, 0x0C);
	screen.Add(CONTROL_BUTTON, 900, 900, 10, 10);
}

}  // namespace

TEST_CASE("a control is reported by its top-left corner") {
	Screen screen;
	screen.Add(CONTROL_BUTTON, 264, 324, 272, 35);
	Agent::ControlShape single = screen.Shapes()[0];
	CHECK(single.x == 264);
	CHECK(single.y == 289);
	CHECK(single.width == 272);
	CHECK(single.height == 35);
}

TEST_CASE("a control is aimed at by its centre") {
	Screen screen;
	screen.Add(CONTROL_BUTTON, 264, 324, 272, 35);
	nlohmann::json json = Agent::ToJson(screen.Shapes()[0]);
	CHECK(json["type"] == "button");
	CHECK(json["center"] == nlohmann::json({400, 306}));
}

TEST_CASE("an edit box reports what it holds, and nothing past its end") {
	Screen screen;
	Control* box = screen.Add(CONTROL_EDITBOX, 322, 342, 162, 19);
	wcscpy_s(box->wText, L"Agent");
	CHECK(screen.Shapes()[0].text == L"Agent");

	for (wchar_t& c : box->wText)
		c = L'x';
	CHECK(screen.Shapes()[0].text.size() == sizeof(box->wText) / sizeof(wchar_t));
}

TEST_CASE("the main menu is named") {
	Screen screen;
	BuildMainMenu(screen);
	CHECK(Agent::NameScreen(screen.Shapes()) == "main_menu");
}

TEST_CASE("the main menu with a popup open over it is not") {
	Screen screen;
	BuildMainMenu(screen);
	AddPopup(screen);
	CHECK(Agent::NameScreen(screen.Shapes()) == "menu");
}

TEST_CASE("the character list is named, and not with a popup over it") {
	Screen list;
	BuildCharacterList(list);
	CHECK(Agent::NameScreen(list.Shapes()) == "character_list");

	AddPopup(list);
	CHECK(Agent::NameScreen(list.Shapes()) == "menu");
}

TEST_CASE("the difficulty choice is named") {
	Screen screen;
	screen.Add(CONTROL_IMAGE, 0, 599, 800, 600);
	screen.Add(CONTROL_TEXTBOX, 0, 285, 800, 35);
	screen.Add(CONTROL_BUTTON, 264, 335, 272, 35);
	screen.Add(CONTROL_BUTTON, 264, 420, 272, 35);
	screen.Add(CONTROL_BUTTON, 264, 505, 272, 35);
	screen.Add(CONTROL_BUTTON, 33, 572, 128, 35);
	CHECK(Agent::NameScreen(screen.Shapes()) == "difficulty");
}

TEST_CASE("the difficulty dialog over the character list is named for its buttons") {
	Screen screen;
	BuildCharacterList(screen);
	AddDifficultyDialog(screen);
	CHECK(Agent::NameScreen(screen.Shapes()) == "difficulty");
}

TEST_CASE("a dialog that is not one the driver knows leaves the screen a menu") {
	Screen screen;
	BuildCharacterList(screen);
	screen.Add(CONTROL_BUTTON, 351, 337, 96, 32, 0x0D);
	CHECK(Agent::NameScreen(screen.Shapes()) == "menu");
}

TEST_CASE("a control says whether it takes clicks and whether it is in a dialog") {
	Screen screen;
	screen.Add(CONTROL_BUTTON, 233, 528, 168, 60, 0x04);
	AddDifficultyDialog(screen);
	std::vector<Agent::ControlShape> shapes = screen.Shapes();
	CHECK(shapes[0].disabled);
	CHECK_FALSE(shapes[0].inDialog);
	CHECK_FALSE(shapes[3].disabled);
	CHECK(shapes[3].inDialog);
	CHECK(shapes[5].disabled);
	CHECK(Agent::ToJson(shapes[3])["dialog"] == true);
	CHECK_FALSE(Agent::ToJson(shapes[0]).contains("dialog"));
}

TEST_CASE("the class choice is named, and not with a popup over it") {
	Screen screen;
	screen.Add(CONTROL_IMAGE, 0, 599, 800, 600);
	screen.Add(CONTROL_BUTTON, 33, 572, 128, 35);
	screen.Add(CONTROL_EDITBOX, 318, 510, 157, 16);
	screen.Add(CONTROL_BUTTON, 319, 540, 15, 16);
	screen.Add(CONTROL_BUTTON, 319, 560, 15, 16);
	screen.Add(CONTROL_BUTTON, 627, 572, 128, 35);
	CHECK(Agent::NameScreen(screen.Shapes()) == "new_character");

	AddPopup(screen);
	CHECK(Agent::NameScreen(screen.Shapes()) == "menu");
}

TEST_CASE("a screen nobody has named is a menu") {
	CHECK(Agent::NameScreen({}) == "menu");

	Screen login;
	login.Add(CONTROL_EDITBOX, 322, 342, 162, 19);
	login.Add(CONTROL_BUTTON, 264, 484, 272, 35);
	CHECK(Agent::NameScreen(login.Shapes()) == "menu");
}

TEST_CASE("a posted key is down until it is posted up") {
	Agent::KeyState keys;
	CHECK_FALSE(keys.Down('A'));
	keys.Observe(WM_KEYDOWN, 'A');
	CHECK(keys.Down('A'));
	keys.Observe(WM_KEYUP, 'A');
	CHECK_FALSE(keys.Down('A'));
}

TEST_CASE("Alt and the keys pressed with it arrive as system keys") {
	Agent::KeyState keys;
	keys.Observe(WM_SYSKEYDOWN, VK_MENU);
	CHECK(keys.Down(VK_MENU));
	keys.Observe(WM_SYSKEYUP, VK_MENU);
	CHECK_FALSE(keys.Down(VK_MENU));
}

TEST_CASE("either side of Shift, Ctrl or Alt is down while the key is") {
	Agent::KeyState keys;
	keys.Observe(WM_KEYDOWN, VK_SHIFT);
	keys.Observe(WM_KEYDOWN, VK_CONTROL);
	keys.Observe(WM_SYSKEYDOWN, VK_MENU);
	CHECK(keys.Down(VK_LSHIFT));
	CHECK(keys.Down(VK_RSHIFT));
	CHECK(keys.Down(VK_LCONTROL));
	CHECK(keys.Down(VK_RCONTROL));
	CHECK(keys.Down(VK_LMENU));
	CHECK(keys.Down(VK_RMENU));
}

TEST_CASE("the answers carry the bits callers test") {
	Agent::KeyState keys;
	keys.Observe(WM_KEYDOWN, VK_SHIFT);
	// BH reads GetKeyState with "& 0x80" and the game with the sign.
	CHECK((keys.KeyStateOf(VK_LSHIFT) & 0x80) != 0);
	CHECK(keys.KeyStateOf(VK_SHIFT) < 0);
	CHECK((keys.AsyncKeyStateOf(VK_SHIFT) & 0x8000) != 0);
	CHECK(keys.KeyStateOf('A') == 0);
	CHECK(keys.AsyncKeyStateOf('A') == 0);
}

TEST_CASE("the keyboard state holds every key down, sides included") {
	Agent::KeyState keys;
	keys.Observe(WM_KEYDOWN, VK_SHIFT);
	keys.Observe(WM_KEYDOWN, 'M');
	unsigned char state[256];
	keys.Fill(state);
	CHECK(state[VK_SHIFT] == 0x80);
	CHECK(state[VK_LSHIFT] == 0x80);
	CHECK(state['M'] == 0x80);
	CHECK(state['N'] == 0);
	CHECK(state[VK_CONTROL] == 0);
}

TEST_CASE("mouse buttons are down between their messages") {
	Agent::KeyState keys;
	keys.Observe(WM_LBUTTONDOWN, MK_LBUTTON);
	CHECK(keys.Down(VK_LBUTTON));
	keys.Observe(WM_LBUTTONUP, 0);
	CHECK_FALSE(keys.Down(VK_LBUTTON));
	keys.Observe(WM_RBUTTONDOWN, MK_RBUTTON);
	CHECK(keys.Down(VK_RBUTTON));
}

TEST_CASE("other messages and keys out of range change nothing") {
	Agent::KeyState keys;
	keys.Observe(WM_MOUSEMOVE, 'A');
	keys.Observe(WM_CHAR, 'A');
	CHECK_FALSE(keys.Down('A'));
	CHECK_FALSE(keys.Down(-1));
	CHECK_FALSE(keys.Down(0x100));
}

TEST_CASE("nothing is drawn into the log unless it is recording") {
	Agent::DrawLog::Box(0, 0, 10, 10, 0, 0);
	Agent::DrawLog::Begin();
	Agent::DrawLog::Box(1, 2, 11, 22, 7, 5);
	std::vector<Agent::DrawRecord> frame = Agent::DrawLog::End();
	Agent::DrawLog::Box(0, 0, 10, 10, 0, 0);

	REQUIRE(frame.size() == 1);
	CHECK_FALSE(Agent::DrawLog::Recording());
	CHECK(Agent::DrawLog::End().empty());
}

TEST_CASE("a box is reported by its corner and size") {
	Agent::DrawLog::Begin();
	Agent::DrawLog::Box(1, 2, 11, 22, 7, 5);
	nlohmann::json json = Agent::ToJson(Agent::DrawLog::End()[0]);
	CHECK(json["kind"] == "box");
	CHECK(json["rect"] == nlohmann::json({1, 2, 10, 20}));
	CHECK(json["color"] == 7);
	CHECK(json["trans"] == 5);
}

TEST_CASE("text is placed by its top, where it was drawn by its baseline") {
	Agent::DrawLog::Begin();
	// Font 6 sits 7 pixels below the top of its line.
	Agent::DrawLog::Text(40, 107, 55, 6, 4, L"Codex");
	nlohmann::json json = Agent::ToJson(Agent::DrawLog::End()[0]);
	CHECK(json["kind"] == "text");
	CHECK(json["rect"] == nlohmann::json({40, 100, 55, 7}));
	CHECK(json["font"] == 6);
	CHECK(json["text"] == "Codex");
}

TEST_CASE("a frame's art reaches the same way past every side of the box it borders") {
	const int out = Drawing::kFrameOutset;
	Drawing::FrameBox box = {234, 566, 46, 15};
	Drawing::FrameBox art = Drawing::FrameBoxOf(Drawing::FrameRectFor(box));
	CHECK(out == 1);
	CHECK(art.x == box.x - out);
	CHECK(art.y == box.y - out);
	CHECK(art.width == box.width + 2 * out);
	CHECK(art.height == box.height + 2 * out);
}

TEST_CASE("a frame is reported by the box its art covers, with no colour") {
	Agent::DrawLog::Begin();
	Agent::DrawLog::Frame(232, 566, 279, 581);
	nlohmann::json json = Agent::ToJson(Agent::DrawLog::End()[0]);
	CHECK(json["kind"] == "frame");
	CHECK(json["rect"] == nlohmann::json({233, 565, 48, 17}));
	CHECK_FALSE(json.contains("color"));
	CHECK_FALSE(json.contains("trans"));
}

TEST_CASE("a line is reported by its ends") {
	Agent::DrawLog::Begin();
	Agent::DrawLog::Line(3, 4, 30, 40, 1);
	nlohmann::json json = Agent::ToJson(Agent::DrawLog::End()[0]);
	CHECK(json["from"] == nlohmann::json({3, 4}));
	CHECK(json["to"] == nlohmann::json({30, 40}));
	CHECK_FALSE(json.contains("rect"));
}

/*
 * Putting an item a session recorded down again in a sandboxed game.
 *
 * The packet is the one the server sent when an Identify Scroll landed in the
 * Worldstone Keep, as the item capture recorded it (fixtures/filter-cases.txt).
 */

namespace {

const std::vector<unsigned char> kScrollDrop = {
	0x9c, 0x00, 0x16, 0x10, 0x15, 0x01, 0x00, 0x00, 0x10, 0x20, 0xa0,
	0x00, 0x65, 0x8c, 0xcb, 0xe3, 0xaa, 0x22, 0x6d, 0x6e, 0x0c, 0x04,
};

unsigned int BitsAt(const std::vector<unsigned char>& packet, unsigned int at, unsigned int count) {
	BitReader reader(packet.data());
	reader.offset = at;
	return reader.read(count);
}

}  // namespace

TEST_CASE("a recorded drop is given its own unit and a place on the ground") {
	std::vector<unsigned char> packet = kScrollDrop;
	CHECK(Agent::PlaceDrop(packet, 0x7F000001, 5123, 4321) == "");
	CHECK(BitsAt(packet, 32, 32) == 0x7F000001);
	CHECK(BitsAt(packet, 109, 16) == 5123);
	CHECK(BitsAt(packet, 125, 16) == 4321);
}

TEST_CASE("placing a drop changes nothing else about the item") {
	std::vector<unsigned char> packet = kScrollDrop;
	Agent::PlaceDrop(packet, 0x7F000001, 5123, 4321);
	for (unsigned int bit = 0; bit < packet.size() * 8; bit++) {
		bool placed = (bit >= 32 && bit < 64) || (bit >= 109 && bit < 141);
		if (!placed)
			CHECK(BitsAt(packet, bit, 1) == BitsAt(kScrollDrop, bit, 1));
	}
}

TEST_CASE("only an item packet for the ground can be placed") {
	std::vector<unsigned char> other = kScrollDrop;
	other[0] = 0x9d;
	CHECK(Agent::PlaceDrop(other, 1, 1, 1) != "");

	std::vector<unsigned char> stored = kScrollDrop;
	stored[13] &= ~0x04;  // bit 106, the destination's lowest: 3 (ground) becomes 2
	CHECK(Agent::PlaceDrop(stored, 1, 1, 1) != "");

	std::vector<unsigned char> cut(kScrollDrop.begin(), kScrollDrop.begin() + 12);
	CHECK(Agent::PlaceDrop(cut, 1, 1, 1) != "");

	std::vector<unsigned char> mislabelled = kScrollDrop;
	mislabelled.push_back(0);
	CHECK(Agent::PlaceDrop(mislabelled, 1, 1, 1) != "");

	std::vector<unsigned char> offMap = kScrollDrop;
	CHECK(Agent::PlaceDrop(offMap, 1, 0x10000, 1) != "");
}

TEST_CASE("a packet arrives as hex, two digits to a byte") {
	std::vector<unsigned char> bytes;
	CHECK(Agent::BytesFromHex("9c0016Ab", bytes));
	CHECK(bytes == std::vector<unsigned char>({0x9c, 0x00, 0x16, 0xab}));
	CHECK_FALSE(Agent::BytesFromHex("9c0", bytes));
	CHECK_FALSE(Agent::BytesFromHex("9g", bytes));
	CHECK_FALSE(Agent::BytesFromHex("", bytes));
}
