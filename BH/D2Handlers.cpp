#include "D2Ptrs.h"
#include "BH.h"
#include "D2Stubs.h"
#include "Constants.h"

#include <iterator>

void GameDraw() {
	__raise BH::moduleManager->OnDraw();
	Drawing::UI::Draw(Drawing::InGame);
	Drawing::StatsDisplay::Draw();
	Drawing::Hook::Draw(Drawing::InGame);
}

void GameAutomapDraw() {
	__raise BH::moduleManager->OnAutomapDraw();
}

void OOGDraw() {
	Drawing::Hook::Draw(Drawing::OutOfGame);
	// The modules before the windows, as GameDraw() does it. A window is drawn
	// from whatever its panels last laid out, so a panel that has not been given
	// its frame yet is one drawn before it has any columns, any rows or any size.
	__raise BH::moduleManager->OnOOGDraw();
	Drawing::UI::Draw(Drawing::OutOfGame);
}
 
void GameLoop() {
	__raise BH::moduleManager->OnLoop();
}

DWORD WINAPI GameThread(VOID* lpvoid) {
	bool inGame = false;
	while(true) {
		if ((*p_D2WIN_FirstControl) && inGame) {
			inGame = false;
			__raise BH::moduleManager->OnGameExit();
			BH::config->Write();
			BH::oogDraw->Install();
		} else if (D2CLIENT_GetPlayerUnit() && !inGame) {
			inGame = true;
			__raise BH::moduleManager->OnGameJoin();
			BH::oogDraw->Remove();
		}
		Sleep(10);
	}
}

// Whether the character that follows the last key belongs to one of our controls.
//
// Blocking a keydown is not enough to keep the key out of the game. The menus'
// own boxes are typed into by WM_CHAR, and TranslateMessage posts that character
// from the keydown before the keydown is ever dispatched, so the character is
// already on its way when we decide to swallow the key it came from. A key one of
// our controls took has to take its character with it.
static bool charBelongsToHook = false;

