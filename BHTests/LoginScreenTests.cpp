#include "doctest.h"
#include <string>
#include <vector>
#include "Modules/Accounts/LoginScreen.h"
#include "D2Structs.h"
#include "Constants.h"

/*
 * Telling the login screen from every other screen the game shows.
 *
 * The controls are built here rather than read from a client, which is the whole
 * point of asking about the shape of the list: what counts as the login screen
 * can be said in a test, and a screen that is not it can be shown not to match.
 */

namespace {

// A list of controls in the order a screen built them, kept alive for as long as
// the test needs it.
class ControlList {
	private:
		std::vector<Control*> built;

	public:
		~ControlList() {
			for (unsigned int i = 0; i < built.size(); i++)
				delete built[i];
		}

		Control* Add(DWORD type, DWORD posY) {
			Control* control = new Control();
			memset(control, 0, sizeof(Control));
			control->dwType = type;
			control->dwPosY = posY;
			if (!built.empty())
				built.back()->pNext = control;
			built.push_back(control);
			return control;
		}

		Control* First() const { return built.empty() ? NULL : built[0]; }
};

}  // namespace

TEST_CASE("the login screen is two boxes to type in and something to press") {
	ControlList screen;
	Control* name = screen.Add(CONTROL_EDITBOX, 342);
	Control* password = screen.Add(CONTROL_EDITBOX, 396);
	screen.Add(CONTROL_BUTTON, 500);

	LoginBoxes found = FindLoginBoxes(screen.First());
	REQUIRE(found.Found());
	CHECK(found.account == name);
	CHECK(found.password == password);
}

TEST_CASE("the name is whichever box is typed above the other") {
	ControlList screen;
	// Built in the other order, which a screen is free to do.
	Control* password = screen.Add(CONTROL_EDITBOX, 396);
	Control* name = screen.Add(CONTROL_EDITBOX, 342);
	screen.Add(CONTROL_BUTTON, 500);

	LoginBoxes found = FindLoginBoxes(screen.First());
	REQUIRE(found.Found());
	CHECK(found.account == name);
	CHECK(found.password == password);
}

TEST_CASE("nothing is in front of the player at all") {
	CHECK_FALSE(FindLoginBoxes(NULL).Found());
}

TEST_CASE("the screen for making an account is not the login screen") {
	ControlList screen;
	screen.Add(CONTROL_EDITBOX, 300);
	screen.Add(CONTROL_EDITBOX, 340);
	screen.Add(CONTROL_EDITBOX, 380);
	screen.Add(CONTROL_BUTTON, 500);

	CHECK_FALSE(FindLoginBoxes(screen.First()).Found());
}

TEST_CASE("a screen with one box to type in is not the login screen") {
	ControlList screen;
	screen.Add(CONTROL_EDITBOX, 300);
	screen.Add(CONTROL_BUTTON, 500);

	CHECK_FALSE(FindLoginBoxes(screen.First()).Found());
}

TEST_CASE("a screen with nothing to type in is not the login screen") {
	ControlList screen;
	screen.Add(CONTROL_LIST, 200);
	screen.Add(CONTROL_BUTTON, 500);
	screen.Add(CONTROL_BUTTON, 520);

	CHECK_FALSE(FindLoginBoxes(screen.First()).Found());
}

TEST_CASE("two boxes with nothing to press is not the login screen") {
	ControlList screen;
	screen.Add(CONTROL_EDITBOX, 342);
	screen.Add(CONTROL_EDITBOX, 396);

	CHECK_FALSE(FindLoginBoxes(screen.First()).Found());
}

TEST_CASE("two boxes side by side are not a name above a password") {
	ControlList screen;
	screen.Add(CONTROL_EDITBOX, 342);
	screen.Add(CONTROL_EDITBOX, 342);
	screen.Add(CONTROL_BUTTON, 500);

	CHECK_FALSE(FindLoginBoxes(screen.First()).Found());
}

TEST_CASE("what else the screen holds is no business of the shape") {
	ControlList screen;
	screen.Add(CONTROL_IMAGE, 0);
	Control* name = screen.Add(CONTROL_EDITBOX, 342);
	screen.Add(CONTROL_TEXTBOX, 360);
	Control* password = screen.Add(CONTROL_EDITBOX, 396);
	screen.Add(CONTROL_IMAGE, 400);
	screen.Add(CONTROL_BUTTON, 500);
	screen.Add(CONTROL_BUTTON, 520);

	LoginBoxes found = FindLoginBoxes(screen.First());
	REQUIRE(found.Found());
	CHECK(found.account == name);
	CHECK(found.password == password);
}
