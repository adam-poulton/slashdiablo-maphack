#include "Scrollbar.h"
#include "../Boxhook/Boxhook.h"

using namespace Drawing;

// The rail is a shade of the panel behind it, so it reads as a groove rather than
// as another object; the thumb is a solid fill on top of it, inset by a pixel so
// a sliver of the rail shows either side of it. Palette indices are from the same
// set the automap markers are drawn with.
#define SCROLLBAR_WIDTH			5
#define SCROLLBAR_GAP			3
#define SCROLLBAR_WHEEL_ROWS	3
#define SCROLLBAR_THUMB			0xD0	// grey
#define SCROLLBAR_THUMB_LIT		0x20	// white
#define SCROLLBAR_THUMB_INSET	1

unsigned int Scrollbar::Width() {
	return SCROLLBAR_WIDTH;
}

unsigned int Scrollbar::GutterWidth() {
	return SCROLLBAR_WIDTH + SCROLLBAR_GAP;
}

unsigned int Scrollbar::WheelRows() {
	return SCROLLBAR_WHEEL_ROWS;
}

unsigned int Scrollbar::ThumbHeight(unsigned int trackHeight, unsigned int visible,
		unsigned int total, unsigned int minHeight) {
	if (total == 0 || visible == 0)
		return trackHeight;
	unsigned int height = (trackHeight * visible) / total;
	return (height < minHeight) ? minHeight : height;
}

unsigned int Scrollbar::ThumbTop(unsigned int trackTop, unsigned int trackHeight,
		unsigned int thumbHeight, unsigned int scroll, unsigned int maxScroll) {
	if (maxScroll == 0 || trackHeight <= thumbHeight)
		return trackTop;
	unsigned int travel = trackHeight - thumbHeight;
	return trackTop + ((travel * scroll) / maxScroll);
}

bool Scrollbar::InBar(unsigned int x, unsigned int y, unsigned int barLeft,
		unsigned int trackTop, unsigned int trackHeight) {
	return x >= barLeft && x <= barLeft + SCROLLBAR_WIDTH &&
		y >= trackTop && y <= trackTop + trackHeight;
}

unsigned int Scrollbar::ScrollForThumbTop(int thumbTop, unsigned int travel,
		unsigned int maxScroll) {
	if (maxScroll == 0 || travel == 0)
		return 0;
	if (thumbTop <= 0)
		return 0;
	if ((unsigned int)thumbTop >= travel)
		return maxScroll;
	return (((unsigned int)thumbTop * maxScroll) + (travel / 2)) / travel;
}

void Scrollbar::Draw(unsigned int barLeft, unsigned int trackTop,
		unsigned int trackHeight, unsigned int thumbTop, unsigned int thumbHeight,
		bool lit) {
	Boxhook::Draw(barLeft, trackTop, SCROLLBAR_WIDTH, trackHeight, 0, BTOneHalf);
	Boxhook::Draw(barLeft + SCROLLBAR_THUMB_INSET, thumbTop,
		SCROLLBAR_WIDTH - (2 * SCROLLBAR_THUMB_INSET), thumbHeight,
		lit ? SCROLLBAR_THUMB_LIT : SCROLLBAR_THUMB, BTNormal);
}
