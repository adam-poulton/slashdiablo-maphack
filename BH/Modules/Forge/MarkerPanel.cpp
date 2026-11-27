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
#include "../../Drawing/Advanced/Inputhook/Inputhook.h"
#include "../../Drawing/Advanced/Tooltiphook/Tooltiphook.h"

using namespace Drawing;

// The list's rows reach this far out past the tab's margin, so the band behind
// the one in hand does not sit against what is drawn on it.
#define MM_ROW_INSET		2

// The most the list takes before it cuts the names short instead.
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

#define MM_LABEL_COLOR		Gold

MarkerPanel::MarkerPanel(UI* ui, std::string name, std::string category) :
		UIPanel(name, ui), category(category), builtVersion(0), divider(NULL),
		tip(NULL), shownTip(-1), listWidth(0), labelWidth(0), detailMinWidth(0),
		minWidth(0), minHeight(0), list(NULL) {
	list = new MarkerListhook(tab, 0, 0);
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
		marker.options = setting->styleOptions;
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
			attached.label->SetColor(MM_LABEL_COLOR);
			attached.control = new Combohook(tab, 0, 0, MM_ATTACHED_WIDTH,
				setting->intValue, setting->options);
			marker.attached.push_back(attached);
			break;
		}
	}

	PushRows();
	Measure();
}

// The markers as the list shows them, under their caption where the tab has one,
// and whatever a derived panel lists after them. Given every frame: a row shows
// its marker as it is styled now.
void MarkerPanel::PushRows() {
	std::vector<MarkerListRow> rows;
	std::string caption = MarkersCaption();
	if (!caption.empty() && !markers.empty())
		rows.push_back(MarkerListRow::Caption(caption));
	for (unsigned int i = 0; i < markers.size(); i++)
		rows.push_back(MarkerListRow::Marker((int)i, markers[i].label, *markers[i].style,
			markers[i].help));
	AppendRows(rows);
	list->SetRows(rows);
	if (list->GetSelected() < 0)
		list->SelectFirst();
}

// Everything here measures text, so it is worked out once per set of markers
// rather than every frame.
void MarkerPanel::Measure() {
	listWidth = list->GetNaturalXSize();
	labelWidth = 0;
	for (const Marker& marker : markers) {
		labelWidth = (std::max)(labelWidth, marker.editor->GetNaturalLabelWidth());
		for (const Attached& attached : marker.attached)
			labelWidth = (std::max)(labelWidth,
				attached.label->GetXSize() + MAP_ICON_LABEL_GAP);
	}
	listWidth = (std::min)(listWidth, (unsigned int)MM_LIST_MAX);

	unsigned int detailHeight = 0, listMinHeight = list->GetNaturalYSize();
	detailMinWidth = 0;
	MeasureExtra(listWidth, labelWidth, detailMinWidth, detailHeight, listMinHeight);

	for (Marker& marker : markers) {
		marker.editor->SetLabelWidth(labelWidth);
		detailMinWidth = (std::max)(detailMinWidth, marker.editor->GetMinXSize());
		for (const Attached& attached : marker.attached)
			detailMinWidth = (std::max)(detailMinWidth,
				labelWidth + attached.control->GetXSize());
		detailHeight = (std::max)(detailHeight, marker.editor->GetYSize() +
			(unsigned int)marker.attached.size() * MAP_ICON_ROW_HEIGHT);
	}

	unsigned int detailX = UI_CONTENT_MARGIN - MM_ROW_INSET + listWidth +
		MM_DIVIDER_GAP + 1 + MM_DIVIDER_GAP;
	minWidth = markers.empty() ? 0 : detailX + detailMinWidth + UI_CONTENT_MARGIN;

	unsigned int tallest = (std::max)(listMinHeight + ListFooterHeight(), detailHeight);
	minHeight = markers.empty() ? 0 : MM_TOP + tallest + MM_BOTTOM;
}

