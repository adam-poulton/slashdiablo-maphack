#include "Inputhook.h"
#include "../../../D2Ptrs.h"
#include "../../Basic/Framehook/Framehook.h"
#include "../../../Common.h"

using namespace std;
using namespace Drawing;

// D2's inline color codes only cover the ten single digit colors, so anything
// outside that range falls back to white.
static std::string InlineColorCode(TextColor color) {
	int index = (int)color;
	if (index < 0 || index > 9)
		index = White;
	return "\377c" + std::to_string(index);
}

// Trim text to fit the given pixel width, so a long hint can't spill out of
// the box.
static std::string FitToWidth(const std::string& text, unsigned int font, unsigned int width) {
	if ((unsigned int)Texthook::GetTextSize(text, font).x <= width)
		return text;
	for (size_t length = text.length(); length > 0; length--) {
		std::string candidate = text.substr(0, length - 1);
		if ((unsigned int)Texthook::GetTextSize(candidate, font).x <= width)
			return candidate;
	}
	return "";
}

Inputhook::Inputhook(HookVisibility visibility, unsigned int x, unsigned int y, unsigned int xSize, std::string formatString, ...) :
 Hook(visibility, x, y) {
	SetXSize(xSize);
	SetFont(0);
	SetColor(Grey);
	SetFocusedColor(White);
	SetActive(false);
	submitted = false;
	SetCursorState(true);
	ResetCursorTick();
	ResetSelection();
	textPos = 0;
	char buffer[4096];
	va_list arg;
	va_start(arg, formatString);
	vsprintf_s(buffer, 4096, formatString.c_str(), arg);
	va_end(arg);
	text = buffer;
	SetCursorPosition(text.length());
}

Inputhook::Inputhook(HookGroup* group, unsigned int x, unsigned int y, unsigned int xSize, std::string formatString, ...) :
 Hook(group, x, y) {
	SetXSize(xSize);
	SetFont(0);
	SetColor(Grey);
	SetFocusedColor(White);
	SetActive(false);
	submitted = false;
	SetCursorState(true);
	ResetCursorTick();
	ResetSelection();
	textPos = 0;
	char buffer[4096];
	va_list arg;
	va_start(arg, formatString);
	vsprintf_s(buffer, 4096, formatString.c_str(), arg);
	va_end(arg);
	text = buffer;
	SetCursorPosition(text.length());
 }

 void Inputhook::SetText(string newText, ...) {
	char buffer[4096];
	va_list arg;
	va_start(arg, newText);
	vsprintf_s(buffer, 4096, newText.c_str(), arg);
	va_end(arg);
	text = buffer;
 }

 void Inputhook::SetFont(unsigned int newFont) {
	if (newFont >=  0 && newFont < 16) {
		Lock();
		font = newFont;
		Unlock();
	}
 }

 void Inputhook::SetXSize(unsigned int newXSize) {
	// if (newXSize > 0 && (Hook::GetScreenWidth() - GetX()) < newXSize) {
		 Lock();
		 xSize = newXSize;
		 Unlock();
	// }
 }

 void Inputhook::CursorTick() {
	  if (cursorTick % 30 == 0) { 
		  ResetCursorTick(); 
		  ToggleCursor(); 
	  }
	  cursorTick++;
 }

 void Inputhook::SetCursorPosition(unsigned int newPosition) {
	 if (newPosition >= 0 && newPosition <= text.length()) {
		Lock();
		cursorPos = newPosition;
		Unlock();
	 }
 }

 void Inputhook::SetSelectionPosition(unsigned int pos) {
	 if (pos < text.length()) {
		 Lock();
		 selectPos = pos;
		 Unlock();
	 }
 }

 void Inputhook::SetSelectionLength(unsigned int length) {
	 if (length <= text.length()) {
		 Lock();
		 selectLength = length;
		 Unlock();
	 }
 }

void Inputhook::IncreaseCursorPosition(unsigned int len) { 
	Lock();
	 SetCursorPosition(cursorPos + len); 
	 if ((textPos + GetCharacterLimit()) < cursorPos)
		 textPos = cursorPos - GetCharacterLimit();
	 Unlock();
};

void Inputhook::DecreaseCursorPosition(unsigned int len) { 
	Lock();
	SetCursorPosition(cursorPos - len); 
	 if ((cursorPos - textPos) == -1 && textPos > 0)
		 textPos -= len;
	Unlock();
}; 

