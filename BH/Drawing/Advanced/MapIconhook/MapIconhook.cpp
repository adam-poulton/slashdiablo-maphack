#include "MapIconhook.h"
#include <algorithm>
#include "../../../D2Ptrs.h"
#include "../../Basic/Boxhook/Boxhook.h"
#include "../../Basic/Framehook/Framehook.h"
#include "../../Basic/Linehook/Linehook.h"
#include "../../Basic/Texthook/Texthook.h"
#include "../Checkhook/Checkhook.h"
#include "../Colorhook/Colorhook.h"
#include "../Sliderhook/Sliderhook.h"

using namespace Drawing;

// An option of the icon dropdown, and the box showing the one chosen: a square
// cell for the icon, tall enough for the game's sprite with a pixel clear of the
// frame above and below it, then the name. Odd, so a cell has a middle pixel to
// centre on. The shapes are drawn at the size the marker list draws them. The
// icon's row keeps a pixel clear around the box in turn.
#define MIH_CELL				27
#define MIH_CELL_ICON_SIZE		4
#define MIH_ICON_ROW_HEIGHT		(MIH_CELL + 2)
#define MIH_TEXT_HEIGHT			10		// font 0
#define MIH_CELL_TEXT_INSET		((MIH_CELL - MIH_TEXT_HEIGHT) / 2)
static_assert(MIH_CELL >= MapIcon::ChestHeight + 4,
	"a cell holds the game's sprite clear of the frame");

// Where an option's name starts, the room kept between it and the arrow, and how
// many options the list shows before it scrolls.
#define MIH_OPTION_LABEL_LEFT	(MIH_CELL + 2)
#define MIH_OPTION_ARROW_ROOM	6
#define MIH_OPTION_ROWS			6

// What an option for no icon shows instead of one: a short rule, in grey.
#define MIH_NONE_HALF_WIDTH		4
#define MIH_NONE_COLOR			0xD0

// What the options are drawn in when the style itself draws nothing at the
// marker, so there is still something to choose between: a grey outline.
#define MIH_BARE_LOOK			MapIcon::Look(MapIcon::NoColor, 0xD0, 1)

// The preview across the top, under its name, and the gap under it.
#define MIH_PREVIEW_LABEL_TEXT	"Preview"
#define MIH_PREVIEW_LABEL_HEIGHT	16
#define MIH_PREVIEW_HEIGHT		56
#define MIH_PREVIEW_GAP			10

// A shape is shown twice, in two boxes that split the preview evenly: at the
// size the map draws it, and beside that magnified, where its border and fill
// can be made out. Each box says how many times life size it is in its bottom
// left corner. The shape keeps clear of the frame and of that caption, and a
// preview with less than the least room for two boxes shows the marker at the
// map's size alone.
#define MIH_PREVIEW_BOX_MIN			64
#define MIH_PREVIEW_MAGNIFY_MOST	6
#define MIH_PREVIEW_PAD				4
#define MIH_PREVIEW_CAPTION_INSET	4
#define MIH_PREVIEW_CAPTION_COLOR	Grey

// Where the line in the preview starts: in from the bottom, and no further to
// the left of the icon than this, so it keeps one slope however wide the preview.
#define MIH_PREVIEW_LINE_INSET	8
#define MIH_PREVIEW_LINE_RUN	64
#define MIH_PREVIEW_EMPTY_TEXT	"Not drawn on the map"
#define MIH_PREVIEW_EMPTY_COLOR	Grey

// How far a setting that depends on another sits in from it, as the settings
// window has them.
#define MIH_INDENT				12

// The sliders' rail, which both share so their thumbs line up; and the least it
// is drawn at once the editor is narrower than it would like.
#define MIH_RAIL_WIDTH			96
#define MIH_RAIL_MIN			40
#define MIH_BORDER_UNIT			" px"

#define MIH_LABEL_COLOR			Gold
#define MIH_LABEL_HOVER			Tan

