#pragma once

#include <string>
#include "../../Hook.h"
#include "../../Basic/Texthook/Texthook.h"

namespace Drawing {
	// Space between the border and the text inside it. A little more above than
	// below, since the glyphs sit on their baseline and reach nearer the top.
	#define INPUT_PADDING_X			5
	#define INPUT_PADDING_TOP		5
	#define INPUT_PADDING_BOTTOM	3

	class Inputhook : public Hook {
		private:
			std::string text; //Text that is actually in the input box
			std::string placeholder; //Hint drawn while the box is empty
			bool submitted; //Set when enter is pressed, until the owner reads it
			bool clearOnFocus; //Empty the box when it is clicked into
			bool focused, showCursor; //Booleans set if the box has focus / currently showing cursor.
			unsigned int xSize; //Length of the input box
			unsigned int cursorPos, cursorTick; //Cursor Position / Timer to control cursor blink
			unsigned int textPos;//Used to determine which part of the current text I should show
			unsigned int selectPos, selectLength; // Selection position and length
			unsigned int font; //What type of font to use in the input hook.
			TextColor color, focusedColor; //Text color when idle / when focused.
		public:
			Inputhook(HookVisibility visibility, unsigned int x, unsigned int y, unsigned int xSize, std::string formatString, ...);
			Inputhook(HookGroup* group, unsigned int x, unsigned int y, unsigned int xSize, std::string formatString, ...);

			//Getters and Setters

			//Text in the input box
			std::string GetText() { return text; };
			void SetText(std::string newText, ...);

			//If the box has focus (can be typed in). Separate from Hook::IsActive(),
			//which controls whether the box is shown at all.
			bool IsFocused() { return focused; };
			void SetFocused(bool isFocused) { Lock(); focused = isFocused; Unlock(); };

			//Font Size
			unsigned int GetFont() { return font; };
			void SetFont(unsigned int newFont);

			//Text color while the box does not have focus
			TextColor GetColor() { return color; };
			void SetColor(TextColor newColor) { Lock(); color = newColor; Unlock(); };

			//Text color while the box has focus
			TextColor GetFocusedColor() { return focusedColor; };
			void SetFocusedColor(TextColor newColor) { Lock(); focusedColor = newColor; Unlock(); };

			//X Size
			unsigned int GetXSize() { return xSize; };
			void SetXSize(unsigned int newXSize);

			//Y Size. This is the height of the box as drawn, not just of the
			//text, so that the clickable area matches what the user sees.
			unsigned int GetYSize() { unsigned int height[] = {10,11,18,24,10,13,7,13,10,12,8,8,7,12}; return height[GetFont()] + INPUT_PADDING_TOP + INPUT_PADDING_BOTTOM; };

			//Hint shown in place of the text while the box is empty
			std::string GetPlaceholder() { return placeholder; };
			void SetPlaceholder(std::string newPlaceholder) { Lock(); placeholder = newPlaceholder; Unlock(); };

			//True once if enter has been pressed since the last call. Enter is
			//consumed rather than typed into the box so the owner can act on it.
			bool TakeSubmitted() { Lock(); bool was = submitted; submitted = false; Unlock(); return was; };

			//Whether clicking into the box empties it, for a box that is normally
			//retyped from scratch rather than edited.
			bool GetClearOnFocus() { return clearOnFocus; };
			void SetClearOnFocus(bool clear) { Lock(); clearOnFocus = clear; Unlock(); };

			//Empties the box and puts the cursor back at the start.
			void Clear();

			//If we are current showing the cursor, for blinking purposes!
			bool ShowCursor() { return showCursor; };
			void SetCursorState(bool state) { Lock(); showCursor = state; Unlock(); };
			void ToggleCursor() { SetCursorState(!ShowCursor()); };

			void CursorTick();
			void ResetCursorTick() { cursorTick = 0; };

			unsigned int GetCursorPosition() { return cursorPos; };
			void SetCursorPosition(unsigned int newPosition);
			void IncreaseCursorPosition(unsigned int len);
			void DecreaseCursorPosition(unsigned int len);

			unsigned int GetSelectionPosition() { return selectPos; };
			void SetSelectionPosition(unsigned int pos);

			unsigned int GetSelectionLength() { return selectLength; };
			void SetSelectionLength(unsigned int length);

			bool IsSelected() { return selectLength > 0; };
			void ResetSelection() { Lock(); selectPos = 0; selectLength = 0; Unlock(); };
		
			bool OnKey(bool up, BYTE key, LPARAM lParam);
			bool OnLeftClick(bool up, unsigned int x, unsigned int y);
			bool OnRightClick(bool up, unsigned int x, unsigned int y);

			unsigned int GetCharacterLimit();

			unsigned int GetTextPos() { return textPos; };
			void SetTextPos(unsigned int pos) { Lock(); textPos = pos; Unlock(); };

			void OnDraw();

			void InputText(std::string newText);
			void Backspace();
			void Replace(unsigned int pos, unsigned int len, std::string str);
			void Erase(unsigned int pos, unsigned int len);
	};
};