// The list down the left, the rule beside it, and the editor of the marker in
// hand with what is attached to it under it. Every other marker's controls are
// put away, so only one marker's are ever answering the mouse.
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

	unsigned int innerHeight = tabHeight > MM_TOP + MM_BOTTOM ?
		tabHeight - MM_TOP - MM_BOTTOM : 0;
	unsigned int footer = (std::min)(ListFooterHeight(), innerHeight);

	list->SetActive(!markers.empty());
	list->SetBaseX(rowX);
	list->SetBaseY(MM_TOP);
	list->SetSize(listWidth, innerHeight - footer);

	divider->SetActive(!markers.empty());
	divider->SetBaseX(dividerX);
	divider->SetBaseY(MM_TOP);
	divider->SetYSize(innerHeight);

	int selected = list->GetSelected();
	for (unsigned int i = 0; i < markers.size(); i++) {
		Marker& marker = markers[i];
		bool editing = ((int)i == selected);
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

	if (!markers.empty())
		LayoutExtra(selected, rowX, listWidth, MM_TOP + innerHeight - footer, detailX,
			MM_TOP, detailWidth, labelWidth);
}

// Over the row the pointer is on, rebuilt only when that changes: the pointer
// rests on one row for many frames. Not while a picker or a dropdown is open,
// which the tip would be drawn over, nor through a window drawn over this one.
void MarkerPanel::UpdateTip() {
	int over = -1;
	if (IsActive() && !Colorhook::current && !Combohook::current && !OverlayOpen())
		over = list->GetHoveredRow();
	const MarkerListRow* row = over < 0 ? NULL : list->GetRow((unsigned int)over);
	unsigned int x = 0, y = 0, width = 0, height = 0;
	if (!row || row->help.empty() || !list->GetRowBox((unsigned int)over, x, y, width, height)) {
		HideTip();
		return;
	}

	if (over != shownTip) {
		std::vector<TooltipLine> lines;
		lines.push_back(TooltipLine(row->help, MM_TIP_COLOR));
		tip->SetMaxWidth(MM_TIP_WIDTH);
		tip->SetLines(lines);
		shownTip = over;
	}
	// After SetLines(): where it fits depends on how big it turned out.
	tip->PlaceAbove(x, y, width, height);
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

// Up and down the list, as through the settings window's rows. An open colour
// picker or dropdown belongs to the marker in hand, so the keys are left alone
// while one is up rather than moving to another marker from under it, and a box
// being typed in keeps the keys it does not use itself.
bool MarkerPanel::OnKey(bool up, BYTE key) {
	if (markers.empty() || Colorhook::current || Combohook::current ||
			Inputhook::current || OverlayOpen())
		return false;
	switch (key) {
		case VK_UP:
		case VK_DOWN:
		case VK_HOME:
		case VK_END:
			if (up)
				return true;
			switch (key) {
				case VK_UP:		list->MoveSelection(-1); break;
				case VK_DOWN:	list->MoveSelection(1); break;
				case VK_HOME:	list->SelectFirst(); break;
				case VK_END:	list->SelectLast(); break;
			}
			return true;
	}
	return false;
}

std::string MarkerPanel::ParentOffStatus() const {
	if (markers.empty())
		return std::string();
	const Marker& marker = markers[0];
	bool off = (marker.parentToggle && !marker.parentToggle->state) ||
		(marker.parentBool && !*marker.parentBool);
	if (!off)
		return std::string();
	return "Not drawn while " + marker.parentLabel + " is off";
}

// What stops the Forge's file being saved, which is said first, since nothing in
// any tab is kept until it is fixed; otherwise why the marker in hand is not on
// the map whatever it is set to. A derived panel's rows hang off what the
// markers hang off.
std::string MarkerPanel::GetStatus() {
	const std::string& problem = BH::forge->GetProblem();
	if (!problem.empty())
		return problem;
	int selected = list->GetSelected();
	if (selected < 0)
		return std::string();
	if (selected >= (int)markers.size())
		return ParentOffStatus();
	const Marker& marker = markers[selected];
	bool off = (marker.parentToggle && !marker.parentToggle->state) ||
		(marker.parentBool && !*marker.parentBool);
	if (!off)
		return std::string();
	return "Not drawn while " + marker.parentLabel + " is off";
}

void MarkerPanel::OnDraw() {
	EnsureMarkers();
	Sync();
	PushRows();
	Layout();
	UpdateTip();
}