// What a style laid over another says of a part it leaves alone, and what it can
// do to the line, in MapIconOverride::Line order.
#define MIH_UNCHANGED_TEXT		"Unchanged"
static const char* const LineChanges[] = { MIH_UNCHANGED_TEXT, "None", "Drawn" };

/* MapIconCombohook
 */
std::vector<std::string> MapIconCombohook::Labels(const std::vector<MapIcon::Icon>& icons,
		bool unchanged) {
	std::vector<std::string> labels;
	if (unchanged)
		labels.push_back(MIH_UNCHANGED_TEXT);
	for (MapIcon::Icon icon : icons)
		labels.push_back(MapIcon::Label(icon));
	return labels;
}

unsigned int MapIconCombohook::Width(const std::vector<MapIcon::Icon>& icons,
		bool unchanged) {
	unsigned int widest = 0;
	for (const std::string& label : Labels(icons, unchanged)) {
		unsigned int width = (unsigned int)Texthook::GetTextSize(label, 0).x;
		if (width > widest)
			widest = width;
	}
	unsigned int arrow = (unsigned int)Texthook::GetTextSize("v", 0).x;
	return MIH_OPTION_LABEL_LEFT + widest + MIH_OPTION_ARROW_ROOM + arrow + COMBO_PADDING_X;
}

MapIconCombohook::MapIconCombohook(HookGroup* group, unsigned int x, unsigned int y,
		MapIconStyle* style, std::vector<MapIcon::Icon> icons,
		const MapIconOverride* partial) :
		Combohook(group, x, y, Width(icons, partial != NULL), &index,
			Labels(icons, partial != NULL)),
		style(style), icons(icons), partial(partial), index(0) {
	SetMaxVisible(MIH_OPTION_ROWS);
}

unsigned int MapIconCombohook::GetYSize() {
	return MIH_CELL;
}

unsigned int MapIconCombohook::GetRowHeight() {
	return MIH_CELL;
}

unsigned int MapIconCombohook::GetTextInset() {
	return MIH_CELL_TEXT_INSET;
}

void MapIconCombohook::OnSelected(unsigned int option) {
	if (option >= icons.size() + Lead())
		return;
	Lock();
	if (partial && option == 0) {
		*partial->setsIcon = false;
	} else {
		style->icon = icons[option - Lead()];
		if (partial)
			*partial->setsIcon = true;
	}
	Unlock();
}

void MapIconCombohook::DrawIcon(MapIcon::Icon shown, const MapIcon::Look& look,
		int cx, int cy) {
	if (shown == MapIcon::None) {
		D2GFX_DrawLine(cx - MIH_NONE_HALF_WIDTH, cy, cx + MIH_NONE_HALF_WIDTH, cy,
			MIH_NONE_COLOR, -1);
		return;
	}
	MapIcon::Draw(shown, MIH_CELL_ICON_SIZE, cx, cy, look.Draws() ? look : MIH_BARE_LOOK);
}

void MapIconCombohook::DrawOption(unsigned int option, unsigned int x, unsigned int y,
		unsigned int width, TextColor color) {
	if (option >= icons.size() + Lead())
		return;
	int cx = (int)x + MIH_CELL / 2;
	int cy = (int)y + MIH_CELL / 2;
	const char* label = MIH_UNCHANGED_TEXT;
	if (partial && option == 0) {
		const MapIconStyle& base = *partial->base;
		DrawIcon(base.DrawsIcon() ? base.Icon() : MapIcon::None, base.Look(), cx, cy);
	} else {
		MapIcon::Icon shown = icons[option - Lead()];
		DrawIcon(shown, style->Look(), cx, cy);
		label = MapIcon::Label(shown);
	}
	Texthook::Draw(x + MIH_OPTION_LABEL_LEFT, y + GetTextInset(), None, 0, color, "%s",
		label);
}

