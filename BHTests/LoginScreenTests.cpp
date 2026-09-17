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
 *
 * The screens are built to the measurements of a client running at 800x600, read
 * off its control lists. They are laid out here as the game lays them out, so a
 * rule that stops telling them apart fails here rather than in front of a player.
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

		Control* Add(DWORD type, DWORD posX, DWORD posY, DWORD sizeX, DWORD sizeY) {
			Control* control = new Control();
			memset(control, 0, sizeof(Control));
			control->dwType = type;
			control->dwPosX = posX;
			control->dwPosY = posY;
			control->dwSizeX = sizeX;
			control->dwSizeY = sizeY;
			if (!built.empty())
				built.back()->pNext = control;
			built.push_back(control);
			return control;
		}

		Control* First() const { return built.empty() ? NULL : built[0]; }
};

// The two boxes a screen was built with, so a test can say which of them the
// shape ought to have picked.
struct Typed {
	Control* upper;
	Control* lower;

	Typed() : upper(NULL), lower(NULL) {}
};

// The three ways onto the realm the login screen stacks under its boxes: signing
// in, making an account, asking for a new password. The fourth button is the way
// back out, off in its own corner.
void AddLoginChoices(ControlList& screen) {
	screen.Add(CONTROL_BUTTON, 264, 484, 272, 35);
	screen.Add(CONTROL_BUTTON, 264, 528, 272, 35);
	screen.Add(CONTROL_BUTTON, 264, 572, 272, 35);
	screen.Add(CONTROL_BUTTON, 33, 572, 128, 35);
}

// The login screen: a name typed over a password, the three choices beneath.
Typed BuildLoginScreen(ControlList& screen) {
	screen.Add(CONTROL_IMAGE, 0, 599, 800, 600);
	screen.Add(CONTROL_IMAGE, 319, 347, 169, 26);
	screen.Add(CONTROL_IMAGE, 319, 401, 169, 26);
	screen.Add(CONTROL_TEXTBOX, 321, 340, 300, 32);
	screen.Add(CONTROL_TEXTBOX, 321, 394, 300, 32);

	Typed typed;
	typed.upper = screen.Add(CONTROL_EDITBOX, 322, 342, 162, 19);
	typed.lower = screen.Add(CONTROL_EDITBOX, 322, 396, 162, 19);
	AddLoginChoices(screen);
	return typed;
}

}  // namespace

TEST_CASE("the login screen is found by its shape") {
	ControlList screen;
	Typed typed = BuildLoginScreen(screen);

	LoginBoxes found = FindLoginBoxes(screen.First());
	REQUIRE(found.Found());
	CHECK(found.account == typed.upper);
	CHECK(found.password == typed.lower);
}

TEST_CASE("the name is whichever box is typed above the other") {
	ControlList screen;
	// Built in the other order, which a screen is free to do.
	Control* password = screen.Add(CONTROL_EDITBOX, 322, 396, 162, 19);
	Control* name = screen.Add(CONTROL_EDITBOX, 322, 342, 162, 19);
	AddLoginChoices(screen);

	LoginBoxes found = FindLoginBoxes(screen.First());
	REQUIRE(found.Found());
	CHECK(found.account == name);
	CHECK(found.password == password);
}

TEST_CASE("what else the screen holds is no business of the shape") {
	ControlList screen;
	screen.Add(CONTROL_IMAGE, 0, 0, 800, 600);
	Control* name = screen.Add(CONTROL_EDITBOX, 322, 342, 162, 19);
	screen.Add(CONTROL_TEXTBOX, 321, 360, 300, 32);
	Control* password = screen.Add(CONTROL_EDITBOX, 322, 396, 162, 19);
	screen.Add(CONTROL_IMAGE, 200, 400, 400, 100);
	AddLoginChoices(screen);
	// A dialog the screen put up over itself, which it is still the screen under.
	screen.Add(CONTROL_BUTTON, 351, 337, 96, 32);

	LoginBoxes found = FindLoginBoxes(screen.First());
	REQUIRE(found.Found());
	CHECK(found.account == name);
	CHECK(found.password == password);
}

TEST_CASE("nothing is in front of the player at all") {
	CHECK_FALSE(FindLoginBoxes(NULL).Found());
}

TEST_CASE("the channel selector is not the login screen") {
	ControlList screen;
	// A chat line along the bottom and a channel name up in the corner: two
	// boxes that share neither column nor size.
	screen.Add(CONTROL_EDITBOX, 28, 450, 335, 32);
	screen.Add(CONTROL_EDITBOX, 432, 162, 155, 20);
	screen.Add(CONTROL_SCROLLBAR, 400, 390, 10, 314);
	screen.Add(CONTROL_SCROLLBAR, 753, 393, 10, 197);
	screen.Add(CONTROL_BUTTON, 433, 433, 96, 32);
	screen.Add(CONTROL_BUTTON, 671, 433, 96, 32);

	CHECK_FALSE(FindLoginBoxes(screen.First()).Found());
}

