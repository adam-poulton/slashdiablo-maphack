#include "Keyhook.h"
#include "../../Basic/Framehook/Framehook.h"
#include "../../Basic/Texthook/Texthook.h"
#include "../../../D2Ptrs.h"
#include "../../../Common.h"
#include "../../../Constants.h"

using namespace std;
using namespace Drawing;

// The chip draws in the default font throughout: it sits on a row beside labels
// and boxes that all draw in that font, and text on a row that did not match them
// would sit off the line they share.
#define KEY_FONT		0

/* Basic Hook Initializer
 *		Used for just drawing basics.
 */
Keyhook::Keyhook(HookVisibility visibility, unsigned int x, unsigned int y, unsigned int* key, std::string hotkeyName) :
Hook(visibility, x, y) {
	//Correctly format the string from the given arguments.
	timeout = 0;
	xSize = 0;
	SetKey(key);
	SetName(hotkeyName);
}

/* Group Hook Initializer
 *		Used in conjuction with other basic hooks to create an advanced hook.
 */
Keyhook::Keyhook(HookGroup *group, unsigned int x, unsigned int y, unsigned int* key, std::string hotkeyName) :
Hook(group, x, y) {
	//Correctly format the string from the given arguments.
	timeout = 0;
	xSize = 0;
	SetKey(key);
	SetName(hotkeyName);
}

// As much of the text as the chip has room for. The column can be capped
// narrower than a binding reads, and text drawn wider than the border around it
// would be drawn over whatever the chip is sitting next to.
static std::string FitToWidth(const std::string& text, unsigned int width) {
	std::string fitted = text;
	while (!fitted.empty() &&
			(unsigned int)Texthook::GetTextSize(fitted, KEY_FONT).x > width)
		fitted.erase(fitted.length() - 1);
	return fitted;
}

std::string Keyhook::CountdownText(unsigned int seconds) {
	char num[16];
	_itoa_s((int)seconds, num, sizeof(num), 10);
	return string(num) + " secs";
}

std::string Keyhook::Caption() {
	if (timeout) {
		unsigned int elapsed = (unsigned int)
			((GetTickCount() - timeout) / 1000);
		return CountdownText((elapsed < KEY_REBIND_SECONDS) ?
			(KEY_REBIND_SECONDS - elapsed) : 0);
	}
	string prefix = (name.length() > 0) ? (name + " ") : "";
	return prefix + GetKeyCode(GetKey()).literalName;
}

bool Keyhook::OnLeftClick(bool up, unsigned int x, unsigned int y) {
	if (InRange(x,y)) {
		if (up) {
			if (!timeout)
				timeout = GetTickCount();
			else
				timeout = 0;
		}
		return true;
	}
	return false;
}

void Keyhook::OnDraw() {
	if (!IsActive())
		return;

	Lock();
	// The wait is given up here rather than on a timer: this is the only thing
	// that runs while the chip is listening, so a chip that was never drawn again
	// would otherwise wait for a key for ever.
	if (timeout && (GetTickCount() - timeout) >= (KEY_REBIND_SECONDS * 1000))
		timeout = 0;

	bool enabled = IsEnabled();
	bool listening = (timeout != 0);
	bool hovered = enabled && InRange(Hook::GetMouseX(), Hook::GetMouseY());

	// A chip waiting for a key is filled and outlined brightly, as a box being
	// typed into is: both are saying that the next thing pressed goes here.
	BoxTrans trans = listening ? BTFull : BTOneHalf;
	unsigned int width = GetXSize();
	unsigned int height = GetYSize();
	D2GFX_DrawRectangle(GetX(), GetY(), GetX() + width, GetY() + height, 0, trans);
	Framehook::DrawBorder(GetX(), GetY(), width, height, trans);

	// A binding nobody has set is dimmed rather than drawn as a value, so an empty
	// chip reads as one waiting to be bound rather than as one bound to a key
	// called "Not Set".
	TextColor color;
	if (!enabled)
		color = DISABLED_TEXT_COLOR;
	else if (listening)
		color = White;
	else if (hovered)
		color = Tan;
	else if (GetKey() == 0)
		color = Grey;
	else
		color = Gold;

	// Centred in the column rather than started at its left edge: the column is as
	// wide as the longest binding in the panel, and every shorter one left against
	// one side of it would read as a ragged edge down the tab.
	unsigned int room = (width > 2 * KEY_PADDING_X) ?
		(width - (2 * KEY_PADDING_X)) : 0;
	Texthook::Draw(GetX() + (width / 2), GetY() + KEY_PADDING_TOP, Center,
		KEY_FONT, color, "%s", FitToWidth(Caption(), room).c_str());
	Unlock();
}

bool Keyhook::OnKey(bool up, BYTE kkey, LPARAM lParam) {
	if (timeout) {
		Lock();
		if (up) {
			if (kkey == VK_ESCAPE)
				kkey = 0;
			*key = (unsigned int)kkey;
			timeout = 0;
		}
		Unlock();
		return true;
	}
	return false;
}

unsigned int Keyhook::GetContentWidth() {
	unsigned int widest = (unsigned int)
		Texthook::GetTextSize(Caption(), KEY_FONT).x;
	// Every countdown the chip can show, not just the one it starts at: the
	// digits are not all the same width, so a column measured off "3 secs" alone
	// could still have to grow as the count came down.
	for (unsigned int left = 0; left <= KEY_REBIND_SECONDS; left++) {
		unsigned int width = (unsigned int)
			Texthook::GetTextSize(CountdownText(left), KEY_FONT).x;
		if (width > widest)
			widest = width;
	}
	return widest + (2 * KEY_PADDING_X);
}

unsigned int Keyhook::GetXSize() {
	return xSize ? xSize : GetContentWidth();
}

unsigned int Keyhook::GetYSize() {
	unsigned int height[] = {10,11,18,24,10,13,7,13,10,12,8,8,7,12};
	return height[KEY_FONT] + KEY_PADDING_TOP + KEY_PADDING_BOTTOM;
}
