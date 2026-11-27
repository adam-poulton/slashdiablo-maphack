#include "MarkerPanel.h"
#include <algorithm>
#include <set>
#include "ForgeFile.h"
#include "../Settings/SettingsRegistry.h"
#include "../../BH.h"
#include "../../D2Ptrs.h"
#include "../../Drawing/Basic/Boxhook/Boxhook.h"
#include "../../Drawing/Basic/Texthook/Texthook.h"
#include "../../Drawing/Advanced/Colorhook/Colorhook.h"
#include "../../Drawing/Advanced/Combohook/Combohook.h"
#include "../../Drawing/Advanced/Tooltiphook/Tooltiphook.h"

using namespace Drawing;

// Above the first row and the preview, and below whatever is lowest, clear of the
// footer's rule.
#define MM_TOP				8
#define MM_BOTTOM			8
#define MM_TEXT_HEIGHT		10		// font 0

// Tall enough for the game's sprite with room above and below it, so a marker
// drawn as it stays clear of the rows on either side. The name is centred on it.
#define MM_ROW_HEIGHT		28
#define MM_ROW_TEXT_TOP		((MM_ROW_HEIGHT - MM_TEXT_HEIGHT) / 2)
static_assert(MM_ROW_HEIGHT >= MapIcon::ChestHeight + 4,
	"a row holds the game's sprite clear of its neighbours");

// The selected row's band reaches this far out past what is drawn on it, as a
// list's does, so the marker does not sit against the band's edge.
#define MM_ROW_INSET		2

// A row's marker is drawn in a column of its own, no larger than the row has
// room for, and the name starts after it. A marker that is only its line shows
// a short rule there, as the editor's icon list does for no icon.
#define MM_ROW_ICON_CENTRE	10
#define MM_ROW_ICON_SIZE	4
#define MM_ROW_LABEL_LEFT	24
#define MM_ROW_NONE_HALF	4
#define MM_ROW_NONE_COLOR	0xD0

// After the longest name, inside the band; and the most the list takes before
// it cuts the names short instead.
#define MM_LIST_PAD			8
#define MM_LIST_MAX			180

// Either side of the rule between the list and the editor, which is drawn as the
// footer's is.
#define MM_DIVIDER_GAP		8
#define MM_DIVIDER_COLOR	UI_SURFACE_RULE

// The widest the editor is laid out. Past it the preview only gets emptier.
#define MM_DETAIL_MAX		360

// A list setting under the editor, as wide as the settings window draws one.
#define MM_ATTACHED_WIDTH	120

// The tip over a name, in the colour the settings window's are drawn in.
#define MM_TIP_WIDTH		240
#define MM_TIP_COLOR		Grey

// Gold for what can be clicked, white on a band for the one in hand, grey for a
// marker that draws nothing at all.
#define MM_ROW_COLOR		Gold
#define MM_ROW_HOVER		Tan
#define MM_ROW_SELECTED		White
#define MM_ROW_OFF			Grey
#define MM_ROW_BAND			UI_SURFACE_BAND

/* MarkerRowhook
 */
MarkerRowhook::MarkerRowhook(HookGroup* group, unsigned int x, unsigned int y,
		MapIconStyle* style, std::string label, unsigned int index,
		unsigned int* selected) :
		Hook(group, x, y), style(style), label(label), index(index),
		selected(selected), xSize(0), fittedWidth(0) {
}

unsigned int MarkerRowhook::GetYSize() {
	return MM_ROW_HEIGHT;
}

unsigned int MarkerRowhook::GetNaturalXSize() {
	return MM_ROW_INSET + MM_ROW_LABEL_LEFT +
		(unsigned int)Texthook::GetTextSize(label, 0).x + MM_LIST_PAD;
}

bool MarkerRowhook::OnLeftClick(bool up, unsigned int x, unsigned int y) {
	if (!InRange(x, y))
		return false;
	if (up)
		*selected = index;
	return true;
}

