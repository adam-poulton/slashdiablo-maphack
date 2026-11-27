#include "SessionHud.h"
#include <algorithm>
#include "../../BH.h"
#include "../../Constants.h"
#include "../../D2Ptrs.h"
#include "../../Drawing/Basic/Texthook/TextMetrics.h"

using namespace Drawing;
using namespace HudLayout;

namespace {
	// The small font, and the spacing that makes one line of it as tall as a
	// collapsed window's title bar.
	const unsigned int kFont = 6;
	const int kRowGap = 4;
	const int kLabelGap = 5;
	const int kGroupGap = 14;
	const int kSectionGap = 3;
	const int kPadding = 4;

	TextColor ColorOf(Tone tone) {
		switch (tone) {
			case ToneValue:		return White;
			case ToneAhead:		return Green;
			case ToneBehind:	return Red;
			case ToneBest:		return Gold;
			default:			return Grey;
		}
	}

	// Where the HUD goes until it is moved: the top of the screen, in the middle.
	const Place kDefaultPlace = { SideMiddle, SideNear, 0, 3 };

	const char* const kSection = "SessionHud";

	// The outline of a HUD being placed, and the gap to what is written by it.
	const unsigned int kOutlineColor = 0x0D;
	const int kHintGap = 5;
	const char* const kHint = "Drag to move, wheel to change view, Escape when done";

	// The game's own panels: on the left half of the screen, and on the right.
	const DWORD kLeftPanels[] = { UI_CHARACTER, UI_QUEST, UI_NPCSHOP, UI_STASH, UI_CUBE,
		UI_TRADE, UI_WPMENU, UI_PARTY, UI_MERC };
	const DWORD kRightPanels[] = { UI_INVENTORY, UI_SKILLTREE };

	template <size_t N>
	bool AnyOpen(const DWORD (&panels)[N]) {
		for (DWORD panel : panels) {
			if (D2CLIENT_GetUIState(panel))
				return true;
		}
		return false;
	}

	// The frame behind the HUD is drawn one pixel outside the block on every
	// side, so the block is placed on a canvas that much smaller all round.
	const int kFrameEdge = 1;

	Size Canvas() {
		return { (int)Hook::GetScreenWidth(), (int)Hook::GetScreenHeight() };
	}

	Size Placeable() {
		const Size canvas = Canvas();
		return { canvas.width - 2 * kFrameEdge, canvas.height - 2 * kFrameEdge };
	}

	Metrics GameMetrics() {
		Metrics metrics;
		metrics.width = [](const std::string& text) {
			return (int)Texthook::GetTextSize(text, kFont).x;
		};
		metrics.textHeight = (int)BaselineOffset(kFont);
		metrics.rowGap = kRowGap;
		metrics.labelGap = kLabelGap;
		metrics.groupGap = kGroupGap;
		metrics.sectionGap = kSectionGap;
		metrics.padding = kPadding;
		return metrics;
	}

	std::string IniPath() {
		return BH::path + "UI.ini";
	}

	// Read as text: a distance from the middle can be negative, which the
	// integer read does not promise to give back.
	int StoredInt(const char* key, int otherwise) {
		char text[32];
		GetPrivateProfileString(kSection, key, "", text, sizeof(text), IniPath().c_str());
		return text[0] ? atoi(text) : otherwise;
	}

	Side StoredSide(const char* key, Side otherwise) {
		const int stored = StoredInt(key, otherwise);
		return stored >= SideNear && stored <= SideFar ? (Side)stored : otherwise;
	}
}

SessionHud::SessionHud() : handle(NULL), viewSteps(0), place(kDefaultPlace),
	at({ 0, 0 }), size({ 0, 0 }), placing(false), dragging(false), grabbed({ 0, 0 }) {}

void SessionHud::Load() {
	if (!handle)
		handle = new Handle(this);

	place.across = StoredSide("Across", kDefaultPlace.across);
	place.down = StoredSide("Down", kDefaultPlace.down);
	place.x = StoredInt("X", kDefaultPlace.x);
	place.y = StoredInt("Y", kDefaultPlace.y);
}

void SessionHud::Save() {
	const std::string path = IniPath();
	WritePrivateProfileString(kSection, "Across", std::to_string((int)place.across).c_str(), path.c_str());
	WritePrivateProfileString(kSection, "Down", std::to_string((int)place.down).c_str(), path.c_str());
	WritePrivateProfileString(kSection, "X", std::to_string(place.x).c_str(), path.c_str());
	WritePrivateProfileString(kSection, "Y", std::to_string(place.y).c_str(), path.c_str());
}

