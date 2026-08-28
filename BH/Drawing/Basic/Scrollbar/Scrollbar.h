#pragma once

#include <Windows.h>

namespace Drawing {
	// The scrollbar that anything scrollable draws: a shaded rail down a gutter on
	// the right with a lighter thumb whose length and position say how much of the
	// content is on screen and where in it we are.
	//
	// Here rather than inside whichever control wanted it first, so that the list
	// and the scrolling box cannot come to look or behave differently. Everything
	// is arithmetic on a track and a scroll position, so a caller that counts in
	// rows and one that counts in pixels can both use it.
	namespace Scrollbar {
		// The bar itself, and the bar plus the gap kept clear of content beside
		// it. The gutter is held clear whether or not the bar is showing, so
		// content does not shift about as it becomes scrollable.
		unsigned int Width();
		unsigned int GutterWidth();

		// How far one notch of the wheel moves the view, in rows.
		unsigned int WheelRows();

		// Proportional to how much of the content is visible, but never so short
		// that there is nothing left to take hold of.
		unsigned int ThumbHeight(unsigned int trackHeight, unsigned int visible,
			unsigned int total, unsigned int minHeight);

		unsigned int ThumbTop(unsigned int trackTop, unsigned int trackHeight,
			unsigned int thumbHeight, unsigned int scroll, unsigned int maxScroll);

		bool InBar(unsigned int x, unsigned int y, unsigned int barLeft,
			unsigned int trackTop, unsigned int trackHeight);

		// Turns a dragged thumb position back into a scroll position. Works in
		// whole pixels of travel, so the thumb sits under the cursor rather than
		// snapping, and rounds so that the halfway point of one step's worth of
		// travel is where the view actually turns over.
		unsigned int ScrollForThumbTop(int thumbTop, unsigned int travel,
			unsigned int maxScroll);

		// Lit reads as something being held rather than something the cursor
		// happens to be over.
		void Draw(unsigned int barLeft, unsigned int trackTop,
			unsigned int trackHeight, unsigned int thumbTop,
			unsigned int thumbHeight, bool lit);
	};
};
