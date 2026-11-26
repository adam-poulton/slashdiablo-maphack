#include "MapIconhook.h"
#include "../../../D2Ptrs.h"
#include "../../Basic/Framehook/Framehook.h"
#include "../../Basic/Linehook/Linehook.h"
#include "../../Basic/Texthook/Texthook.h"
#include "../Checkhook/Checkhook.h"
#include "../Colorhook/Colorhook.h"
#include "../Sliderhook/Sliderhook.h"

using namespace Drawing;

// The strip: one square cell per icon, the icon drawn in it at a size that leaves
// room around the largest of them. A cell is a little shorter than a row, and its
// text inset is what centres the line of font 0 on it.
#define MIH_CELL				20
#define MIH_CELL_ICON_SIZE		3
#define MIH_TEXT_HEIGHT			10		// font 0
#define MIH_CELL_TEXT_INSET		((MIH_CELL - MIH_TEXT_HEIGHT) / 2)

// What the chosen and the hovered cell are outlined in, as in the colour picker.
#define MIH_CHOSEN_EDGE			0xD0	// grey
#define MIH_HOVER_EDGE			0x20	// white

// What a cell for no icon shows instead of one: a short rule, in grey.
#define MIH_NONE_HALF_WIDTH		4
#define MIH_NONE_COLOR			0xD0

// The preview across the top, and the gap under it. Together a whole number of
// rows, so the form's rows land on the same lines as rows laid beside the editor
// from its top.
#define MIH_PREVIEW_HEIGHT		56
#define MIH_PREVIEW_GAP			10
static_assert((MIH_PREVIEW_HEIGHT + MIH_PREVIEW_GAP) % MAP_ICON_ROW_HEIGHT == 0,
	"the form under the preview has to start on a row");

// Where the line in the preview starts: in from the bottom, and no further to
// the left of the icon than this, so it keeps one slope however wide the preview.
#define MIH_PREVIEW_LINE_INSET	8
#define MIH_PREVIEW_LINE_RUN	64
#define MIH_PREVIEW_EMPTY_TEXT	"Not drawn on the map"
#define MIH_PREVIEW_EMPTY_COLOR	Grey

// Between the strip and the name after it, and how far a setting that depends on
// another sits in from it, as the settings window has them.
#define MIH_READOUT_GAP			SLIDER_READOUT_GAP
#define MIH_INDENT				12

// The slider on its own, without a strip to take its width from; and the least it
// is drawn at once the editor is narrower than it would like.
#define MIH_SLIDER_WIDTH		150
#define MIH_SLIDER_MIN			60

#define MIH_LABEL_COLOR			Gold
#define MIH_LABEL_HOVER			Tan
#define MIH_READOUT_COLOR		Gold

static void DrawEdges(int x, int y, int width, int height, unsigned int color) {
	D2GFX_DrawRectangle(x, y, x + width, y + 1, color, BTNormal);
	D2GFX_DrawRectangle(x, y + height - 1, x + width, y + height, color, BTNormal);
	D2GFX_DrawRectangle(x, y, x + 1, y + height, color, BTNormal);
	D2GFX_DrawRectangle(x + width - 1, y, x + width, y + height, color, BTNormal);
}

// Where the icon is drawn from: the cell's middle. An even cell has no middle
// pixel, and the shapes lean a pixel to the top left, so the centre is taken on
// the right of the two.
static int CellCentre(int origin) {
	return origin + MIH_CELL / 2;
}

/* MapIconStriphook
 */
MapIconStriphook::MapIconStriphook(HookGroup* group, unsigned int x, unsigned int y,
		MapIconStyle* style, std::vector<MapIcon::Icon> icons) :
		Hook(group, x, y), style(style), icons(icons), readoutWidth(0) {
	for (MapIcon::Icon icon : icons) {
		unsigned int width = (unsigned int)Texthook::GetTextSize(MapIcon::Label(icon), 0).x;
		if (width > readoutWidth)
			readoutWidth = width;
	}
}

unsigned int MapIconStriphook::CellsWidth() {
	return (unsigned int)icons.size() * MIH_CELL;
}

unsigned int MapIconStriphook::GetXSize() {
	return CellsWidth() + MIH_READOUT_GAP + readoutWidth;
}

unsigned int MapIconStriphook::GetYSize() {
	return MIH_CELL;
}

