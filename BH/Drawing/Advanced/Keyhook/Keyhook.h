#pragma once

#include <string>
#include "../../Hook.h"

namespace Drawing {
	// Space between the border and the text inside it. The compact padding a box
	// on a settings row keeps, so a row carrying a binding is no taller than a row
	// carrying a box.
	#define KEY_PADDING_X			5
	#define KEY_PADDING_TOP			2
	#define KEY_PADDING_BOTTOM		2

	// How long the chip waits for a key after it is clicked. The countdown is
	// drawn in place of the binding, so this is also one of the things the chip
	// has to be wide enough to say.
	#define KEY_REBIND_SECONDS		3

	// A key binding, drawn as the cap you would press.
	//
	// The border is what says the binding can be changed: a binding drawn as bare
	// text is a click target with nothing to say so, and on a settings row it
	// reads as the value beside it rather than as something to press.
	class Keyhook : public Hook {
		private:
			unsigned int* key;//Pointer to the current key
			std::string name;//Name of the hotkey
			unsigned int timeout;//Timeout to change hotkey if clicked
			unsigned int xSize;//Column the chip is drawn in, 0 to fit its text

			// What the chip says: the countdown while it is waiting for a key, and
			// the binding otherwise.
			std::string Caption();

			// The countdown at a given number of seconds remaining. Shared by
			// drawing it and measuring how wide the chip has to be to hold it.
			static std::string CountdownText(unsigned int seconds);

		public:
			//Two Hook Initializations; one for basic hooks, one for grouped hooks.
			Keyhook(HookVisibility visibility, unsigned int x, unsigned int y, unsigned int* key, std::string hotkeyName);
			Keyhook(HookGroup* group, unsigned int x, unsigned int y, unsigned int* key, std::string hotkeyName);

			std::string GetName() { return name; };
			void SetName(std::string newName) { Lock(); name = newName; Unlock(); };

			unsigned int GetKey() { return *key; };
			void SetKey(unsigned int* newKey) { Lock(); key = newKey; Unlock(); };

			bool OnLeftClick(bool up, unsigned int x, unsigned int y);
			void OnDraw();
			bool OnKey(bool up, BYTE key, LPARAM lParam);

			// The column the chip is drawn in, fixed by whoever lays it out. A chip
			// that sized itself would change width the moment it was clicked and
			// the countdown replaced the binding, shoving the rest of the row
			// about; zero fits it to its text, for a chip standing on its own.
			void SetXSize(unsigned int width) { Lock(); xSize = width; Unlock(); };

			// The narrowest the chip can be drawn and still say everything it can be
			// asked to say - its binding, and the countdown that replaces it.
			// Public so that whoever lays it out can size a column to it.
			unsigned int GetContentWidth();

			unsigned int GetXSize();
			unsigned int GetYSize();
			unsigned int GetTextInset() { return KEY_PADDING_TOP; };
	};
};
