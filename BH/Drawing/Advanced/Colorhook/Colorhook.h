#pragma once

#include "../../../Constants.h"
#include "../../Hook.h"

namespace Drawing {
	class Colorhook;

	class Colorhook : public Hook {
		private:
			std::string text;//Text to have linked
			unsigned int* currentColor;//Color that we will be changing
			unsigned int curColor;

			//The label's own colours, as against the colour being edited. A colour
			//is a setting like any other and its name is read down a panel with the
			//rest of them, so what its label is drawn in cannot be its own affair.
			TextColor textColor, hoverColor, disabledColor;
		public:
			static Colorhook* current;//Pointer to the current colorhook

			//Two Hook Initializations; one for basic hooks, one for grouped hooks.
			Colorhook(HookVisibility visibility, unsigned int x, unsigned int y, unsigned int* color, std::string formatString, ...);
			Colorhook(HookGroup* group, unsigned int x, unsigned int y, unsigned int* color, std::string formatString, ...);

			//The open picker is held in a static, which would dangle if the hook
			//behind it were destroyed while it was still open.
			~Colorhook();

			std::string GetText() { return text; };
			void SetText(std::string newText);

			unsigned int GetColor() { return *currentColor; };
			void SetColor(unsigned int newColor);

			TextColor GetTextColor() { return textColor; };
			void SetTextColor(TextColor newColor);

			TextColor GetHoverColor() { return hoverColor; };
			void SetHoverColor(TextColor newColor);

			TextColor GetDisabledColor() { return disabledColor; };
			void SetDisabledColor(TextColor newColor);

			bool OnLeftClick(bool up, unsigned int x, unsigned int y);
			bool OnRightClick(bool up, unsigned int x, unsigned int y);
			void OnDraw();

			unsigned int GetXSize();
			unsigned int GetYSize();
	};
};