void MapIconCombohook::OnDraw() {
	if (partial && !*partial->setsIcon) {
		index = 0;
	} else {
		for (unsigned int i = 0; i < icons.size(); i++) {
			if (icons[i] == style->Icon()) {
				index = i + Lead();
				break;
			}
		}
	}
	Combohook::OnDraw();
}

/* MapIconPreviewhook
 */
MapIconPreviewhook::MapIconPreviewhook(HookGroup* group, unsigned int x, unsigned int y,
		MapIconStyle* style) : Hook(group, x, y), style(style), xSize(0), ySize(0) {
}

void MapIconPreviewhook::SetSize(unsigned int width, unsigned int height) {
	Lock();
	xSize = width;
	ySize = height;
	Unlock();
}

void MapIconPreviewhook::OnDraw() {
	if (!IsActive() || xSize == 0 || ySize == 0)
		return;
	Lock();
	int left = (int)GetX();
	int top = (int)GetY();
	if (!style->Draws()) {
		Framehook::Draw(left, top, xSize, ySize, 0, BTNormal);
		Texthook::Draw(left + (int)xSize / 2, top + ((int)ySize - MIH_TEXT_HEIGHT) / 2,
			Center, 0, MIH_PREVIEW_EMPTY_COLOR, MIH_PREVIEW_EMPTY_TEXT);
		Unlock();
		return;
	}

	// The game's sprite is big enough to read as it is, and a preview too narrow
	// for two boxes keeps the one that is true to the map.
	bool magnified = MapIcon::IsShape(style->Icon()) && style->Look().Draws() &&
		xSize >= 2 * MIH_PREVIEW_BOX_MIN;
	if (!magnified) {
		Framehook::Draw(left, top, xSize, ySize, 0, BTNormal);
		DrawPane(left, top, (int)xSize, (int)ySize, 1);
		Unlock();
		return;
	}

	// A frame is drawn on its box's outermost pixels and the pixel outside them,
	// so where the two boxes meet their frames land on the same two columns and
	// the line between them is the frame's own.
	int actualWidth = (int)xSize / 2;
	int magnifiedLeft = left + actualWidth;
	int magnifiedWidth = (int)xSize - actualWidth;
	int roomHeight = (int)ySize - MIH_PREVIEW_CAPTION_INSET - MIH_TEXT_HEIGHT;

	bool bordered = style->borderColor != MapIcon::NoColor;
	MapIcon::Rect bounds = MapIcon::Extent(MapIcon::Rasterize(style->Icon(),
		style->size, bordered ? style->borderWidth : 0));
	unsigned int scale = MapIcon::FitScale(bounds,
		magnifiedWidth - 2 * MIH_PREVIEW_PAD, roomHeight - MIH_PREVIEW_PAD,
		MIH_PREVIEW_MAGNIFY_MOST);

	DrawBox(left, top, actualWidth, 1);
	DrawBox(magnifiedLeft, top, magnifiedWidth, scale);
	Unlock();
}

// One of the two boxes: its frame, the marker in what the caption leaves of it,
// and the caption.
void MapIconPreviewhook::DrawBox(int left, int top, int width, unsigned int scale) {
	int captionTop = top + (int)ySize - MIH_PREVIEW_CAPTION_INSET - MIH_TEXT_HEIGHT;
	Framehook::Draw(left, top, width, ySize, 0, BTNormal);
	DrawPane(left, top, width, captionTop - top, scale);
	Texthook::Draw(left + MIH_PREVIEW_CAPTION_INSET, captionTop, None, 0,
		MIH_PREVIEW_CAPTION_COLOR, "%ux", scale);
}

