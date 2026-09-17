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
// Nothing is found unless the list is exactly the login screen's: nothing that
// scrolls, two boxes to type in of one size stacked in one column, and the three
// ways onto the realm stacked under them. The account screen has three boxes to
// type in, a game to join by address has one, and choosing a character has none.
// The channel selector has two, but they are a chat line and a channel name
// sharing neither column nor size, and it scrolls. Asking for a new password and
// registering an email both stack the two boxes as here, and are turned away by
// what sits beneath: a button to send and a button to go back in their own
// corners, and a column of two.
LoginBoxes FindLoginBoxes(Control* first);