void MarkerRowhook::OnDraw() {
	if (!IsActive())
		return;
	Lock();
	bool chosen = (*selected == index);
	if (chosen)
		Boxhook::Draw(GetX(), GetY(), xSize, MM_ROW_HEIGHT, MM_ROW_BAND, BTNormal);

	int iconX = (int)GetX() + MM_ROW_INSET + MM_ROW_ICON_CENTRE;
	int centreY = (int)GetY() + MM_ROW_HEIGHT / 2;
	if (!style->DrawsIcon()) {
		D2GFX_DrawLine(iconX - MM_ROW_NONE_HALF, centreY, iconX + MM_ROW_NONE_HALF,
			centreY, MM_ROW_NONE_COLOR, -1);
	} else {
		unsigned int size = (std::min)(style->size, (unsigned int)MM_ROW_ICON_SIZE);
		MapIcon::Draw(style->Icon(), size, iconX, centreY, style->Look());
	}

	unsigned int taken = MM_ROW_INSET + MM_ROW_LABEL_LEFT + MM_LIST_PAD;
	unsigned int room = xSize > taken ? xSize - taken : 0;
	if (room != fittedWidth) {
		fitted = Texthook::Fit(label, room, 0);
		fittedWidth = room;
	}

	TextColor color = MM_ROW_COLOR;
	if (chosen)
		color = MM_ROW_SELECTED;
	else if (InRange(Hook::GetMouseX(), Hook::GetMouseY()))
		color = MM_ROW_HOVER;
	else if (!style->Draws())
		color = MM_ROW_OFF;
	Texthook::Draw(GetX() + MM_ROW_INSET + MM_ROW_LABEL_LEFT,
		GetY() + MM_ROW_TEXT_TOP, None, 0, color, "%s", fitted.c_str());
	Unlock();
}

/* MarkerPanel
 */
MarkerPanel::MarkerPanel(UI* ui, std::string name, std::string category) :
		UIPanel(name, ui), category(category), selected(0), builtVersion(0), divider(NULL),
		tip(NULL), shownTip(-1), listWidth(0), labelWidth(0), detailMinWidth(0),
		minWidth(0), minHeight(0) {
	divider = new Boxhook(tab, 0, 0, 1, 0);
	divider->SetColor(MM_DIVIDER_COLOR);
	divider->SetTransparency(BTNormal);
	// Placed and switched on by UpdateTip().
	tip = new Tooltiphook(InGame, 0, 0);
	tip->SetActive(false);
}

MarkerPanel::~MarkerPanel() {
	delete tip;
}

// Only a setting that can be off says anything about whether the marker is drawn.
void MarkerPanel::FindParent(Marker& marker, const std::string& parentKey) {
	marker.parentBool = NULL;
	marker.parentToggle = NULL;
	if (parentKey.empty())
		return;
	for (const Settings::Descriptor& setting : Settings::All()) {
		if (setting.key.compare(parentKey) != 0)
			continue;
		if (setting.kind == Settings::KindToggle)
			marker.parentToggle = setting.toggleValue;
		else if (setting.kind == Settings::KindBool)
			marker.parentBool = setting.boolValue;
		marker.parentLabel = setting.label;
		return;
	}
}

void MarkerPanel::EnsureMarkers() {
	if (builtVersion == Settings::Version())
		return;
	builtVersion = Settings::Version();

	std::set<std::string> built;
	for (const Marker& marker : markers) {
		built.insert(marker.key);
		for (const Attached& attached : marker.attached)
			built.insert(attached.key);
	}

	std::vector<const Settings::Descriptor*> settings =
		Settings::InCategory(category);

	for (const Settings::Descriptor* setting : settings) {
		if (setting->kind != Settings::KindMapIcon || !setting->styleValue)
			continue;
		if (built.count(setting->key))
			continue;
		built.insert(setting->key);

		Marker marker;
		marker.key = setting->key;
		marker.label = setting->label;
		marker.style = setting->styleValue;
		marker.row = new MarkerRowhook(tab, 0, 0, setting->styleValue,
			setting->label, (unsigned int)markers.size(), &selected);
		marker.editor = new MapIconhook(tab, 0, 0, setting->styleValue,
			setting->styleOptions);
		marker.help = setting->help;
		FindParent(marker, setting->parent);
		markers.push_back(marker);
	}

	// What hangs off a marker, built once whichever marker it is on.
	for (const Settings::Descriptor* setting : settings) {
		if (setting->kind != Settings::KindEnum || !setting->intValue)
			continue;
		if (built.count(setting->key))
			continue;
		for (Marker& marker : markers) {
			if (marker.key.compare(setting->parent) != 0)
				continue;
			built.insert(setting->key);
			Attached attached;
			attached.key = setting->key;
			attached.label = new Texthook(tab, 0, 0, "%s", setting->label.c_str());
			attached.label->SetColor(MM_ROW_COLOR);
			attached.control = new Combohook(tab, 0, 0, MM_ATTACHED_WIDTH,
				setting->intValue, setting->options);
			marker.attached.push_back(attached);
			break;
		}
	}

	if (selected >= markers.size())
		selected = 0;
	Measure();
}

