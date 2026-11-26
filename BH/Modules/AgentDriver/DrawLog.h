#pragma once
#include <string>
#include <vector>

#include "nlohmann/json.hpp"

namespace Agent {

// One thing BH drew, in window pixels: text, boxes and frames as a rectangle
// (top-left corner and size), lines as their two ends.
struct DrawRecord {
	std::string kind;	// "text", "box", "frame" or "line"
	int x = 0;
	int y = 0;
	int width = 0;
	int height = 0;
	int x2 = 0;
	int y2 = 0;
	unsigned int color = 0;
	int font = -1;		// text only
	int trans = -1;		// boxes only: the BoxTrans they were drawn with
	std::wstring text;
};

nlohmann::json ToJson(const DrawRecord& record);

// What BH draws in one frame, recorded only while asked for, from the four
// game calls everything BH draws goes through (see D2Ptrs.h). Everything is
// drawn on the game's thread, so the log is too, and the rest of the time it
// costs a test of one flag.
namespace DrawLog {
	bool Recording();
	void Begin();
	std::vector<DrawRecord> End();

	// As D2WIN_DrawText is called: from the baseline.
	void Text(int x, int baseline, int width, int font, unsigned int color, const std::wstring& text);
	// As D2GFX_DrawRectangle and D2GFX_DrawLine are called: by two corners.
	void Box(int x1, int y1, int x2, int y2, unsigned int color, int trans);
	void Line(int x1, int y1, int x2, int y2, unsigned int color);
	// As D2CLIENT_DrawRectFrame is called, recorded as the box its art covers
	// (see FrameArt.h). A frame has no colour of its own.
	void Frame(int left, int top, int right, int bottom);
}

}