int SessionHud::TakeViewSteps() {
	const int steps = viewSteps;
	viewSteps = 0;
	return steps;
}

void SessionHud::SetBeingPlaced(bool place) {
	if (placing == place || !handle)
		return;
	placing = place;
	handle->SetActive(place);
	if (!place && dragging) {
		dragging = false;
		Save();
	}
}

// Keeps the corner that was taken hold of under the cursor.
void SessionHud::Follow() {
	const Size within = Placeable();
	const Point corner = {
		(std::max)(0, (std::min)(Hook::GetMouseX() - grabbed.x - kFrameEdge, within.width - size.width)),
		(std::max)(0, (std::min)(Hook::GetMouseY() - grabbed.y - kFrameEdge, within.height - size.height)),
	};
	place = PlaceOf(corner, size, within);
}

void SessionHud::Draw(const Figures& figures, HudLayout::View view, unsigned int background) {
	const Block block = ArrangeSections(Sections(view, figures), GameMetrics());

	size = block.size;
	if (dragging)
		Follow();
	at = Resolve(place, Placeable(), size);
	at.x += kFrameEdge;
	at.y += kFrameEdge;
	if (handle) {
		handle->SetBaseX(at.x);
		handle->SetBaseY(at.y);
	}

	// Shown wherever it is while it is being placed, so that it can be found.
	if (!placing && UnderPanel(at, size, Canvas(), AnyOpen(kLeftPanels),
			AnyOpen(kRightPanels)))
		return;

	if (background == BackgroundTranslucent)
		Framehook::Draw(at.x, at.y, size.width, size.height, 0, BTOneHalf);
	else if (background == BackgroundSolid)
		Framehook::Draw(at.x, at.y, size.width, size.height, 0, BTNormal);

	for (const Placed& cell : block.cells) {
		Texthook::Draw(at.x + cell.at.x, at.y + cell.at.y, None, kFont, ColorOf(cell.tone),
			"%s", cell.text.c_str());
	}
}

void SessionHud::DrawOutline() {
	const unsigned int x = at.x, y = at.y, width = size.width, height = size.height;
	Boxhook::Draw(x, y, width, 1, kOutlineColor, BTNormal);
	Boxhook::Draw(x, y + height - 1, width, 1, kOutlineColor, BTNormal);
	Boxhook::Draw(x, y, 1, height, kOutlineColor, BTNormal);
	Boxhook::Draw(x + width - 1, y, 1, height, kOutlineColor, BTNormal);

	// Under the HUD, or over it where there is no room under, and kept on the
	// canvas at either end.
	const int hintWidth = (int)Texthook::GetTextSize(kHint, kFont).x;
	const int hintHeight = (int)BaselineOffset(kFont);
	const int canvasWidth = (int)Hook::GetScreenWidth();
	const int canvasHeight = (int)Hook::GetScreenHeight();
	int hintY = at.y + size.height + kHintGap;
	if (hintY + hintHeight > canvasHeight)
		hintY = at.y - kHintGap - hintHeight;
	const int hintX = (std::max)(0, (std::min)(at.x + (size.width - hintWidth) / 2,
		canvasWidth - hintWidth));
	Texthook::Draw(hintX, hintY, None, kFont, Gold, "%s", kHint);
}

SessionHud::Handle::Handle(SessionHud* hud) : Hook(InGame, 0, 0), hud(hud) {
	SetActive(false);
}

unsigned int SessionHud::Handle::GetXSize() {
	return hud->size.width;
}

unsigned int SessionHud::Handle::GetYSize() {
	return hud->size.height;
}

void SessionHud::Handle::OnDraw() {
	if (IsActive())
		hud->DrawOutline();
}

bool SessionHud::Handle::OnLeftClick(bool up, unsigned int x, unsigned int y) {
	if (up) {
		if (!hud->dragging)
			return false;
		hud->dragging = false;
		hud->Save();
		return true;
	}
	if (!InRange(x, y))
		return false;
	hud->dragging = true;
	hud->grabbed = { (int)x - hud->at.x, (int)y - hud->at.y };
	return true;
}

bool SessionHud::Handle::OnMouseWheel(int notches, unsigned int x, unsigned int y) {
	if (!InRange(x, y))
		return false;
	// Towards the user is on to the next view, as it is down a list.
	hud->viewSteps += notches > 0 ? -1 : 1;
	return true;
}

bool SessionHud::Handle::OnKey(bool up, BYTE key, LPARAM lParam) {
	if (key != VK_ESCAPE)
		return false;
	// Ended on the release, so that the press that ends it is not also a press
	// the game opens its menu on.
	if (up)
		hud->SetBeingPlaced(false);
	return true;
}