// Everything here measures text, so it is worked out once per set of markers
// rather than every frame.
void MarkerPanel::Measure() {
	listWidth = 0;
	labelWidth = 0;
	for (const Marker& marker : markers) {
		listWidth = (std::max)(listWidth, marker.row->GetNaturalXSize());
		labelWidth = (std::max)(labelWidth, marker.editor->GetNaturalLabelWidth());
		for (const Attached& attached : marker.attached)
			labelWidth = (std::max)(labelWidth,
				attached.label->GetXSize() + MAP_ICON_LABEL_GAP);
	}
	listWidth = (std::min)(listWidth, (unsigned int)MM_LIST_MAX);

	detailMinWidth = 0;
	for (Marker& marker : markers) {
		marker.editor->SetLabelWidth(labelWidth);
		detailMinWidth = (std::max)(detailMinWidth, marker.editor->GetMinXSize());
		for (const Attached& attached : marker.attached)
			detailMinWidth = (std::max)(detailMinWidth,
				labelWidth + attached.control->GetXSize());
	}

	unsigned int detailX = UI_CONTENT_MARGIN - MM_ROW_INSET + listWidth +
		MM_DIVIDER_GAP + 1 + MM_DIVIDER_GAP;
	minWidth = markers.empty() ? 0 : detailX + detailMinWidth + UI_CONTENT_MARGIN;

	unsigned int tallest = (unsigned int)markers.size() * MM_ROW_HEIGHT;
	for (const Marker& marker : markers)
		tallest = (std::max)(tallest, marker.editor->GetYSize() +
			(unsigned int)marker.attached.size() * MAP_ICON_ROW_HEIGHT);
	minHeight = markers.empty() ? 0 : MM_TOP + tallest + MM_BOTTOM;
}

// The rows top down, the rule beside them, and the editor of the marker in hand
// with what is attached to it under it. Every other marker's controls are put
// away, so only one marker's are ever answering the mouse.
void MarkerPanel::Layout() {
	unsigned int tabWidth = tab->GetXSize();
	unsigned int tabHeight = tab->GetYSize();

	unsigned int rowX = UI_CONTENT_MARGIN - MM_ROW_INSET;
	unsigned int dividerX = rowX + listWidth + MM_DIVIDER_GAP;
	unsigned int detailX = dividerX + 1 + MM_DIVIDER_GAP;
	unsigned int room = tabWidth > detailX + UI_CONTENT_MARGIN ?
		tabWidth - detailX - UI_CONTENT_MARGIN : 0;
	// A window held smaller than the editor by the screen it is on gives what it
	// has rather than drawing past its own edge.
	unsigned int detailWidth = room < detailMinWidth ? room :
		(std::min)(room, (std::max)((unsigned int)MM_DETAIL_MAX, detailMinWidth));

	divider->SetActive(!markers.empty());
	divider->SetBaseX(dividerX);
	divider->SetBaseY(MM_TOP);
	divider->SetYSize(tabHeight > MM_TOP + MM_BOTTOM ? tabHeight - MM_TOP - MM_BOTTOM : 0);

	for (unsigned int i = 0; i < markers.size(); i++) {
		Marker& marker = markers[i];
		marker.row->SetBaseX(rowX);
		marker.row->SetBaseY(MM_TOP + i * MM_ROW_HEIGHT);
		marker.row->SetXSize(listWidth);

		bool editing = (i == selected);
		marker.editor->SetActive(editing);
		marker.editor->SetBaseX(detailX);
		marker.editor->SetBaseY(MM_TOP);
		marker.editor->SetXSize(detailWidth);

		unsigned int y = MM_TOP + marker.editor->GetYSize();
		for (Attached& attached : marker.attached) {
			attached.label->SetActive(editing);
			attached.control->SetActive(editing);
			attached.label->SetBaseX(detailX);
			attached.label->SetBaseY(MapIconhook::RowY(y, attached.label));
			attached.control->SetBaseX(detailX + labelWidth);
			attached.control->SetBaseY(MapIconhook::RowY(y, attached.control));
			y += MAP_ICON_ROW_HEIGHT;
		}
	}
}