LONG WINAPI GameWindowEvent(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {

	bool blockEvent = false;

	if (uMsg == WM_CHAR || uMsg == WM_SYSCHAR) {
		if (charBelongsToHook)
			return NULL;
	}

	// Which screen the message arrived on. Everything drawn says which screen it
	// is drawn on, and answers input on that screen and no other, so a window
	// laid out for a game cannot take a click on the login screen and a panel
	// laid out for the login screen cannot take one in a game.
	bool inGame = D2CLIENT_GetPlayerUnit() != NULL;
	Drawing::HookVisibility screen = inGame ? Drawing::InGame : Drawing::OutOfGame;

	// Outside a game D2Client is not running its input loop, so the position
	// everything is hit tested against is never updated and every click lands
	// wherever the cursor last was inside a game. The window is told where the
	// cursor is by every mouse message it gets, so that is where it is taken
	// from instead. In a game the game's own position still answers.
	if (!inGame &&
			(uMsg == WM_MOUSEMOVE || uMsg == WM_LBUTTONDOWN || uMsg == WM_LBUTTONUP ||
			 uMsg == WM_RBUTTONDOWN || uMsg == WM_RBUTTONUP)) {
		Drawing::Hook::SetMousePosition((int)(short)LOWORD(lParam),
			(int)(short)HIWORD(lParam));
	}

	int mouseX = Drawing::Hook::GetMouseX();
	int mouseY = Drawing::Hook::GetMouseY();

	if (uMsg == WM_LBUTTONDOWN) {
		if (Drawing::Hook::LeftClick(screen, false, mouseX, mouseY))
			blockEvent = true;
		if (Drawing::UI::LeftClick(screen, false, mouseX, mouseY))
			blockEvent = true;
		if (inGame && Drawing::StatsDisplay::Click(false, mouseX, mouseY))
			blockEvent = true;
		__raise BH::moduleManager->OnLeftClick(false, mouseX, mouseY, &blockEvent);
	}

	if (uMsg == WM_LBUTTONUP) {
		if (Drawing::Hook::LeftClick(screen, true, mouseX, mouseY))
			blockEvent = true;
		if (Drawing::UI::LeftClick(screen, true, mouseX, mouseY))
			blockEvent = true;
		if (inGame && Drawing::StatsDisplay::Click(true, mouseX, mouseY))
			blockEvent = true;
		__raise BH::moduleManager->OnLeftClick(true, mouseX, mouseY, &blockEvent);
	}

	if (uMsg == WM_RBUTTONDOWN) {
		if (Drawing::Hook::RightClick(screen, false, mouseX, mouseY))
			blockEvent = true;
		if (Drawing::UI::RightClick(screen, false, mouseX, mouseY))
			blockEvent = true;
		if (inGame && Drawing::StatsDisplay::Click(false, mouseX, mouseY))
			blockEvent = true;
		__raise BH::moduleManager->OnRightClick(false, mouseX, mouseY, &blockEvent);
	}

	if (uMsg == WM_RBUTTONUP) {
		if (Drawing::Hook::RightClick(screen, true, mouseX, mouseY))
			blockEvent = true;
		if (Drawing::UI::RightClick(screen, true, mouseX, mouseY))
			blockEvent = true;
		if (inGame && Drawing::StatsDisplay::Click(true, mouseX, mouseY))
			blockEvent = true;
		__raise BH::moduleManager->OnRightClick(true, mouseX, mouseY, &blockEvent);
	}

	if (uMsg == WM_MOUSEWHEEL) {
		// The wheel arrives in multiples of WHEEL_DELTA. The position it reports is in screen coordinates,
		// so use the game's cursor position like every other handler here.
		int notches = (int)(short)HIWORD(wParam) / WHEEL_DELTA;
		if (notches != 0 && Drawing::Hook::MouseWheel(screen, notches, mouseX, mouseY))
			blockEvent = true;
	}

	// In a game, nothing is offered a key while the chat box is open, which is
	// what keeps a typed message out of the hotkeys. Out of a game there is no
	// chat box, and no hotkey either: only what is drawn on the screen in front
	// of the player answers, which is how a panel on the login screen comes to be
	// typed into without the game's own boxes losing anything.
	//
	// BH's window procedure runs before the game's and returns without calling it
	// when something takes the key, so a control of ours out of a game really does
	// take the keystroke out of the game's hands. That is only safe because the
	// controls stand down while the game's own box has the caret.
	bool typing = inGame && D2CLIENT_GetUIState(UI_CHAT_CONSOLE);
	if (!typing) {
		if (uMsg == WM_KEYDOWN) {
			// Set either way: a key nothing took leaves its character to the
			// game, and a key taken a moment ago must not go on swallowing
			// characters it had nothing to do with.
			charBelongsToHook = Drawing::Hook::KeyClick(screen, false, wParam, lParam);
			if (charBelongsToHook)
				return NULL;
			if (inGame) {
				if (Drawing::StatsDisplay::KeyClick(false, wParam, lParam))
					return NULL;
				__raise BH::moduleManager->OnKey(false, wParam, lParam, &blockEvent);
			}
		}

		if (uMsg == WM_KEYUP) {
			if (Drawing::Hook::KeyClick(screen, true, wParam, lParam))
				return NULL;
			if (inGame) {
				if (Drawing::StatsDisplay::KeyClick(true, wParam, lParam))
					return NULL;
				__raise BH::moduleManager->OnKey(true, wParam, lParam, &blockEvent);
			}
		}
	}

	return blockEvent ? NULL : (LONG)CallWindowProcA(BH::OldWNDPROC, hWnd, uMsg, wParam, lParam);
}

BOOL ChatPacketRecv(DWORD dwSize,BYTE* pPacket) {
	bool blockPacket = false;
	__raise BH::moduleManager->OnChatPacketRecv(pPacket, &blockPacket);
	return !blockPacket;
}

BOOL __fastcall RealmPacketRecv(BYTE* pPacket) {
	bool blockPacket = false;
	__raise BH::moduleManager->OnRealmPacketRecv(pPacket, &blockPacket);
	return !blockPacket;
}

DWORD __fastcall GamePacketRecv(BYTE* pPacket, DWORD dwSize) {
	switch(pPacket[0])
	{
		case 0xAE: if(!BH::cGuardLoaded) return false; break;
		case 0x26: {
			char* pName = (char*)pPacket+10;
			char* pMessage = (char*)pPacket + strlen(pName) + 11;
			bool blockMessage = false;
			__raise BH::moduleManager->OnChatMsg(pName, pMessage, true, &blockMessage);
			} break;
	}
	bool blockPacket = false;
	__raise BH::moduleManager->OnGamePacketRecv(pPacket, &blockPacket);
	return !blockPacket;
}

DWORD __fastcall GameInput(wchar_t* wMsg)
{
	bool hasCmd = wcslen(wMsg) > 1 && wMsg[0] == '.';
	if(hasCmd)
	{
		wchar_t *buf = wMsg+1, *ctx = NULL, *seps = L" ";
		wchar_t* token = wcstok_s(buf, seps, &ctx);
		wchar_t* wparam = buf+wcslen(token)+1;
		int len = wcslen(wparam)+1;
		if(len > 0)
		{
			if(!BH::moduleManager->UserInput(token, wparam, true)) hasCmd = false;
		}
	}

	return hasCmd ? -1 : 0;
}

DWORD __fastcall ChannelInput(wchar_t* wMsg)
{
	bool hasCmd = wcslen(wMsg) > 1 && wMsg[0] == '.';
	if(hasCmd)
	{
		wchar_t *buf = wMsg+1, *ctx = NULL, *seps = L" ";
		wchar_t* token = wcstok_s(buf, seps, &ctx);
		wchar_t* wparam = buf+wcslen(token)+1;
		int len = wcslen(wparam)+1;
		if(len > 0)
		{
			if(!BH::moduleManager->UserInput(token, wparam, false)) hasCmd = false;
			D2WIN_SetControlText(*p_D2MULTI_ChatInputBox, L"");
		}
	}

	return hasCmd ? FALSE : TRUE;
}

BOOL __fastcall ChatHandler(char* user, char* msg)
{
	bool block = false;
	__raise BH::moduleManager->OnChatMsg(user, msg, false, &block);
	return block;
}