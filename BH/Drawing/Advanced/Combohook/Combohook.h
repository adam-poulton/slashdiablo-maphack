#pragma once
#include <vector>
#include "../../Hook.h"

namespace Drawing {
	// Space between the frame and the text inside it, matching what the box has
	// always drawn. A little more above than below, since the glyphs sit on their
	// baseline and reach nearer the top of their line.
	#define COMBO_PADDING_X			5
	#define COMBO_PADDING_TOP		3
	#define COMBO_PADDING_BOTTOM	1

	// The arrow on the right, in from the frame.
	#define COMBO_ARROW_GAP			8

	class Combohook : public Hook {
		private:
			std::vector<std::string> options;
			unsigned int xSize;
			unsigned int font;
			unsigned int* currentIndex;
			bool active;

			// Opening and closing go through one place, so that the static below
			// cannot get out of step with it.
			void SetOpen(bool open);
		public:
			// The combo box whose list is open, if any, so that whoever draws last
			// can put it on top. The colour picker holds itself the same way.
			static Combohook* current;

			Combohook(HookVisibility visibility, unsigned int x, unsigned int y, unsigned int xSize, unsigned int* currentIndex, std::vector<std::string> options);
			Combohook(HookGroup* group, unsigned int x, unsigned int y, unsigned int xSize, unsigned int* currentIndex, std::vector<std::string> options);

			//The open list is held in the static above, which would dangle if the
			//box behind it were destroyed while it was still open.
			~Combohook();

			std::vector<std::string> GetOptions() { return options; };
			unsigned int NewOption(std::string opt) { Lock(); options.push_back(opt); Unlock(); return options.size() - 1; };
			//Clamped to the options there actually are. The value behind the box is
			//read from the config, so it can name an option that does not exist -
			//and the box draws the option it names, which threw rather than drew.
			//The value itself is left alone: it belongs to the module, not to us.
			unsigned int GetSelectedIndex() {
				if (!currentIndex || options.empty())
					return 0;
				return (*currentIndex < options.size()) ?
					*currentIndex : (unsigned int)options.size() - 1;
			};
			void SetSelectedIndex(unsigned int index) { if (index >= options.size()) { return; } Lock(); *currentIndex = index; Unlock(); };

			unsigned int GetFont() { return font; };
			void SetFont(unsigned int newFont) { Lock(); font = newFont; Unlock(); };

			unsigned int GetXSize() { return xSize; };
			void SetXSize(unsigned int size) { Lock(); xSize = size; Unlock(); };

			//The frame as drawn, not just the text in it. It used to report the text
			//alone, four pixels short of what it drew: everything that had to know
			//how big the box really was added those four back by hand, and anything
			//laying the box out beside something else placed it as though it were
			//the height of a bare line of text.
			unsigned int GetYSize() { unsigned int height[] = {10,11,18,24,10,13,7,13,10,12,8,8,7,12}; return height[GetFont()] + COMBO_PADDING_TOP + COMBO_PADDING_BOTTOM; };

			unsigned int GetTextInset() { return COMBO_PADDING_TOP; };

			bool OnLeftClick(bool up, unsigned int x, unsigned int y);
			void OnDraw();

			// The list an open combo box hangs below itself. Drawn separately, and
			// last, because it covers whatever is laid out under the box - which
			// the box's own OnDraw() cannot do, being drawn before its neighbours.
			void DrawOpenList();

			//The closed box, which is exactly what it draws now that it reports its
			//own height honestly.
			bool InHook(unsigned int nx, unsigned int ny) { return nx >= GetX() && ny >= GetY() && nx < GetX() + GetXSize() && ny < GetY() + GetYSize(); };

			//Where the list hangs, and how far down it reaches. One place, so the
			//drawing and the clicking cannot disagree about where an option is.
			unsigned int GetListY() { return GetY() + GetYSize(); };
			unsigned int GetOptionY(unsigned int index) { return GetListY() + (index * GetYSize()); };
	};
};
