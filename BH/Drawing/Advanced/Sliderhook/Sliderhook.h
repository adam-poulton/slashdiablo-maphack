#pragma once
#include <string>
#include "SliderRange.h"
#include "../../Hook.h"

namespace Drawing {
	// A number chosen by dragging a thumb along a rail, with the value written
	// beside it. Every position on the rail is a value the setting will take, so
	// unlike a box there is nothing to type that the setting cannot use and no
	// value it can be left holding that it cannot recover from.
	//
	// That is the reason to have it rather than the look of it: a number box takes
	// whatever is typed and only finds out afterwards whether it was usable, while
	// a slider is bounded by construction.

	// The rail, and the thumb that runs along it. The thumb stands taller than the
	// rail so it reads as something sitting on top of it rather than as a lit
	// section of it. Palette indices are from the same set the scrollbar uses.
	#define SLIDER_RAIL_HEIGHT		5
	#define SLIDER_THUMB_WIDTH		7
	#define SLIDER_THUMB_HEIGHT		11
	#define SLIDER_RAIL_COLOR		0x00	// shaded rather than filled
	#define SLIDER_FILL_COLOR		0xD0	// grey, as the thumb
	#define SLIDER_THUMB_COLOR		0xD0	// grey
	#define SLIDER_THUMB_LIT		0x20	// white

	// Room above and below the line of text, as the other boxed controls keep it.
	#define SLIDER_PADDING_TOP		3
	#define SLIDER_PADDING_BOTTOM	1

	// Between the end of the rail and the value written after it.
	#define SLIDER_READOUT_GAP		6

	class Sliderhook : public Hook {
		private:
			unsigned int* value;
			SliderRange range;
			std::string unit;		// written after the number, "" for none
			unsigned int xSize;
			unsigned int font;
			bool dragging;

			// Where the number is written, and therefore how much of the hook is
			// left for the rail. Measured for the largest value the slider will
			// take rather than the one it is showing, so the rail does not breathe
			// as the thumb is dragged along it.
			unsigned int ReadoutWidth();

			// Follows the cursor, in whole steps. Rounds at the halfway point of a
			// step's worth of travel, so the thumb turns over where it looks like
			// it should.
			void DragTo(unsigned int mouseX);

			void Init(unsigned int width, unsigned int* target,
				const std::string& suffix);

		public:
			Sliderhook(HookVisibility visibility, unsigned int x, unsigned int y,
				unsigned int xSize, unsigned int* value, unsigned int min,
				unsigned int max, unsigned int step, std::string unit = "");
			Sliderhook(HookGroup* group, unsigned int x, unsigned int y,
				unsigned int xSize, unsigned int* value, unsigned int min,
				unsigned int max, unsigned int step, std::string unit = "");

			// Puts the value on a step and inside the range. The value belongs to a
			// module, which reads it from the config and can be handed anything at
			// all, so the slider does not assume it was left one it can draw.
			void Snap();

			// Moves by whole steps, for the arrow keys. Signed: negative is towards
			// the minimum.
			//
			// The wheel is deliberately not one of these. A slider is laid out in
			// a scrolling list, and a hook that took the wheel would adjust the
			// value of whichever one the cursor happened to be resting on instead
			// of scrolling the list past it.
			void Step(int steps);

			unsigned int GetValue() const { return value ? *value : range.min; };
			const SliderRange& GetRange() const { return range; };

			// What the value reads as, which is the number and its unit. Public so
			// that whoever lays the slider out can size a column to it.
			std::string Readout(unsigned int raw) const;

			unsigned int GetFont() { return font; };
			void SetFont(unsigned int newFont) { Lock(); font = newFont; Unlock(); };

			unsigned int GetXSize() { return xSize; };
			void SetXSize(unsigned int size) { Lock(); xSize = size; Unlock(); };

			unsigned int GetYSize();
			unsigned int GetTextInset() { return SLIDER_PADDING_TOP; };

			// The rail and the thumb on it. Shared by drawing, hit testing and
			// dragging, so none of them can disagree about where a value sits.
			unsigned int RailLeft() { return GetX(); };
			unsigned int RailWidth();
			unsigned int RailTop();
			unsigned int Travel();
			unsigned int ThumbLeft();
			unsigned int ThumbTop();

			// True while the thumb is held, which is what tells the panel around it
			// that the gesture is the slider's rather than a click on the row.
			bool IsDragging() { return dragging; };

			bool OnLeftClick(bool up, unsigned int x, unsigned int y);
			void OnDraw();

			// The rail's full height plus the thumb standing over it, so the whole
			// of what is drawn takes the click rather than the rail alone.
			bool InHook(unsigned int nx, unsigned int ny);
	};
};