// The marker in the middle of a patch of the preview, each of its pixels drawn
// scale pixels square, with its line running in from the bottom left.
void MapIconPreviewhook::DrawPane(int left, int top, int width, int height,
		unsigned int scale) {
	int cx = left + width / 2;
	int cy = top + height / 2;
	// Drawn first so the icon sits on top of it, as it does on the map.
	if (style->line) {
		int fromX = cx - MIH_PREVIEW_LINE_RUN * (int)scale;
		if (fromX < left + MIH_PREVIEW_LINE_INSET)
			fromX = left + MIH_PREVIEW_LINE_INSET;
		int fromY = top + height - MIH_PREVIEW_LINE_INSET;
		// As many pixels thick as the pane is magnified: that many lines, stacked
		// across whichever way the line runs least, so they lie side by side
		// instead of along one another.
		bool shallow = (cx - fromX) > (fromY - cy);
		for (int i = 0; i < (int)scale; i++) {
			int offset = i - (int)scale / 2;
			int dx = shallow ? 0 : offset, dy = shallow ? offset : 0;
			Linehook::Draw(fromX + dx, fromY + dy, cx + dx, cy + dy, style->lineColor);
		}
	}
	if (scale > 1)
		MapIcon::DrawMagnified(style->Icon(), style->size, scale, cx, cy, style->Look());
	else
		MapIcon::Draw(style->Icon(), style->size, cx, cy, style->Look());
}

/* MapIconContent
 */
unsigned int MapIconContent::GetX() {
	return editor->GetX();
}

unsigned int MapIconContent::GetY() {
	return editor->GetY();
}

unsigned int MapIconContent::GetXSize() {
	return editor->GetXSize();
}

unsigned int MapIconContent::GetYSize() {
	return editor->GetYSize();
}

bool MapIconContent::IsActive() {
	return editor->IsActive();
}

HookVisibility MapIconContent::GetVisibility() {
	return editor->GetScreen();
}

bool MapIconContent::IsCoveredAt(unsigned int x, unsigned int y) {
	return editor->IsCoveredAt(x, y);
}

/* MapIconhook
 */
static Texthook* Label(HookGroup* group, unsigned int x, const char* text) {
	Texthook* label = new Texthook(group, x, 0, text);
	label->SetColor(MIH_LABEL_COLOR);
	return label;
}

MapIconhook::MapIconhook(HookGroup* group, unsigned int x, unsigned int y,
		MapIconStyle* style, MapIconOptions options) :
		Hook(group, x, y), style(style), options(options), overrides(false),
		shown(*style), content(NULL),
		iconLabel(NULL), icon(NULL), sizeLabel(NULL), size(NULL), fill(NULL),
		border(NULL), widthLabel(NULL), width(NULL), line(NULL), lineLabel(NULL),
		lineChange(NULL), lineColor(NULL),
		previewLabel(NULL), preview(NULL), xSize(0), ySize(0), labelWidth(0) {
	Build();
}

MapIconhook::MapIconhook(HookGroup* group, unsigned int x, unsigned int y,
		MapIconStyle* style, MapIconOptions options, MapIconOverride partial) :
		Hook(group, x, y), style(style), options(options), partial(partial),
		overrides(true), shown(partial.Over(*style)), content(NULL),
		iconLabel(NULL), icon(NULL), sizeLabel(NULL), size(NULL), fill(NULL),
		border(NULL), widthLabel(NULL), width(NULL), line(NULL), lineLabel(NULL),
		lineChange(NULL), lineColor(NULL),
		previewLabel(NULL), preview(NULL), xSize(0), ySize(0), labelWidth(0) {
	Build();
}

