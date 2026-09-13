#pragma once
#include "../../../Constants.h"
#include "../../Hook.h"
#include "../../Basic/Texthook/Texthook.h"

namespace Drawing {
	// The box, and the clear space between it and the label. The hook's width is
	// both plus the label, so that what it reports is what it draws and a click
	// anywhere along it lands.
	#define CHECK_BOX_SIZE		12
	#define CHECK_LABEL_GAP		6

	// The label sits a little below the top of the box, so the two read as one line
	// rather than the text sitting on the box's rim.
	#define CHECK_LABEL_TOP		2

	class Checkhook : public Hook {
		private:
			bool* state;//Holds if the checkbox is checked.
			TextColor color, hoverColor, disabledColor;//Holds text color/hover color.
			std::string text;//The text beside the checkhook.

			//The box and, when checked, the mark in it. Apart from OnDraw so that
			//what the box looks like sits in one place rather than mixed in with
			//placing the label beside it.
			static void DrawBox(unsigned int x, unsigned int y, bool checked,
				TextColor color);
		public:
			Checkhook(HookVisibility visibility, unsigned int x, unsigned int y, bool* checked, std::string formatString, ...);
			Checkhook(HookGroup* group, unsigned int x, unsigned int y, bool* checked, std::string formatString, ...);

			//Returns if the check is checked.
			bool IsChecked();

			//Sets if it is checked or not.
			void SetState(bool checked);

			//Returns the text color.
			TextColor GetTextColor();

			//Returns the hover color
			TextColor GetHoverColor();

			//Sets the text color
			void SetTextColor(TextColor newColor);

			//Sets the hover color
			void SetHoverColor(TextColor newColor);

			//Returns the color the box and its label are drawn in while switched off.
			TextColor GetDisabledColor();

			//Sets the color to draw while switched off. Settable because one colour
			//for every disabled hook leaves a disabled hook unable to say anything
			//else about itself.
			void SetDisabledColor(TextColor newColor);

			//Gets the text
			std::string GetText();

			//Sets the text
			void SetText(std::string formatString, ...);

			//Returns the total width of the check hook
			unsigned int GetXSize();

			//Returns the total hright of the check hook
			unsigned int GetYSize();
			unsigned int GetTextInset();

			//Draw the text.
			void OnDraw();

			//Checks if we've been clicked on and calls the handler if so.
			bool OnLeftClick(bool up, unsigned int x, unsigned int y);

			//Checks if we've been clicked on and calls the handler if so.
			bool OnRightClick(bool up, unsigned int x, unsigned int y);
	};
};