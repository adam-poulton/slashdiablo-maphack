#pragma once

#include "../../Hook.h"

namespace Drawing {
	// The size of MapIcon::Cross that is the X monsters are marked with.
	#define MONSTER_CROSS_SIZE	4

	class Crosshook : public Hook {
		private:
			unsigned int color;//Color of the cross hook 0-255.

		public:
			//Crosshook Initaliztors, one for basic hooks and one for groups.
			Crosshook(HookVisibility visiblity, unsigned int x, unsigned int y);
			Crosshook(HookGroup* group, unsigned int x, unsigned int y);

			//Returns the color of the cross hook.
			unsigned int GetColor();

			//Sets the color of the cross hook.
			void SetColor(unsigned int newColor);

			//Filler to return nothing to base class.
			unsigned int GetXSize();
			unsigned int GetYSize();

			//Draw the text.
			void OnDraw();

			//Static Cross Draw
			static bool Draw(unsigned int x, unsigned int y, unsigned int color);
	};
};