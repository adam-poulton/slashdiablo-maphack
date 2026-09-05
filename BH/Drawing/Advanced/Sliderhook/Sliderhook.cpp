#include "Sliderhook.h"
#include <stdio.h>
#include "../../Basic/Boxhook/Boxhook.h"
#include "../../Basic/Texthook/Texthook.h"
#include "../../../D2Ptrs.h"

using namespace Drawing;

// What the two constructors share. The range is not part of it: it has no default
// of its own, so each constructor builds it in its initialiser list.
void Sliderhook::Init(unsigned int width, unsigned int* target,
		const std::string& suffix) {
	xSize = width;
	value = target;
	unit = suffix;
	dragging = false;
	SetFont(0);
	Snap();
}

Sliderhook::Sliderhook(HookVisibility visibility, unsigned int x, unsigned int y,
		unsigned int xSize, unsigned int* value, unsigned int min,
		unsigned int max, unsigned int step, std::string unit)
	: Hook(visibility, x, y), range(min, max, step) {
		Init(xSize, value, unit);
}

Sliderhook::Sliderhook(HookGroup* group, unsigned int x, unsigned int y,
		unsigned int xSize, unsigned int* value, unsigned int min,
		unsigned int max, unsigned int step, std::string unit)
	: Hook(group, x, y), range(min, max, step) {
		Init(xSize, value, unit);
}

void Sliderhook::Snap() {
	if (!value)
		return;
	Lock();
	*value = range.Snap(*value);
	Unlock();
}

void Sliderhook::Step(int steps) {
	if (!value || steps == 0)
		return;
	Lock();
	int at = (int)range.IndexForValue(*value) + steps;
	if (at < 0)
		at = 0;
	if (at > (int)range.StepCount())
		at = (int)range.StepCount();
	*value = range.ValueForIndex((unsigned int)at);
	Unlock();
}

std::string Sliderhook::Readout(unsigned int raw) const {
	char text[32];
	sprintf_s(text, sizeof(text), "%u%s", raw, unit.c_str());
	return std::string(text);
}

unsigned int Sliderhook::ReadoutWidth() {
	return (unsigned int)Texthook::GetTextSize(Readout(range.max), GetFont()).x;
}

unsigned int Sliderhook::GetYSize() {
	unsigned int height[] = {10,11,18,24,10,13,7,13,10,12,8,8,7,12};
	unsigned int text = height[GetFont()] + SLIDER_PADDING_TOP + SLIDER_PADDING_BOTTOM;
	return (text > SLIDER_THUMB_HEIGHT) ? text : SLIDER_THUMB_HEIGHT;
}

unsigned int Sliderhook::RailWidth() {
	unsigned int taken = ReadoutWidth() + SLIDER_READOUT_GAP;
	// A hook too narrow for both keeps a rail wide enough to hold the thumb: a
	// rail shorter than its own thumb has nowhere to draw it.
	if (xSize <= taken + SLIDER_THUMB_WIDTH)
		return SLIDER_THUMB_WIDTH;
	return xSize - taken;
}

// Centred on the hook, so the rail sits on the line of text rather than above or
// below it however tall the row turned out.
unsigned int Sliderhook::RailTop() {
	return GetY() + ((GetYSize() - SLIDER_RAIL_HEIGHT) / 2);
}

unsigned int Sliderhook::Travel() {
	unsigned int rail = RailWidth();
	return (rail > SLIDER_THUMB_WIDTH) ? (rail - SLIDER_THUMB_WIDTH) : 0;
}

unsigned int Sliderhook::ThumbLeft() {
	unsigned int steps = range.StepCount();
	if (steps == 0)
		return RailLeft();
	return RailLeft() + ((Travel() * range.IndexForValue(GetValue())) / steps);
}

unsigned int Sliderhook::ThumbTop() {
	return GetY() + ((GetYSize() - SLIDER_THUMB_HEIGHT) / 2);
}

bool Sliderhook::InHook(unsigned int nx, unsigned int ny) {
	unsigned int top = ThumbTop();
	return nx >= RailLeft() && nx < RailLeft() + RailWidth() &&
		ny >= top && ny < top + SLIDER_THUMB_HEIGHT;
}

void Sliderhook::DragTo(unsigned int mouseX) {
	if (!value)
		return;
	// The cursor holds the middle of the thumb rather than wherever on it the drag
	// started: the thumb is a few pixels wide, so an offset would only ever put
	// the value one step away from the one being pointed at.
	int along = (int)mouseX - (int)RailLeft() - (int)(SLIDER_THUMB_WIDTH / 2);
	Lock();
	*value = range.ValueForIndex(range.IndexForTravel(along, Travel()));
	Unlock();
}

bool Sliderhook::OnLeftClick(bool up, unsigned int x, unsigned int y) {
	if (!IsActive())
		return false;

	// Let go wherever the button comes up, since a drag usually ends with the
	// cursor away from the rail it started on.
	if (up && dragging) {
		Lock();
		dragging = false;
		Unlock();
		return true;
	}

	if (InHook(x, y)) {
		if (!up) {
			Lock();
			dragging = true;
			Unlock();
			// Anywhere on the rail takes the thumb there, so a value a few steps
			// off can be reached with one click rather than a drag.
			DragTo(x);
		}
		return true;
	}
	return false;
}

void Sliderhook::OnDraw() {
	// A slider that has stopped being drawn cannot be being dragged either: the
	// row it was on may have been scrolled away or filtered out mid-gesture.
	if (!IsActive()) {
		dragging = false;
		return;
	}

	Lock();
	// There is no mouse move event to hang a drag off, so the thumb catches up
	// with the cursor here, once per frame.
	if (dragging)
		DragTo((*p_D2CLIENT_MouseX));
	// The value belongs to a module, which a reload or a revert can put anything
	// behind the rail through between one frame and the next.
	Snap();

	unsigned int rail = RailWidth();
	unsigned int railTop = RailTop();
	unsigned int thumbLeft = ThumbLeft();
	bool enabled = IsEnabled();
	bool lit = enabled && (dragging ||
		InHook((*p_D2CLIENT_MouseX), (*p_D2CLIENT_MouseY)));

	Boxhook::Draw(RailLeft(), railTop, rail, SLIDER_RAIL_HEIGHT,
		SLIDER_RAIL_COLOR, BTOneHalf);
	// Filled up to the far edge of the thumb, so how far along the rail the value
	// sits reads without having to read the number after it - and so the rail is
	// exactly full at the maximum and exactly a thumb's worth at the minimum.
	if (enabled) {
		unsigned int filled = (thumbLeft - RailLeft()) + SLIDER_THUMB_WIDTH;
		Boxhook::Draw(RailLeft(), railTop, filled, SLIDER_RAIL_HEIGHT,
			SLIDER_FILL_COLOR, BTNormal);
	}
	Boxhook::Draw(thumbLeft, ThumbTop(), SLIDER_THUMB_WIDTH, SLIDER_THUMB_HEIGHT,
		lit ? SLIDER_THUMB_LIT : SLIDER_THUMB_COLOR, BTNormal);

	// Against the right edge of the hook, so the number stays in its column
	// whatever it says and however wide the row is.
	Texthook::Draw(GetX() + xSize, GetY() + SLIDER_PADDING_TOP, Right, GetFont(),
		enabled ? Gold : DISABLED_TEXT_COLOR, Readout(GetValue()));
	Unlock();
}
