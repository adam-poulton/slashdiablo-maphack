#include "MapMarkersPanel.h"
#include <algorithm>
#include <set>
#include "../Settings/SettingsRegistry.h"
#include "../../Drawing/Basic/Texthook/Texthook.h"
#include "../../Drawing/Advanced/Combohook/Combohook.h"

using namespace Drawing;

// The list down the left, a row per marker, and the editor beside it.
#define MM_TOP				8
#define MM_LIST_WIDTH		132
#define MM_ROW_HEIGHT		20
#define MM_EDITOR_GAP		12
#define MM_ATTACHED_GAP		12
#define MM_ATTACHED_ROW		20
#define MM_ATTACHED_LABEL	96
#define MM_ATTACHED_WIDTH	120
#define MM_HELP_LINES		3
#define MM_HELP_LINE		12

// A row's marker is drawn in a column of its own, no larger than the row has
// room for, and the name starts after it.
#define MM_ROW_ICON_CENTRE	10
#define MM_ROW_ICON_SIZE	4
#define MM_ROW_LABEL_LEFT	24
#define MM_TEXT_HEIGHT		10		// font 0

// Gold for what can be clicked, white for the one in hand, grey for a marker that
// draws nothing at all.
#define MM_ROW_COLOR		Gold
#define MM_ROW_HOVER		Tan
#define MM_ROW_SELECTED		White
#define MM_ROW_OFF			Grey
#define MM_HELP_COLOR		Grey

/* MapMarkerRowhook
 */
MapMarkerRowhook::MapMarkerRowhook(HookGroup* group, unsigned int x, unsigned int y,
		unsigned int xSize, MapIconStyle* style, std::string label,
		unsigned int index, unsigned int* selected) :
		Hook(group, x, y), style(style), label(label), index(index),
		selected(selected), xSize(xSize) {
}

unsigned int MapMarkerRowhook::GetYSize() {
	return MM_ROW_HEIGHT;
}

bool MapMarkerRowhook::OnLeftClick(bool up, unsigned int x, unsigned int y) {
	if (!InRange(x, y))
		return false;
	if (up)
		*selected = index;
	return true;
}

void MapMarkerRowhook::OnDraw() {
	if (!IsActive())
		return;
	Lock();
	int centreY = (int)GetY() + MM_ROW_HEIGHT / 2;
	unsigned int size = (std::min)(style->size, (unsigned int)MM_ROW_ICON_SIZE);
	MapIcon::Draw(style->Icon(), size, (int)GetX() + MM_ROW_ICON_CENTRE, centreY,
		style->color);

	TextColor color = MM_ROW_COLOR;
	if (*selected == index)
		color = MM_ROW_SELECTED;
	else if (InRange(Hook::GetMouseX(), Hook::GetMouseY()))
		color = MM_ROW_HOVER;
	else if (!style->Draws())
		color = MM_ROW_OFF;
	Texthook::Draw(GetX() + MM_ROW_LABEL_LEFT, centreY - MM_TEXT_HEIGHT / 2, None, 0,
		color, "%s", label.c_str());
	Unlock();
}

/* MapMarkersPanel
 */
MapMarkersPanel::MapMarkersPanel(UI* ui) :
		UIPanel("Markers", ui), selected(0), builtVersion(0) {
}

void MapMarkersPanel::EnsureMarkers() {
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
		Settings::InCategory(Settings::Category::MapMarkers);

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
		marker.row = new MapMarkerRowhook(tab, UI_CONTENT_MARGIN, 0, MM_LIST_WIDTH,
			setting->styleValue, setting->label, (unsigned int)markers.size(), &selected);
		marker.editor = new MapIconhook(tab, 0, 0, setting->styleValue,
			setting->styleOptions);
		marker.help = setting->help;
		marker.helpWidth = 0;
		for (unsigned int line = 0; line < MM_HELP_LINES; line++) {
			Texthook* text = new Texthook(tab, 0, 0, "");
			text->SetColor(MM_HELP_COLOR);
			marker.helpLines.push_back(text);
		}
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
}

// The rows top down, and the editor of the marker in hand with what is attached
// to it under it. Every other marker's controls are put away, so only one
// marker's are ever answering the mouse.
void MapMarkersPanel::Layout() {
	unsigned int rowY = MM_TOP;
	unsigned int editorX = UI_CONTENT_MARGIN + MM_LIST_WIDTH + MM_EDITOR_GAP;
	unsigned int editorRight = tab->GetXSize() > UI_CONTENT_MARGIN ?
		tab->GetXSize() - UI_CONTENT_MARGIN : 0;
	unsigned int helpWidth = editorRight > editorX ? editorRight - editorX : 0;

	for (unsigned int i = 0; i < markers.size(); i++) {
		Marker& marker = markers[i];
		marker.row->SetBaseY(rowY);
		rowY += MM_ROW_HEIGHT;

		bool editing = (i == selected);
		marker.editor->SetActive(editing);
		marker.editor->SetBaseX(editorX);
		marker.editor->SetBaseY(MM_TOP);

		unsigned int y = MM_TOP + marker.editor->GetYSize() + MM_ATTACHED_GAP;
		for (Attached& attached : marker.attached) {
			attached.label->SetActive(editing);
			attached.control->SetActive(editing);
			attached.label->SetBaseX(editorX);
			attached.label->SetBaseY(y + attached.control->GetTextInset());
			attached.control->SetBaseX(editorX + MM_ATTACHED_LABEL);
			attached.control->SetBaseY(y);
			y += MM_ATTACHED_ROW;
		}

		// Broken to the window's width only while it is the one shown: measuring
		// text is a call into the game, and the others are not drawn.
		if (editing && marker.helpWidth != helpWidth) {
			marker.helpWidth = helpWidth;
			std::vector<std::string> lines = marker.help.empty() ?
				std::vector<std::string>() : Texthook::Wrap(marker.help, helpWidth, 0);
			for (unsigned int line = 0; line < marker.helpLines.size(); line++)
				marker.helpLines[line]->SetText("%s",
					line < lines.size() ? lines[line].c_str() : "");
		}
		for (Texthook* line : marker.helpLines) {
			line->SetActive(editing && line->GetText().length() > 0);
			line->SetBaseX(editorX);
			line->SetBaseY(y);
			y += MM_HELP_LINE;
		}
	}
}

void MapMarkersPanel::OnDraw() {
	EnsureMarkers();
	Layout();
}