// Over the name the pointer is on, rebuilt only when that changes: the pointer
// rests on one row for many frames. Not while a picker or a dropdown is open,
// which the tip would be drawn over, nor through a window drawn over this one.
void MarkerPanel::UpdateTip() {
	int over = -1;
	unsigned int x = Hook::GetMouseX(), y = Hook::GetMouseY();
	if (IsActive() && !Colorhook::current && !Combohook::current &&
			!tab->IsCoveredAt(x, y)) {
		for (unsigned int i = 0; i < markers.size() && over < 0; i++) {
			if (!markers[i].help.empty() && markers[i].row->InRange(x, y))
				over = (int)i;
		}
	}
	if (over < 0) {
		HideTip();
		return;
	}

	if (over != shownTip) {
		std::vector<TooltipLine> lines;
		lines.push_back(TooltipLine(markers[over].help, MM_TIP_COLOR));
		tip->SetMaxWidth(MM_TIP_WIDTH);
		tip->SetLines(lines);
		shownTip = over;
	}
	// After SetLines(): where it fits depends on how big it turned out.
	MarkerRowhook* row = markers[over].row;
	tip->PlaceAbove(row->GetX(), row->GetY(), row->GetXSize(), row->GetYSize());
	tip->SetActive(true);
}

void MarkerPanel::HideTip() {
	tip->SetActive(false);
	shownTip = -1;
}

// The tip is drawn with the screen rather than the window, so it has to be put
// away as the window goes.
void MarkerPanel::OnClose() {
	HideTip();
}

void MarkerPanel::Select(int index) {
	if (markers.empty())
		return;
	if (index < 0)
		index = 0;
	if (index >= (int)markers.size())
		index = (int)markers.size() - 1;
	selected = (unsigned int)index;
}

// Up and down the list, as through the settings window's rows. An open colour
// picker or dropdown belongs to the marker in hand, so the keys are left alone
// while one is up rather than moving to another marker from under it.
bool MarkerPanel::OnKey(bool up, BYTE key) {
	if (markers.empty() || Colorhook::current || Combohook::current)
		return false;
	switch (key) {
		case VK_UP:
		case VK_DOWN:
		case VK_HOME:
		case VK_END:
			if (up)
				return true;
			switch (key) {
				case VK_UP:		Select((int)selected - 1); break;
				case VK_DOWN:	Select((int)selected + 1); break;
				case VK_HOME:	Select(0); break;
				case VK_END:	Select((int)markers.size() - 1); break;
			}
			return true;
	}
	return false;
}

// What stops the Forge's file being saved, which is said first, since nothing in
// any tab is kept until it is fixed; otherwise why the marker in hand is not on
// the map whatever it is set to.
std::string MarkerPanel::GetStatus() {
	const std::string& problem = BH::forge->GetProblem();
	if (!problem.empty())
		return problem;
	if (selected >= markers.size())
		return std::string();
	const Marker& marker = markers[selected];
	bool off = (marker.parentToggle && !marker.parentToggle->state) ||
		(marker.parentBool && !*marker.parentBool);
	if (!off)
		return std::string();
	return "Not drawn while " + marker.parentLabel + " is off";
}

void MarkerPanel::OnDraw() {
	EnsureMarkers();
	Layout();
	UpdateTip();
}
