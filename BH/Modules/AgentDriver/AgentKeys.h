#pragma once
#include <atomic>

namespace Agent {

// Which keys and mouse buttons are down, as told by the messages the game
// window receives. In a sandbox the window sits on a desktop no keyboard is
// attached to, so every key message it gets is one the agent posted, and
// posted messages never reach the state GetKeyState reads. This is what
// answers instead.
class KeyState {
	public:
		KeyState();

		// Reads WM_KEYDOWN, WM_KEYUP, WM_SYSKEYDOWN, WM_SYSKEYUP and the mouse
		// button messages; everything else is ignored.
		void Observe(unsigned int message, unsigned int wParam);

		bool Down(int virtualKey) const;

		// What GetKeyState and GetAsyncKeyState return for the key, to the bit:
		// GetKeyState answers 0xFF80 for a key that is down, which code reading
		// "& 0x80" (BH included) depends on, and GetAsyncKeyState 0x8000. A left
		// or right Shift, Ctrl or Alt is down while the key it is a side of is,
		// since a posted message names only the key.
		short KeyStateOf(int virtualKey) const;
		short AsyncKeyStateOf(int virtualKey) const;

		// What GetKeyboardState fills in: 0x80 for every key down, left and right
		// sides included, which is what ToAscii reads Shift from when BH's
		// boxes turn a key into a character.
		void Fill(unsigned char state[256]) const;

		void ReleaseAll();

	private:
		std::atomic<bool> down[256];
};

}
