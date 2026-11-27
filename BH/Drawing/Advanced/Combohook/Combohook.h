#pragma once
#include <vector>
#include "../../../Constants.h"
#include "../../Hook.h"
#include "../../Basic/Texthook/TextMetrics.h"

namespace Drawing {
	// Space between the frame and the text inside it, matching what the box has
	// always drawn. A little more above than below, since the glyphs sit on their
	// baseline and reach nearer the top of their line.
	#define COMBO_PADDING_X			5
	#define COMBO_PADDING_TOP		3
	#define COMBO_PADDING_BOTTOM	1

	// Where the list's scrollbar sits, in from the list's right edge.
	#define COMBO_SCROLLBAR_INSET	3

	class Combohook : public Hook {
		private:
			std::vector<std::string> options;
			unsigned int xSize;
			unsigned int font;
			unsigned int* currentIndex;
			bool active;

			// Whether the box draws the arrow that says it has a list to open.
			bool arrow;

			// How many options the open list shows at once, 0 for all of them, and
			// the first one it shows. Past that many it scrolls.
			unsigned int maxVisible;
			unsigned int scroll;

			// While the list's thumb is held: how far down the thumb it was taken.
			bool dragging;
			int dragOffset;

			void Init(unsigned int xSize, unsigned int* index,
				std::vector<std::string> opts);

			// Opening and closing go through one place, so that the static below
			// cannot get out of step with it.
			void SetOpen(bool open);

			unsigned int MaxScroll();
			void ScrollTo(int first);

			// The scrollbar down the open list's right side, where it scrolls.
			unsigned int BarLeft();
			unsigned int ThumbHeight();
			unsigned int ThumbTop();
			bool InBar(unsigned int x, unsigned int y);

			// The option under a point in the open list, or -1 for none.
			int OptionAt(unsigned int x, unsigned int y);

		protected:
			// Where an option's text is drawn from in a row that starts at x and is
			// width across, and how it is aligned against it.
			unsigned int ValueX(unsigned int x, unsigned int width);
			int ValueAlignment();

			// Draws one option, in the closed box or as a row of the open list, in
			// a row whose top left is at x and y. The text is in the given colour,
			// which says whether it is hovered or the box is switched off.
			virtual void DrawOption(unsigned int index, unsigned int x, unsigned int y,
				unsigned int width, TextColor color);

			// How tall a row of the open list is. The closed box's height, unless a
			// box draws its options taller than its text.
			virtual unsigned int GetRowHeight() { return GetYSize(); };

			// Told of an option picked from the list, once the value is set.
			virtual void OnSelected(unsigned int index) {};

		public:
			// The combo box whose list is open, if any, so that whoever draws last
			// can put it on top. The colour picker holds itself the same way.
			static Combohook* current;

			Combohook(HookVisibility visibility, unsigned int x, unsigned int y, unsigned int xSize, unsigned int* currentIndex, std::vector<std::string> options);
			Combohook(HookGroup* group, unsigned int x, unsigned int y, unsigned int xSize, unsigned int* currentIndex, std::vector<std::string> options);

			//The open list is held in the static above, which would dangle if the
			//box behind it were destroyed while it was still open.
			virtual ~Combohook();

			// Whether the box carries the arrow that says it has a list, and so
			// whether its value sits after the left inset or in the middle.
			//
			// A box whose options are one character each - a comparator, say -
			// is more arrow than value with one, and the arrow tells the player
			// nothing they would not find by clicking it. Such a box drops the
			// arrow and centres its value instead, which is also how its options
			// are then listed, so the one showing does not move as the list
			// opens over it.
			bool HasArrow() { return arrow; };
			void SetArrow(bool shown) { Lock(); arrow = shown; Unlock(); };

			// How many options the open list shows before it scrolls instead, 0
			// for every one.
			unsigned int GetMaxVisible() { return maxVisible; };
			void SetMaxVisible(unsigned int rows) { Lock(); maxVisible = rows; Unlock(); };

			// How many options the open list shows, and whether it scrolls to
			// show the rest.
			unsigned int VisibleRows();
			bool Scrolls() { return VisibleRows() < options.size(); };

			std::vector<std::string> GetOptions() { return options; };

			// Replaces what the box offers, for options that follow what is being
			// edited. The value is left where it is.
			void SetOptions(const std::vector<std::string>& newOptions) {
				Lock();
				options = newOptions;
				scroll = 0;
				Unlock();
			};
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

			//The frame as drawn, not just the text in it.
			unsigned int GetYSize() { return BaselineOffset(GetFont()) + COMBO_PADDING_TOP + COMBO_PADDING_BOTTOM; };

			unsigned int GetTextInset() { return COMBO_PADDING_TOP; };

			bool OnLeftClick(bool up, unsigned int x, unsigned int y);
			bool OnMouseWheel(int notches, unsigned int x, unsigned int y);
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
			//Below the box, or above it where the screen has no room below.
			unsigned int GetListHeight() { return VisibleRows() * GetRowHeight(); };
			unsigned int GetListY();
			unsigned int GetOptionY(unsigned int index) { return GetListY() + ((index - scroll) * GetRowHeight()); };
	};
};
