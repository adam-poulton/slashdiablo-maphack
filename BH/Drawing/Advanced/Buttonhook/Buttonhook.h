#pragma once

#include "../../Hook.h"

namespace Drawing {
	// The mark a square button carries. Drawn from boxes and lines rather than
	// from a font: the game's fonts have no funnel in them, and a letter x
	// standing in for a close mark reads as a typo beside one that is drawn.
	enum ButtonIcon {
		IconFilter,
		IconClose
	};

	// Space kept between the frame and the mark inside it, as a share of the
	// button. A mark inset by a fixed number of pixels is most of a large button
	// and almost nothing of a small one.
	#define BUTTON_ICON_DIVISOR		4
	#define BUTTON_ICON_MIN_INSET	2

	// A square button with a mark in the middle and no label.
	//
	// It says what it is by its fill and its colour rather than by its text,
	// which is what lets it sit on a row beside a search box without taking
	// width from it. The fill follows the pressed state, the way an input box's
	// follows its caret, so a button that opens something looks open while that
	// something is open.
	//
	// What the colours mean is the owner's business: a button is told what to
	// draw itself in, and not what it is for.
	class Buttonhook : public Hook {
		private:
			ButtonIcon icon;
			unsigned int size;
			bool pressed;

			// Palette indices rather than TextColors, since the mark is drawn
			// from boxes and lines and those take the palette directly.
			unsigned int color, hoverColor;

			void DrawFilter(unsigned int x, unsigned int y, unsigned int side,
				unsigned int shade);
			void DrawClose(unsigned int x, unsigned int y, unsigned int side,
				unsigned int shade);
		public:
			Buttonhook(HookVisibility visibility, unsigned int x, unsigned int y,
				unsigned int size, ButtonIcon icon);
			Buttonhook(HookGroup* group, unsigned int x, unsigned int y,
				unsigned int size, ButtonIcon icon);

			ButtonIcon GetIcon() { return icon; };
			void SetIcon(ButtonIcon newIcon) { Lock(); icon = newIcon; Unlock(); };

			// Square, so one number sizes it. Matching the height of whatever it
			// sits beside is what makes it look like part of that row.
			unsigned int GetXSize() { return size; };
			unsigned int GetYSize() { return size; };
			void SetSize(unsigned int newSize) { Lock(); size = newSize; Unlock(); };

			// Whether the button draws as held. Set by the owner rather than
			// toggled here: what a press means is the owner's to decide, and a
			// button that latched itself would be wrong for one that does not.
			bool IsPressed() { return pressed; };
			void SetPressed(bool state) { Lock(); pressed = state; Unlock(); };

			unsigned int GetColor() { return color; };
			void SetColor(unsigned int newColor) { Lock(); color = newColor; Unlock(); };

			unsigned int GetHoverColor() { return hoverColor; };
			void SetHoverColor(unsigned int newColor) { Lock(); hoverColor = newColor; Unlock(); };

			bool IsHovered();

			bool OnLeftClick(bool up, unsigned int x, unsigned int y);
			void OnDraw();
	};
};