void MapIconhook::Build() {
	content = new MapIconContent(this);
	previewLabel = Label(content, 0, MIH_PREVIEW_LABEL_TEXT);
	preview = new MapIconPreviewhook(content, 0, 0, &shown);
	if (!options.icons.empty()) {
		iconLabel = Label(content, 0, "Icon");
		icon = new MapIconCombohook(content, 0, 0, style, options.icons,
			overrides ? &partial : NULL);
	}
	if (options.size) {
		sizeLabel = Label(content, 0, "Size");
		size = new Sliderhook(content, 0, 0, MIH_RAIL_WIDTH, &style->size,
			MapIcon::MinSize, MapIcon::MaxSize, 1);
	}
	fill = new Colorhook(content, 0, 0, &style->fillColor, "Fill color");
	fill->SetAllowNone(true);
	border = new Colorhook(content, 0, 0, &style->borderColor, "Border color");
	border->SetAllowNone(true);
	widthLabel = Label(content, MIH_INDENT, "Width");
	width = new Sliderhook(content, 0, 0, MIH_RAIL_WIDTH, &style->borderWidth,
		MapIcon::MinBorder, MapIcon::MaxBorder, 1, MIH_BORDER_UNIT);
	if (options.line) {
		if (overrides) {
			std::vector<std::string> changes(LineChanges, LineChanges + 3);
			unsigned int widest = 0;
			for (const std::string& change : changes)
				widest = (std::max)(widest, (unsigned int)Texthook::GetTextSize(change, 0).x);
			lineLabel = Label(content, 0, "Line");
			lineChange = new Combohook(content, 0, 0,
				COMBO_PADDING_X + widest + MIH_OPTION_ARROW_ROOM +
					(unsigned int)Texthook::GetTextSize("v", 0).x + COMBO_PADDING_X,
				partial.line, changes);
		} else {
			line = new Checkhook(content, 0, 0, &style->line, "Draw line");
			line->SetTextColor(MIH_LABEL_COLOR);
			line->SetHoverColor(MIH_LABEL_HOVER);
		}
		lineColor = new Colorhook(content, MIH_INDENT, 0, &style->lineColor, "Line color");
	}
	labelWidth = GetNaturalLabelWidth();
	xSize = GetMinXSize();
	Layout();
}

MapIconhook::~MapIconhook() {
	// The controls take themselves out of the content group as they go, so it
	// goes last.
	delete iconLabel;
	delete icon;
	delete sizeLabel;
	delete size;
	delete fill;
	delete border;
	delete widthLabel;
	delete width;
	delete line;
	delete lineLabel;
	delete lineChange;
	delete lineColor;
	delete previewLabel;
	delete preview;
	delete content;
}

unsigned int MapIconhook::RowY(unsigned int rowTop, Hook* hook) {
	return rowTop + MAP_ICON_ROW_TEXT_TOP - hook->GetTextInset();
}

// A label's reach is from the column's left edge, so an indented one counts its
// indent.
unsigned int MapIconhook::GetNaturalLabelWidth() {
	unsigned int widest = 0;
	Texthook* labels[] = { iconLabel, sizeLabel, widthLabel, lineLabel };
	for (Texthook* label : labels) {
		if (label && label->GetBaseX() + label->GetXSize() > widest)
			widest = label->GetBaseX() + label->GetXSize();
	}
	return widest > 0 ? widest + MAP_ICON_LABEL_GAP : 0;
}

unsigned int MapIconhook::ReadoutRoom() {
	unsigned int room = 0;
	Sliderhook* sliders[] = { size, width };
	for (Sliderhook* slider : sliders) {
		if (slider && slider->ReadoutWidth() + SLIDER_READOUT_GAP > room)
			room = slider->ReadoutWidth() + SLIDER_READOUT_GAP;
	}
	return room;
}

unsigned int MapIconhook::GetMinXSize() {
	unsigned int controls = MIH_RAIL_WIDTH + ReadoutRoom();
	if (icon && icon->GetXSize() > controls)
		controls = icon->GetXSize();
	if (lineChange && lineChange->GetXSize() > controls)
		controls = lineChange->GetXSize();
	return labelWidth + controls;
}

void MapIconhook::SetXSize(unsigned int newWidth) {
	if (newWidth == xSize)
		return;
	xSize = newWidth;
	Layout();
}

