#include "AgentKeys.h"

#include <Windows.h>

namespace Agent {

KeyState::KeyState() {
	ReleaseAll();
}

void KeyState::ReleaseAll() {
	for (std::atomic<bool>& key : down)
		key = false;
}

void KeyState::Observe(unsigned int message, unsigned int wParam) {
	switch (message) {
		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
			down[wParam & 0xFF] = true;
			break;
		case WM_KEYUP:
		case WM_SYSKEYUP:
			down[wParam & 0xFF] = false;
			break;
		case WM_LBUTTONDOWN: down[VK_LBUTTON] = true; break;
		case WM_LBUTTONUP: down[VK_LBUTTON] = false; break;
		case WM_RBUTTONDOWN: down[VK_RBUTTON] = true; break;
		case WM_RBUTTONUP: down[VK_RBUTTON] = false; break;
		case WM_MBUTTONDOWN: down[VK_MBUTTON] = true; break;
		case WM_MBUTTONUP: down[VK_MBUTTON] = false; break;
	}
}

bool KeyState::Down(int virtualKey) const {
	if (virtualKey < 0 || virtualKey > 0xFF)
		return false;
	switch (virtualKey) {
		case VK_LSHIFT: case VK_RSHIFT: return down[VK_SHIFT];
		case VK_LCONTROL: case VK_RCONTROL: return down[VK_CONTROL];
		case VK_LMENU: case VK_RMENU: return down[VK_MENU];
		default: return down[virtualKey];
	}
}

short KeyState::KeyStateOf(int virtualKey) const {
	return Down(virtualKey) ? (short)0xFF80 : 0;
}

short KeyState::AsyncKeyStateOf(int virtualKey) const {
	return Down(virtualKey) ? (short)0x8000 : 0;
}

void KeyState::Fill(unsigned char state[256]) const {
	for (int key = 0; key < 256; key++)
		state[key] = Down(key) ? 0x80 : 0;
}

}
