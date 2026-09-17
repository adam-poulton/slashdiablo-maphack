#include "LoginScreen.h"
#include "../../D2Structs.h"
#include "../../Constants.h"

namespace {

// The three ways onto the realm the login screen stacks under its boxes: sign
// in, make an account, ask for a new password. Counted as buttons of one size
// sharing a column, weighed against each other rather than against any
// measurement of the screen, so this holds at whatever resolution the game is
// running.
#define LOGIN_SCREEN_CHOICES 3

bool HasColumnOfChoices(Control* first) {
	for (Control* button = first; button; button = button->pNext) {
		if (button->dwType != CONTROL_BUTTON)
			continue;

		unsigned int inColumn = 1;
		for (Control* below = button->pNext; below; below = below->pNext) {
			if (below->dwType == CONTROL_BUTTON &&
				below->dwPosX == button->dwPosX &&
				below->dwSizeX == button->dwSizeX &&
				below->dwSizeY == button->dwSizeY)
				inColumn++;
		}
		if (inColumn >= LOGIN_SCREEN_CHOICES)
			return true;
	}
	return false;
}

}  // namespace

LoginBoxes FindLoginBoxes(Control* first) {
	Control* typed[3] = { NULL, NULL, NULL };
	unsigned int typedCount = 0;

	for (Control* control = first; control; control = control->pNext) {
		// Nothing on the login screen scrolls. The realm's screens are built
		// around lists that do, so one of these says at a glance that what is in
		// front of the player is far bigger than a sign-in.
		if (control->dwType == CONTROL_SCROLLBAR)
			return LoginBoxes();

		if (control->dwType != CONTROL_EDITBOX)
			continue;
		// Counted past what the login screen has, so that a screen with more
		// boxes than it is told from it rather than read as it.
		if (typedCount < 3)
			typed[typedCount] = control;
		typedCount++;
	}

	LoginBoxes found;
	if (typedCount != 2)
		return found;

	// The name is typed above the password, which is the only thing that tells
	// the two apart: they are the same kind of control and the list is in
	// whatever order the screen built them.
	if (typed[0]->dwPosY == typed[1]->dwPosY)
		return found;

	// One box above the other in the same column, both cut to the same size: a
	// name typed over a password and nothing else. Two boxes that sit apart or
	// differ in size belong to a screen that merely happens to have two, which is
	// what the channel selector is.
	if (typed[0]->dwPosX != typed[1]->dwPosX ||
		typed[0]->dwSizeX != typed[1]->dwSizeX ||
		typed[0]->dwSizeY != typed[1]->dwSizeY)
		return found;

	// Asking for a new password and registering an email are the same two boxes
	// stacked the same way, and are told from signing in by what sits under them.
	// Both are a screen to get through rather than a way in: one offers a button
	// to send and a button to go back, apart in their own corners, the other two
	// choices where the login screen has three.
	if (!HasColumnOfChoices(first))
		return found;

	bool firstIsUpper = typed[0]->dwPosY < typed[1]->dwPosY;
	found.account = firstIsUpper ? typed[0] : typed[1];
	found.password = firstIsUpper ? typed[1] : typed[0];
	return found;
}