void MapIconhook::SetLabelWidth(unsigned int newWidth) {
	if (newWidth == labelWidth)
		return;
	labelWidth = newWidth;
	Layout();
}

// Row by row from the top. Only where things are placed depends on the width;
// nothing in it depends on the style's values.
void MapIconhook::Layout() {
	previewLabel->SetBaseY(0);
	preview->SetBaseX(0);
	preview->SetBaseY(MIH_PREVIEW_LABEL_HEIGHT);
	preview->SetSize(xSize, MIH_PREVIEW_HEIGHT);

	// One rail for both sliders, cut short only by an editor too narrow for it.
	unsigned int readout = ReadoutRoom();
	unsigned int room = xSize > labelWidth + readout ? xSize - labelWidth - readout : 0;
	unsigned int rail = room < MIH_RAIL_WIDTH ? room : MIH_RAIL_WIDTH;
	if (rail < MIH_RAIL_MIN)
		rail = MIH_RAIL_MIN;

	unsigned int row = MIH_PREVIEW_LABEL_HEIGHT + MIH_PREVIEW_HEIGHT + MIH_PREVIEW_GAP;
	if (icon) {
		// A row of its own height, its label centred on it as the others are.
		unsigned int textTop = (MIH_ICON_ROW_HEIGHT - MIH_TEXT_HEIGHT) / 2;
		iconLabel->SetBaseY(row + textTop);
		icon->SetBaseX(labelWidth);
		icon->SetBaseY(row + textTop - icon->GetTextInset());
		row += MIH_ICON_ROW_HEIGHT;
	}
	if (size) {
		size->SetXSize(rail + size->ReadoutWidth() + SLIDER_READOUT_GAP);
		sizeLabel->SetBaseY(RowY(row, sizeLabel));
		size->SetBaseX(labelWidth);
		size->SetBaseY(RowY(row, size));
		row += MAP_ICON_ROW_HEIGHT;
	}
	fill->SetBaseY(RowY(row, fill));
	row += MAP_ICON_ROW_HEIGHT;
	border->SetBaseY(RowY(row, border));
	row += MAP_ICON_ROW_HEIGHT;
	width->SetXSize(rail + width->ReadoutWidth() + SLIDER_READOUT_GAP);
	widthLabel->SetBaseY(RowY(row, widthLabel));
	width->SetBaseX(labelWidth);
	width->SetBaseY(RowY(row, width));
	row += MAP_ICON_ROW_HEIGHT;
	if (lineColor) {
		if (lineChange) {
			lineLabel->SetBaseY(RowY(row, lineLabel));
			lineChange->SetBaseX(labelWidth);
			lineChange->SetBaseY(RowY(row, lineChange));
		} else {
			line->SetBaseY(RowY(row, line));
		}
		row += MAP_ICON_ROW_HEIGHT;
		lineColor->SetBaseY(RowY(row, lineColor));
		row += MAP_ICON_ROW_HEIGHT;
	}
	ySize = row;
}

void MapIconhook::Refresh() {
	shown = overrides ? partial.Over(*style) : *style;
	bool shape = MapIcon::IsShape(style->Icon()) && (!overrides || *partial.setsIcon);
	bool bordered = shape && style->borderColor != MapIcon::NoColor;
	if (size) {
		size->SetEnabled(shape);
		sizeLabel->SetEnabled(shape);
	}
	fill->SetEnabled(shape);
	border->SetEnabled(shape);
	width->SetEnabled(bordered);
	widthLabel->SetEnabled(bordered);
	if (lineColor)
		lineColor->SetEnabled(overrides ? *partial.line == MapIconOverride::LineOn :
			style->line);
}

// The controls are in a group of the editor's own, which nothing else draws.
void MapIconhook::OnDraw() {
	if (!IsActive())
		return;
	Refresh();
	for (HookIterator it = content->Hooks.begin(); it != content->Hooks.end(); ++it)
		(*it)->OnDraw();
}