unsigned int Inputhook::GetCharacterLimit() {
	return (GetXSize() / Texthook::GetTextSize("A", GetFont()).x);
}

 void Inputhook::OnDraw() {
	 Lock();
	 //Font height
	 unsigned int height[] = {10,11,18,24,10,13,7,13,10,12,8,8,7,12};

	 //A focused box gets a solid field, a second frame around it and a blinking
	 //cursor; an unfocused one is translucent with dimmed text, so it is obvious
	 //at a glance whether typing will go into the box.
	 bool focused = IsActive();
	 unsigned int boxHeight = height[GetFont()] + 4;
	 TextColor textColor = focused ? GetFocusedColor() : GetColor();

	 //Current text width
	 POINT textSize = Texthook::GetTextSize(GetText().substr(textPos, GetCursorPosition() - textPos), GetFont());

	 //Draw the outline box!
	 RECT pRect  = {static_cast<long>(GetX()), static_cast<long>(GetY()), static_cast<long>(GetX() + GetXSize()), static_cast<long>(GetY() + boxHeight)};
	 D2GFX_DrawRectangle(GetX(), GetY(), GetX() + GetXSize(), GetY() + boxHeight, 0, focused ? BTFull : BTOneHalf);
	 Framehook::DrawRectStub(&pRect);
	 if (focused) {
		 RECT pHalo = {static_cast<long>(GetX()) - 1, static_cast<long>(GetY()) - 1, static_cast<long>(GetX() + GetXSize()) + 1, static_cast<long>(GetY() + boxHeight) + 1};
		 Framehook::DrawRectStub(&pHalo);
	 }
	 //An empty box shows its hint instead, always dimmed so it doesn't read as
	 //text that is really in the box.
	 if (text.length() == 0 && placeholder.length() > 0) {
		 DWORD placeholderFont = D2WIN_SetTextSize(GetFont());
		 wchar_t* wHint = AnsiToUnicode(FitToWidth(placeholder, GetFont(), GetXSize() - 6).c_str());
		 D2WIN_DrawText(wHint, GetX() + 3, GetY() + 3 + height[GetFont()], Grey, 0);
		 delete[] wHint;
		 D2WIN_SetTextSize(placeholderFont);
	 }

	 string drawnText = text;

	 //Draw the text in!
	 int len = drawnText.length() - textPos;
	 if (len > (int)GetCharacterLimit())
		len = GetCharacterLimit();
	drawnText = drawnText.substr(textPos, len);


	 if (IsSelected()) {
		 //Reset to the base color rather than always to white, so an unfocused
		 //box stays dimmed after the selected run.
		 drawnText.insert(GetSelectionPosition() + GetSelectionLength(), InlineColorCode(textColor));
		 drawnText.insert(GetSelectionPosition(), "\377c9");
	 }


	 DWORD oldFont = D2WIN_SetTextSize(GetFont());
	 wchar_t* wText = AnsiToUnicode(drawnText.c_str());
	 D2WIN_DrawText(wText, GetX() + 3, GetY() + 3 + height[GetFont()], textColor, 0);
	 delete[] wText;
	 D2WIN_SetTextSize(oldFont);

	 //Draw the cursor!
	 CursorTick();
	 if (ShowCursor() && focused)
		 D2GFX_DrawLine(GetX() + textSize.x + 2, GetY() + 3, GetX() + textSize.x + 2, GetY() + textSize.y, 255, 0);

	 Unlock();
 }


 bool Inputhook::OnKey(bool up, BYTE key, LPARAM lParam) {
	 if (!IsActive())
		 return false;
	 Lock();
	 bool ctrlState = ((GetKeyState(VK_LCONTROL) & 0x80) || (GetKeyState(VK_RCONTROL) & 0x80));
	 bool shiftState = ((GetKeyState(VK_LSHIFT) & 0x80) || (GetKeyState(VK_RSHIFT) & 0x80));
	 switch(key) {
		case VK_BACK:
			if (!up)
				Backspace();
		break;
		case VK_DELETE:
			if (!up && text.length() != GetCursorPosition()) {
				Erase(GetCursorPosition(), 1);
			}
		break;
		case VK_ESCAPE:
			if (up)
				SetActive(false);
		break;
		case VK_RETURN:
			//Enter isn't text; hand it to the owner to act on.
			if (!up)
				submitted = true;
		break;
		case VK_LEFT:
			if (!up && GetCursorPosition() != 0) {
				if (shiftState) {
					if (IsSelected()) {
						if (GetSelectionPosition() == GetCursorPosition()) {
							SetSelectionPosition(GetSelectionPosition() - 1);
							SetSelectionLength(GetSelectionLength() + 1);
						} else {
							SetSelectionLength(GetSelectionLength() - 1);
						}
					} else {
						SetSelectionPosition(GetCursorPosition() - 1);
						SetSelectionLength(1);
					}
				}
				DecreaseCursorPosition(1);
			}
		break;
		case VK_RIGHT:
			if (!up && GetCursorPosition() != text.length()) {
				if (shiftState) {
					if (IsSelected()) {
						if (GetCursorPosition() == (GetSelectionPosition() + GetSelectionLength())) {
							SetSelectionLength(GetSelectionLength() + 1);
						} else {
							SetSelectionPosition(GetSelectionPosition() + 1);
							SetSelectionLength(GetSelectionLength() - 1);
						}
					} else {
						SetSelectionPosition(GetCursorPosition());
						SetSelectionLength(1);
					}
				}
				IncreaseCursorPosition(1);
			}
		break;
		default:
			if (up) {
				Unlock();
				return true;
			}

			if (ctrlState) {
				//Select All
				if (key == 0x41) {
					SetSelectionPosition(0);
					SetSelectionLength(text.length());
				}
				OpenClipboard(NULL);
				//Paste
				if (key == 0x56) {
					HANDLE pHandle = GetClipboardData(CF_TEXT);
					if (!pHandle) {
						CloseClipboard();
						Unlock();
						return true;
					}
					InputText((char*)GlobalLock(pHandle));
				}
				//Copy & Cut
				if (key == 0x43 || key == 0x58) {
					if (!IsSelected() || text.length() == 0) {
						CloseClipboard();
						Unlock();
						return true;
					}

					Lock();
					string mText = text.substr(GetSelectionPosition(), GetSelectionLength());
			
					HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, (mText.size() + 1) * sizeof(CHAR)); 
					char* szStr = (char*)GlobalLock(hGlobal);
					memcpy(szStr, mText.c_str(), mText.size() * sizeof(CHAR));
					GlobalUnlock(hGlobal);
					EmptyClipboard();
					SetClipboardData(CF_TEXT, hGlobal);

					if (key == 0x58) {
						Erase(GetSelectionPosition(), GetSelectionLength());

						ResetSelection();
					}
					Unlock();
				}
				CloseClipboard();
				Unlock();
				return true;
			}

			BYTE layout[256];
			WORD out[2];
			CHAR szChar[10];
			GetKeyboardState(layout);
			if (ToAscii(key, (lParam & 0xFF0000), layout, out, 0) == 0) {
				Unlock();
				return false;
			}
			//Only printable characters belong in the box; control codes like
			//return and tab would otherwise be inserted verbatim.
			if (out[0] < ' ' || out[0] == 0x7F) {
				Unlock();
				return true;
			}
			sprintf_s(szChar, sizeof(szChar), "%c", out[0]);

			InputText(szChar);
		break;
	 }
	 Unlock();
	 return true;
 }

 bool Inputhook::OnLeftClick(bool up, unsigned int x, unsigned int y) {
	 if (InRange(x, y)) {
		 //Take focus on the press, so the box responds the moment it is clicked.
		 if (!up)
			 SetActive(true);
		 if (GetLeftClickHandler())
			 GetLeftClickHandler()(up, this, GetLeftClickVoid());
		 return true;
	 } else {
		 SetActive(false);
	 }
	 return false;
 }

 bool Inputhook::OnRightClick(bool up, unsigned int x, unsigned int y) {
		if (!InRange(x, y))
			if (GetLeftClickHandler())
				return GetRightClickHandler()(up, this, GetRightClickVoid());
		return false;
 }

 void Inputhook::InputText(string newText) {
	 Lock();

	 //If we have text selected, replace the text with the new text
	 if (IsSelected()) {
		 Replace(GetSelectionPosition(), GetSelectionLength(), newText);
		 ResetSelection();
	 //Otherwise just add the text at the cursor position.
	 } else {
		 text.insert(GetCursorPosition(), newText);
		 IncreaseCursorPosition(newText.length());
	 }

	 Unlock();
 }

 void Inputhook::Backspace() {
	 if (GetCursorPosition() == 0)
		 return;
	 Lock();
	 if (IsSelected()) {
		 Erase(GetSelectionPosition(), GetSelectionLength());
		 ResetSelection();
	 } else {
		 text.erase(GetCursorPosition() - 1, 1);
		 DecreaseCursorPosition(1);
		 if (textPos > 0)
			textPos -= 1;
	 }
	 Unlock();
 }

void Inputhook::Replace(unsigned int pos, unsigned int len, std::string str) {
	if ((unsigned int)pos + len > text.length())
		return;
	Lock();
	text.replace(pos, len, str);
	SetCursorPosition(pos + str.length());
	Unlock();
}

void Inputhook::Erase(unsigned int pos, unsigned int len) {
	if ((unsigned int)pos + len > text.length())
		return;
	Lock();
	text.erase(pos,len);
	SetCursorPosition(pos);
	Unlock();
}