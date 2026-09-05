#include "LoginScreen.h"
#include "../../D2Structs.h"
#include "../../Constants.h"

LoginBoxes FindLoginBoxes(Control* first) {
	Control* typed[3] = { NULL, NULL, NULL };
	unsigned int typedCount = 0;
	bool pressable = false;

	for (Control* control = first; control; control = control->pNext) {
		if (control->dwType == CONTROL_BUTTON) {
			pressable = true;
			continue;
		}
		if (control->dwType != CONTROL_EDITBOX)
			continue;
		// Counted past what the login screen has, so that a screen with more
		// boxes than it is told from it rather than read as it.
		if (typedCount < 3)
			typed[typedCount] = control;
		typedCount++;
	}

	LoginBoxes found;
	if (typedCount != 2 || !pressable)
		return found;

	// The name is typed above the password, which is the only thing that tells
	// the two apart: they are the same kind of control and the list is in
	// whatever order the screen built them.
	if (typed[0]->dwPosY == typed[1]->dwPosY)
		return found;

	bool firstIsUpper = typed[0]->dwPosY < typed[1]->dwPosY;
	found.account = firstIsUpper ? typed[0] : typed[1];
	found.password = firstIsUpper ? typed[1] : typed[0];
	return found;
}
