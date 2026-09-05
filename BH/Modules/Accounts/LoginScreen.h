#pragma once
#include <cstddef>

struct Control;

// The two boxes the login screen signs in with.
struct LoginBoxes {
	Control* account;
	Control* password;

	LoginBoxes() : account(NULL), password(NULL) {}

	bool Found() const { return account != NULL && password != NULL; }
};

// Which of the game's controls are the login screen's boxes, or nothing at all
// where what is in front of the player is some other screen.
//
// Told by the shape of the control list rather than by any address, so that
// nothing here has to be found by hand for each version of the game and a shape
// that stops matching costs a panel that does not appear rather than a crash.
// Nothing is found unless the list is exactly the login screen's: two boxes to
// type in and something to press. The account screen has three boxes to type in,
// a game to join by address has one, and choosing a character has none.
LoginBoxes FindLoginBoxes(Control* first);
