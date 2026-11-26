#pragma once

namespace Drawing {

// A RECT as D2CLIENT_DrawRectFrame reads it: right and bottom are its last
// column and row.
struct FrameRect {
	int left;
	int top;
	int right;
	int bottom;
};

struct FrameBox {
	int x;
	int y;
	int width;
	int height;
};

// The frame's art is a ring two pixels wide with a black line one pixel inside
// it, and it does not land on its RECT: it covers from one column right of left
// to one right of right, and from one row above top down to bottom. Measured at
// 1.13c on frames 12, 15 and 400 pixels tall; D2Client's own panels are drawn
// with the same offset.

// How far the art reaches past the box it frames, on every side.
const int kFrameOutset = 1;

// The RECT whose ring straddles the box's edge on every side: one pixel of it
// outside the box and one on its outermost pixels, with the black line inside.
// That is where the art always sat above and below a box, which every control's
// text is placed against.
inline FrameRect FrameRectFor(const FrameBox& box) {
	return {box.x - 2, box.y, box.x + box.width - 1, box.y + box.height};
}

// The box the art drawn from a RECT covers.
inline FrameBox FrameBoxOf(const FrameRect& rect) {
	return {rect.left + 1, rect.top - 1, rect.right - rect.left + 1, rect.bottom - rect.top + 2};
}

}
