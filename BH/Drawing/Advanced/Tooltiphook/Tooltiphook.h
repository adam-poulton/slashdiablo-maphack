#pragma once

#include <string>
#include <vector>
#include "../../Hook.h"
#include "../../Basic/Texthook/Texthook.h"

namespace Drawing {
	// One line of a tooltip, with the colour it is drawn in.
	struct TooltipLine {
		std::string text;
		TextColor color;

		TooltipLine() : color(White) {};
		TooltipLine(std::string text, TextColor color = White) :
			text(text), color(color) {};
	};

	// A bordered panel that describes something the way the game describes an
	// item: lines of text centred inside a border sized to hold them.
	//
	// It knows nothing about what it is describing. Callers hand it lines and a
	// rectangle to sit beside; it wraps those lines to its maximum width, sizes
	// itself to whatever it ends up holding, and picks a side that keeps it on
	// screen. So anything that can be reduced to lines of text can be shown with
	// it, and how a thing is described stays separate from how it is drawn.
	//
	// Give it a visibility rather than a group: a tooltip needs to sit wherever
	// there is room for it, including outside the window that raised it, which a
	// group's coordinate space cannot express.
	//
	// SetLines(), SetMaxWidth() and SetFont() measure text with the game's font
	// routines, so call them from the draw thread (a module's OnDraw).
	class Tooltiphook : public Hook {
		private:
			std::vector<TooltipLine> source;		// as supplied
			std::vector<TooltipLine> wrapped;		// broken to fit maxWidth
			unsigned int xSize, ySize;				// sized to the wrapped lines
			unsigned int font;
			unsigned int maxWidth;

			// Rewraps the source lines and resizes to fit them. Assumes the lock
			// is already held.
			void Rebuild();
			void WrapLine(const TooltipLine& line);

		public:
			Tooltiphook(HookVisibility visibility, unsigned int x, unsigned int y);

			unsigned int GetXSize() { return xSize; };
			unsigned int GetYSize() { return ySize; };

			unsigned int GetFont() { return font; };
			void SetFont(unsigned int newFont);

			// Longest line the panel will draw before wrapping. The panel itself
			// comes out wider than this by its padding.
			unsigned int GetMaxWidth() { return maxWidth; };
			void SetMaxWidth(unsigned int newMaxWidth);

			bool IsEmpty() { return wrapped.empty(); };
			void SetLines(const std::vector<TooltipLine>& newLines);
			void Clear();

			// Puts the panel alongside the given rectangle, on whichever side
			// there is room for it, and pulls it back on screen if it would hang
			// off an edge. Call it after SetLines(), since where the panel fits
			// depends on how big it turned out.
			void PlaceBeside(unsigned int x, unsigned int y,
				unsigned int width, unsigned int height);

			// Puts the panel over the given rectangle, under it when there is no
			// room over, and lines it up with its left edge. For describing one
			// line of a list rather than the list itself: beside would put the
			// panel a long way from the line that raised it. Call it after
			// SetLines(), for the same reason as PlaceBeside().
			void PlaceAbove(unsigned int x, unsigned int y,
				unsigned int width, unsigned int height);

			void OnDraw();
	};
};
