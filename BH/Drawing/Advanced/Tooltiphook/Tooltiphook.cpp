#include "Tooltiphook.h"
#include "../../Basic/Framehook/Framehook.h"

using namespace std;
using namespace Drawing;

#define TIP_PADDING		7
#define TIP_LINE_HEIGHT	12
#define TIP_GAP			6	// between the panel and whatever it describes
#define TIP_SCREEN_EDGE	4	// how close to the screen edge it may sit

// Wide enough for a full stat line without running the panel across the screen.
#define TIP_DEFAULT_MAX_WIDTH	360

Tooltiphook::Tooltiphook(HookVisibility visibility, unsigned int x, unsigned int y) :
Hook(visibility, x, y), xSize(0), ySize(0), font(0), maxWidth(TIP_DEFAULT_MAX_WIDTH) {
}

void Tooltiphook::SetFont(unsigned int newFont) {
	if (newFont < 14) {
		Lock();
		font = newFont;
		Rebuild();
		Unlock();
	}
}

void Tooltiphook::SetMaxWidth(unsigned int newMaxWidth) {
	Lock();
	maxWidth = newMaxWidth;
	Rebuild();
	Unlock();
}

void Tooltiphook::SetLines(const std::vector<TooltipLine>& newLines) {
	Lock();
	source = newLines;
	Rebuild();
	Unlock();
}

void Tooltiphook::Clear() {
	Lock();
	source.clear();
	wrapped.clear();
	xSize = 0;
	ySize = 0;
	Unlock();
}

// Splits one line over as many as it takes to fit the maximum width, keeping its
// colour on every piece so a wrapped stat still reads as one thing.
void Tooltiphook::WrapLine(const TooltipLine& line) {
	if (line.text.length() == 0 ||
		(unsigned int)Texthook::GetTextSize(line.text, font).x <= maxWidth) {
		wrapped.push_back(line);
		return;
	}

	std::string current;
	size_t pos = 0;
	while (pos < line.text.length()) {
		size_t space = line.text.find(' ', pos);
		std::string word = (space == std::string::npos) ?
			line.text.substr(pos) : line.text.substr(pos, space - pos);
		pos = (space == std::string::npos) ? line.text.length() : space + 1;
		if (word.length() == 0)
			continue;	// runs of spaces

		std::string candidate = current.length() ? (current + " " + word) : word;
		if (current.length() > 0 &&
			(unsigned int)Texthook::GetTextSize(candidate, font).x > maxWidth) {
			wrapped.push_back(TooltipLine(current, line.color));
			current = word;
		} else {
			current = candidate;
		}
	}
	if (current.length() > 0)
		wrapped.push_back(TooltipLine(current, line.color));
}

// The panel is only ever as big as what it holds, so a short description gets a
// small border rather than a mostly empty one.
void Tooltiphook::Rebuild() {
	wrapped.clear();
	for (unsigned int i = 0; i < source.size(); i++)
		WrapLine(source[i]);

	unsigned int widest = 0;
	for (unsigned int i = 0; i < wrapped.size(); i++) {
		unsigned int width = (unsigned int)Texthook::GetTextSize(wrapped[i].text, font).x;
		if (width > widest)
			widest = width;
	}

	xSize = wrapped.empty() ? 0 : widest + (2 * TIP_PADDING);
	ySize = wrapped.empty() ? 0 :
		(unsigned int)(wrapped.size() * TIP_LINE_HEIGHT) + (2 * TIP_PADDING);
}

void Tooltiphook::PlaceBeside(unsigned int x, unsigned int y,
		unsigned int width, unsigned int height) {
	unsigned int screenWidth = Hook::GetScreenWidth();
	unsigned int screenHeight = Hook::GetScreenHeight();

	Lock();
	// Beside it on the right by preference, on the left when there is no room
	// there, and pinned to the edge when there is room on neither side.
	unsigned int left = x + width + TIP_GAP;
	if (left + xSize + TIP_SCREEN_EDGE > screenWidth) {
		left = (x > xSize + TIP_GAP) ? (x - xSize - TIP_GAP) : TIP_SCREEN_EDGE;
	}

	// Lined up with the top of what it describes rather than with the row that
	// raised it, so it stays put while the mouse runs down a list instead of
	// following it about.
	unsigned int top = y;
	if (top + ySize + TIP_SCREEN_EDGE > screenHeight) {
		top = (screenHeight > ySize + TIP_SCREEN_EDGE) ?
			(screenHeight - ySize - TIP_SCREEN_EDGE) : TIP_SCREEN_EDGE;
	}

	SetBaseX(left);
	SetBaseY(top);
	Unlock();
}

void Tooltiphook::PlaceAbove(unsigned int x, unsigned int y,
		unsigned int width, unsigned int height) {
	unsigned int screenWidth = Hook::GetScreenWidth();
	unsigned int screenHeight = Hook::GetScreenHeight();

	Lock();
	// Over it by preference, under it when there is no room over, and against the
	// top edge when there is room for neither.
	unsigned int top;
	if (y > ySize + TIP_GAP + TIP_SCREEN_EDGE) {
		top = y - ySize - TIP_GAP;
	} else {
		top = y + height + TIP_GAP;
		if (top + ySize + TIP_SCREEN_EDGE > screenHeight)
			top = TIP_SCREEN_EDGE;
	}

	// Lined up with the left edge of what it describes, so it reads as belonging
	// to it, and pulled back on screen where it would hang off the right.
	unsigned int left = x;
	if (left + xSize + TIP_SCREEN_EDGE > screenWidth) {
		left = (screenWidth > xSize + TIP_SCREEN_EDGE) ?
			(screenWidth - xSize - TIP_SCREEN_EDGE) : TIP_SCREEN_EDGE;
	}

	SetBaseX(left);
	SetBaseY(top);
	Unlock();
}

void Tooltiphook::OnDraw() {
	if (!IsActive() || wrapped.empty())
		return;

	Lock();
	Framehook::Draw(GetX(), GetY(), xSize, ySize, 0, BTFull);

	unsigned int y = GetY() + TIP_PADDING;
	for (unsigned int i = 0; i < wrapped.size(); i++, y += TIP_LINE_HEIGHT) {
		if (wrapped[i].text.length() == 0)
			continue;	// a blank line is just a gap
		unsigned int width = (unsigned int)Texthook::GetTextSize(wrapped[i].text, font).x;
		Texthook::Draw(GetX() + ((xSize - width) / 2), y, None, font,
			wrapped[i].color, "%s", wrapped[i].text.c_str());
	}
	Unlock();
}