TEST_CASE("asking for a new password is not the login screen") {
	ControlList screen;
	// The same two boxes stacked the same way. What tells it apart is beneath:
	// one button to send the request and one to go back, in opposite corners.
	screen.Add(CONTROL_EDITBOX, 251, 422, 293, 19);
	screen.Add(CONTROL_EDITBOX, 251, 472, 293, 19);
	screen.Add(CONTROL_BUTTON, 627, 572, 128, 35);
	screen.Add(CONTROL_BUTTON, 33, 572, 128, 35);

	CHECK_FALSE(FindLoginBoxes(screen.First()).Found());
}

TEST_CASE("registering an email is not the login screen") {
	ControlList screen;
	// Two boxes stacked where the login screen stacks its own, under a column of
	// two where the login screen offers three.
	screen.Add(CONTROL_EDITBOX, 253, 342, 293, 19);
	screen.Add(CONTROL_EDITBOX, 253, 396, 293, 19);
	screen.Add(CONTROL_BUTTON, 265, 527, 272, 35);
	screen.Add(CONTROL_BUTTON, 265, 572, 272, 35);
	screen.Add(CONTROL_BUTTON, 33, 572, 128, 35);

	CHECK_FALSE(FindLoginBoxes(screen.First()).Found());
}

TEST_CASE("the screen for making an account is not the login screen") {
	ControlList screen;
	screen.Add(CONTROL_EDITBOX, 322, 300, 162, 19);
	screen.Add(CONTROL_EDITBOX, 322, 340, 162, 19);
	screen.Add(CONTROL_EDITBOX, 322, 380, 162, 19);
	AddLoginChoices(screen);

	CHECK_FALSE(FindLoginBoxes(screen.First()).Found());
}

TEST_CASE("a screen with one box to type in is not the login screen") {
	ControlList screen;
	screen.Add(CONTROL_EDITBOX, 322, 342, 162, 19);
	AddLoginChoices(screen);

	CHECK_FALSE(FindLoginBoxes(screen.First()).Found());
}

TEST_CASE("a screen with nothing to type in is not the login screen") {
	ControlList screen;
	screen.Add(CONTROL_LIST, 200, 200, 400, 200);
	AddLoginChoices(screen);

	CHECK_FALSE(FindLoginBoxes(screen.First()).Found());
}

TEST_CASE("two boxes with nothing to press is not the login screen") {
	ControlList screen;
	screen.Add(CONTROL_EDITBOX, 322, 342, 162, 19);
	screen.Add(CONTROL_EDITBOX, 322, 396, 162, 19);

	CHECK_FALSE(FindLoginBoxes(screen.First()).Found());
}

TEST_CASE("two boxes under fewer choices than a way in offers") {
	ControlList screen;
	screen.Add(CONTROL_EDITBOX, 322, 342, 162, 19);
	screen.Add(CONTROL_EDITBOX, 322, 396, 162, 19);
	screen.Add(CONTROL_BUTTON, 264, 528, 272, 35);
	screen.Add(CONTROL_BUTTON, 264, 572, 272, 35);

	CHECK_FALSE(FindLoginBoxes(screen.First()).Found());
}

TEST_CASE("buttons apart in their own corners are not a column of choices") {
	ControlList screen;
	screen.Add(CONTROL_EDITBOX, 322, 342, 162, 19);
	screen.Add(CONTROL_EDITBOX, 322, 396, 162, 19);
	// Three of them, but no two of them stacked.
	screen.Add(CONTROL_BUTTON, 33, 572, 128, 35);
	screen.Add(CONTROL_BUTTON, 336, 572, 128, 35);
	screen.Add(CONTROL_BUTTON, 627, 572, 128, 35);

	CHECK_FALSE(FindLoginBoxes(screen.First()).Found());
}

TEST_CASE("two boxes side by side are not a name above a password") {
	ControlList screen;
	screen.Add(CONTROL_EDITBOX, 222, 342, 162, 19);
	screen.Add(CONTROL_EDITBOX, 422, 342, 162, 19);
	AddLoginChoices(screen);

	CHECK_FALSE(FindLoginBoxes(screen.First()).Found());
}

TEST_CASE("two boxes in different columns are not stacked") {
	ControlList screen;
	screen.Add(CONTROL_EDITBOX, 222, 342, 162, 19);
	screen.Add(CONTROL_EDITBOX, 422, 396, 162, 19);
	AddLoginChoices(screen);

	CHECK_FALSE(FindLoginBoxes(screen.First()).Found());
}

TEST_CASE("two boxes cut to different sizes are not a name above a password") {
	ControlList screen;
	screen.Add(CONTROL_EDITBOX, 322, 342, 162, 19);
	screen.Add(CONTROL_EDITBOX, 322, 396, 293, 19);
	AddLoginChoices(screen);

	CHECK_FALSE(FindLoginBoxes(screen.First()).Found());
}

TEST_CASE("a screen that scrolls is not the login screen") {
	ControlList screen;
	screen.Add(CONTROL_EDITBOX, 322, 342, 162, 19);
	screen.Add(CONTROL_EDITBOX, 322, 396, 162, 19);
	AddLoginChoices(screen);
	screen.Add(CONTROL_SCROLLBAR, 585, 457, 10, 363);

	CHECK_FALSE(FindLoginBoxes(screen.First()).Found());
}