unsigned int MapIconStriphook::GetTextInset() {
	return MIH_CELL_TEXT_INSET;
}

int MapIconStriphook::CellAt(unsigned int x, unsigned int y) {
	if (!InRange(x, y) || x < GetX())
		return -1;
	unsigned int cell = (x - GetX()) / MIH_CELL;
	return cell < icons.size() ? (int)cell : -1;
}

bool MapIconStriphook::OnLeftClick(bool up, unsigned int x, unsigned int y) {
	if (!IsEnabled())
		return false;
	int cell = CellAt(x, y);
	if (cell < 0)
		return false;
	if (up) {
		Lock();
		style->icon = icons[cell];
		Unlock();
	}
	return true;
}

void MapIconStriphook::OnDraw() {
	if (!IsActive())
		return;
	Lock();
	int hovered = IsEnabled() ? CellAt(Hook::GetMouseX(), Hook::GetMouseY()) : -1;
	for (unsigned int i = 0; i < icons.size(); i++) {
		int left = (int)GetX() + (int)i * MIH_CELL;
		int top = (int)GetY();
		int cx = CellCentre(left);
		int cy = CellCentre(top);
		if (icons[i] == MapIcon::None)
			D2GFX_DrawLine(cx - MIH_NONE_HALF_WIDTH, cy, cx + MIH_NONE_HALF_WIDTH, cy,
				MIH_NONE_COLOR, -1);
		else
			MapIcon::Draw(icons[i], MIH_CELL_ICON_SIZE, cx, cy, style->color);
		if (icons[i] == style->Icon())
			DrawEdges(left, top, MIH_CELL, MIH_CELL, MIH_CHOSEN_EDGE);
		if (hovered == (int)i)
			DrawEdges(left, top, MIH_CELL, MIH_CELL, MIH_HOVER_EDGE);
	}
	// What a click would pick while one is pointed at, as the colour picker's
	// preview does, and what is picked otherwise.
	MapIcon::Icon named = hovered >= 0 ? icons[hovered] : style->Icon();
	Texthook::Draw(GetX() + CellsWidth() + MIH_READOUT_GAP, GetY() + GetTextInset(),
		None, 0, IsEnabled() ? MIH_READOUT_COLOR : DISABLED_TEXT_COLOR, "%s",
		MapIcon::Label(named));
	Unlock();
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
	Framehook::Draw(left, top, xSize, ySize, 0, BTNormal);
	int cx = left + (int)xSize / 2;
	int cy = top + (int)ySize / 2;
	if (!style->Draws()) {
		Texthook::Draw(cx, cy - MIH_TEXT_HEIGHT / 2, Center, 0, MIH_PREVIEW_EMPTY_COLOR,
			MIH_PREVIEW_EMPTY_TEXT);
		Unlock();
		return;
	}
	// Drawn first so the icon sits on top of it, as it does on the map.
	if (style->line) {
		int fromX = cx - MIH_PREVIEW_LINE_RUN;
		if (fromX < left + MIH_PREVIEW_LINE_INSET)
			fromX = left + MIH_PREVIEW_LINE_INSET;
		Linehook::Draw(fromX, top + (int)ySize - MIH_PREVIEW_LINE_INSET, cx, cy,
			style->lineColor);
	}
	MapIcon::Draw(style->Icon(), style->size, cx, cy, style->color);
	Unlock();
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

/* MapIconhook
 */
MapIconhook::MapIconhook(HookGroup* group, unsigned int x, unsigned int y,
		MapIconStyle* style, MapIconOptions options) :
		Hook(group, x, y), style(style), options(options), content(NULL),
		iconLabel(NULL), strip(NULL), sizeLabel(NULL), size(NULL), color(NULL),
		line(NULL), lineColor(NULL), preview(NULL), xSize(0), ySize(0),
		labelWidth(0) {
	content = new MapIconContent(this);
	preview = new MapIconPreviewhook(content, 0, 0, style);
	if (!options.icons.empty()) {
		iconLabel = new Texthook(content, 0, 0, "Icon");
		iconLabel->SetColor(MIH_LABEL_COLOR);
		strip = new MapIconStriphook(content, 0, 0, style, options.icons);
	}
	if (options.size) {
		sizeLabel = new Texthook(content, 0, 0, "Size");
		sizeLabel->SetColor(MIH_LABEL_COLOR);
		size = new Sliderhook(content, 0, 0, MIH_SLIDER_WIDTH, &style->size,
			MapIcon::MinSize, MapIcon::MaxSize, 1);
	}
	color = new Colorhook(content, 0, 0, &style->color, "Color");
	if (options.line) {
		line = new Checkhook(content, 0, 0, &style->line, "Draw line");
		line->SetTextColor(MIH_LABEL_COLOR);
		line->SetHoverColor(MIH_LABEL_HOVER);
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
	delete strip;
	delete sizeLabel;
	delete size;
	delete color;
	delete line;
	delete lineColor;
	delete preview;
	delete content;
}

unsigned int MapIconhook::RowY(unsigned int rowTop, Hook* hook) {
	return rowTop + MAP_ICON_ROW_TEXT_TOP - hook->GetTextInset();
}

unsigned int MapIconhook::GetNaturalLabelWidth() {
	unsigned int widest = 0;
	Texthook* labels[] = { iconLabel, sizeLabel };
	for (Texthook* label : labels) {
		if (label && label->GetXSize() > widest)
			widest = label->GetXSize();
	}
	return widest > 0 ? widest + MAP_ICON_LABEL_GAP : 0;
}

// The strip and the slider are drawn the same width, so the two rows end
// together; the slider alone keeps the width the settings window gives one.
unsigned int MapIconhook::GetMinXSize() {
	unsigned int controls = strip ? strip->GetXSize() : MIH_SLIDER_WIDTH;
	return labelWidth + controls;
}

void MapIconhook::SetXSize(unsigned int width) {
	if (width == xSize)
		return;
	xSize = width;
	Layout();
}

void MapIconhook::SetLabelWidth(unsigned int width) {
	if (width == labelWidth)
		return;
	labelWidth = width;
	Layout();
}

// Row by row from the top. Only where things are placed depends on the width;
// nothing in it depends on the style's values.
void MapIconhook::Layout() {
	preview->SetBaseX(0);
	preview->SetBaseY(0);
	preview->SetSize(xSize, MIH_PREVIEW_HEIGHT);

	unsigned int row = MIH_PREVIEW_HEIGHT + MIH_PREVIEW_GAP;
	if (strip) {
		iconLabel->SetBaseY(RowY(row, iconLabel));
		strip->SetBaseX(labelWidth);
		strip->SetBaseY(RowY(row, strip));
		row += MAP_ICON_ROW_HEIGHT;
	}
	if (size) {
		unsigned int wanted = strip ? strip->GetXSize() : MIH_SLIDER_WIDTH;
		unsigned int room = xSize > labelWidth ? xSize - labelWidth : 0;
		unsigned int width = wanted < room ? wanted : room;
		size->SetXSize(width > MIH_SLIDER_MIN ? width : MIH_SLIDER_MIN);
		sizeLabel->SetBaseY(RowY(row, sizeLabel));
		size->SetBaseX(labelWidth);
		size->SetBaseY(RowY(row, size));
		row += MAP_ICON_ROW_HEIGHT;
	}
	color->SetBaseY(RowY(row, color));
	row += MAP_ICON_ROW_HEIGHT;
	if (line) {
		line->SetBaseY(RowY(row, line));
		row += MAP_ICON_ROW_HEIGHT;
		lineColor->SetBaseY(RowY(row, lineColor));
		row += MAP_ICON_ROW_HEIGHT;
	}
	ySize = row;
}

void MapIconhook::Refresh() {
	MapIcon::Icon icon = style->Icon();
	bool hasSize = MapIcon::HasSize(icon);
	bool hasColor = MapIcon::HasColor(icon);
	if (size) {
		size->SetEnabled(hasSize);
		sizeLabel->SetEnabled(hasSize);
	}
	color->SetEnabled(hasColor);
	// What the colour is drawn on: the icon itself where it takes one, a plain
	// square where it has no colour of its own to show.
	color->SetSwatch(hasColor ? icon : MapIcon::Square);
	if (lineColor)
		lineColor->SetEnabled(style->line);
}

// The controls are in a group of the editor's own, which nothing else draws.
void MapIconhook::OnDraw() {
	if (!IsActive())
		return;
	Refresh();
	for (HookIterator it = content->Hooks.begin(); it != content->Hooks.end(); ++it)
		(*it)->OnDraw();
}
