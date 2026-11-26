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
// room around the largest of them.
#define MIH_CELL				24
#define MIH_CELL_ICON_SIZE		3

// What the chosen and the hovered cell are outlined in, as in the colour picker.
#define MIH_CHOSEN_EDGE			0xD0	// grey
#define MIH_HOVER_EDGE			0x20	// white

// What a cell for no icon shows instead of one: a short rule, in grey.
#define MIH_NONE_HALF_WIDTH		4
#define MIH_NONE_COLOR			0xD0

// The editor's columns and rows. Labels take the first column and the controls
// they name start after it; the preview stands to the right of everything.
#define MIH_LABEL_WIDTH			44
#define MIH_SLIDER_WIDTH		120
#define MIH_ROW_GAP				8
#define MIH_INDENT				12
#define MIH_PREVIEW_GAP			14
#define MIH_PREVIEW_SIZE		72
#define MIH_PREVIEW_LINE_INSET	8
#define MIH_TEXT_HEIGHT			10		// font 0, as Sliderhook measures it
#define MIH_LABEL_COLOR			Gold
#define MIH_LABEL_HOVER			Tan

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
		Hook(group, x, y), style(style), icons(icons) {
}

unsigned int MapIconStriphook::GetXSize() {
	return (unsigned int)icons.size() * MIH_CELL;
}

unsigned int MapIconStriphook::GetYSize() {
	return MIH_CELL;
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
	int hovered = CellAt(Hook::GetMouseX(), Hook::GetMouseY());
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
		if (hovered == (int)i && IsEnabled())
			DrawEdges(left, top, MIH_CELL, MIH_CELL, MIH_HOVER_EDGE);
	}
	Unlock();
}

/* MapIconPreviewhook
 */
MapIconPreviewhook::MapIconPreviewhook(HookGroup* group, unsigned int x, unsigned int y,
		MapIconStyle* style) : Hook(group, x, y), style(style) {
}

unsigned int MapIconPreviewhook::GetXSize() {
	return MIH_PREVIEW_SIZE;
}

unsigned int MapIconPreviewhook::GetYSize() {
	return MIH_PREVIEW_SIZE;
}

void MapIconPreviewhook::OnDraw() {
	if (!IsActive())
		return;
	Lock();
	int left = (int)GetX();
	int top = (int)GetY();
	Framehook::Draw(left, top, MIH_PREVIEW_SIZE, MIH_PREVIEW_SIZE, 0, BTNormal);
	int cx = left + MIH_PREVIEW_SIZE / 2;
	int cy = top + MIH_PREVIEW_SIZE / 2;
	// From the corner the player would be in, drawn first so the icon sits on top
	// of it as it does on the map.
	if (style->line)
		Linehook::Draw(left + MIH_PREVIEW_LINE_INSET,
			top + MIH_PREVIEW_SIZE - MIH_PREVIEW_LINE_INSET, cx, cy, style->lineColor);
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
		line(NULL), lineColor(NULL), preview(NULL), xSize(0), ySize(0) {
	content = new MapIconContent(this);
	if (!options.icons.empty()) {
		iconLabel = new Texthook(content, 0, 0, "Icon");
		iconLabel->SetColor(MIH_LABEL_COLOR);
		strip = new MapIconStriphook(content, MIH_LABEL_WIDTH, 0, style, options.icons);
	}
	if (options.size) {
		sizeLabel = new Texthook(content, 0, 0, "Size");
		sizeLabel->SetColor(MIH_LABEL_COLOR);
		size = new Sliderhook(content, MIH_LABEL_WIDTH, 0, MIH_SLIDER_WIDTH,
			&style->size, MapIcon::MinSize, MapIcon::MaxSize, 1);
	}
	color = new Colorhook(content, 0, 0, &style->color, "Color");
	if (options.line) {
		line = new Checkhook(content, 0, 0, &style->line, "Draw line");
		line->SetTextColor(MIH_LABEL_COLOR);
		line->SetHoverColor(MIH_LABEL_HOVER);
		lineColor = new Colorhook(content, MIH_INDENT, 0, &style->lineColor, "Line color");
	}
	preview = new MapIconPreviewhook(content, 0, 0, style);
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

// The controls are in a group of the editor's own, which nothing else draws.
void MapIconhook::OnDraw() {
	if (!IsActive())
		return;
	Refresh();
	for (HookIterator it = content->Hooks.begin(); it != content->Hooks.end(); ++it)
		(*it)->OnDraw();
}

// Row by row from the top, each control's text set level with its label's. What
// a row is made of is fixed when the editor is built, so this places everything
// once; nothing in it depends on the style's values.
void MapIconhook::Layout() {
	unsigned int row = 0;
	unsigned int right = 0;

	if (strip) {
		strip->SetBaseY(row);
		iconLabel->SetBaseY(row + (MIH_CELL - MIH_TEXT_HEIGHT) / 2);
		right = MIH_LABEL_WIDTH + strip->GetXSize();
		row += MIH_CELL + MIH_ROW_GAP;
	}
	if (size) {
		size->SetBaseY(row);
		sizeLabel->SetBaseY(row + size->GetTextInset());
		if (MIH_LABEL_WIDTH + MIH_SLIDER_WIDTH > right)
			right = MIH_LABEL_WIDTH + MIH_SLIDER_WIDTH;
		row += size->GetYSize() + MIH_ROW_GAP;
	}
	color->SetBaseY(row);
	row += color->GetYSize() + MIH_ROW_GAP;
	if (line) {
		line->SetBaseY(row);
		row += CHECK_BOX_SIZE + MIH_ROW_GAP;
		lineColor->SetBaseY(row);
		row += lineColor->GetYSize();
	}

	preview->SetBaseX(right + MIH_PREVIEW_GAP);
	preview->SetBaseY(0);

	xSize = right + MIH_PREVIEW_GAP + MIH_PREVIEW_SIZE;
	ySize = (row > MIH_PREVIEW_SIZE) ? row : MIH_PREVIEW_SIZE;
}

void MapIconhook::Refresh() {
	MapIcon::Icon icon = style->Icon();
	bool hasColor = MapIcon::HasColor(icon);
	if (size)
		size->SetEnabled(MapIcon::HasSize(icon));
	color->SetEnabled(hasColor);
	// What the colour is drawn on: the icon itself where it takes one, a plain
	// square where it has no colour of its own to show.
	color->SetSwatch(hasColor ? icon : MapIcon::Square);
	if (lineColor)
		lineColor->SetEnabled(style->line);
}
