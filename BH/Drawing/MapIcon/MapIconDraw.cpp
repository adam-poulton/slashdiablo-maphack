#include "MapIcon.h"
#include <map>
#include <string.h>
#include <tuple>
#include "../../D2Ptrs.h"
#include "../Hook.h"

namespace MapIcon {
	namespace {
		// The brightness the automap draws a cell at when nothing fades it.
		const DWORD CellBrightness = 5;

		// The game zeroes this much of the context before filling it in (D2Client
		// 0x60317), more than the fields CellContext names.
		const unsigned int CellContextSize = 0x48;

		void DrawCell(unsigned int cell, int x, int y) {
			CellFile* cells = *p_D2CLIENT_AutomapCells;
			if (!cells)
				return;
			BYTE buffer[CellContextSize];
			memset(buffer, 0, sizeof(buffer));
			CellContext* context = (CellContext*)buffer;
			context->nCellNo = cell;
			context->pCellFile = cells;
			CellClip clip = { 0, (int)Drawing::Hook::GetScreenWidth() - 1, 0,
				(int)Drawing::Hook::GetScreenHeight() - 1 };
			D2GFX_DrawAutomapCell(context, x, y, &clip, CellBrightness);
		}

		// Worked out once for each icon, size and width there is: every chest on
		// the map is drawn from it every frame. Only ever reached from the game's
		// drawing, so it needs no lock.
		const Raster& Cached(Icon icon, unsigned int size, unsigned int borderWidth) {
			static std::map<std::tuple<int, unsigned int, unsigned int>, Raster> cache;
			std::tuple<int, unsigned int, unsigned int> key((int)icon, size, borderWidth);
			auto found = cache.find(key);
			if (found == cache.end())
				found = cache.emplace(key, Rasterize(icon, size, borderWidth)).first;
			return found->second;
		}

		void DrawRects(const std::vector<Rect>& rects, int x, int y, unsigned int color,
				int scale) {
			for (const Rect& rect : rects)
				D2GFX_DrawRectangle(x + rect.x * scale, y + rect.y * scale,
					x + (rect.x + rect.width) * scale, y + (rect.y + rect.height) * scale,
					color, Drawing::BTHighlight);
		}

		void DrawShape(Icon icon, unsigned int size, int x, int y, const Look& look,
				int scale) {
			if (!look.Draws())
				return;
			// Without a border the fill is the whole shape, and without a fill the
			// border is drawn alone.
			bool bordered = look.border != NoColor;
			const Raster& raster = Cached(icon, size, bordered ? look.borderWidth : 0);
			if (look.fill != NoColor)
				DrawRects(raster.fill, x, y, look.fill, scale);
			if (bordered)
				DrawRects(raster.border, x, y, look.border, scale);
		}

		// A cell is drawn from the bottom left of its frame. The super chest's
		// frame is 16 by 32 with what shows of it in columns 2 to 15 and rows 7 to
		// 29 (MaxiMap.dc6, frame 318), so it is placed with that centred on the
		// point, leaning a pixel left as the shapes do.
		const int ChestCentreX = 9;
		const int ChestCentreY = 14;
	}

	void Draw(Icon icon, unsigned int size, int x, int y, const Look& look) {
		if (icon == Chest)
			DrawCell(ChestCell, x - ChestCentreX, y + ChestCentreY);
		else
			DrawShape(icon, size, x, y, look, 1);
	}

	void DrawMagnified(Icon icon, unsigned int size, unsigned int scale, int x, int y,
			const Look& look) {
		if (IsShape(icon))
			DrawShape(icon, size, x, y, look, scale < 1 ? 1 : (int)scale);
	}

	void DrawAt(Icon icon, unsigned int size, int worldX, int worldY,
			const Look& look) {
		if (icon == Chest) {
			// The cell's position as Maphack::RevealRoom has always worked it out
			// for the cells it adds, then placed as the automap places every cell
			// (D2Client 0x602A6): scaled by the divisor and moved by the offset.
			// Both pixel coordinates are words in the cell, read back signed.
			short xPixel = (short)((((worldX - worldY) * 16) / 10) + 1);
			short yPixel = (short)((((worldY + worldX) * 8) / 10) - 3);
			int divisor = *(int*)p_D2CLIENT_Divisor;
			if (divisor <= 0)
				return;
			int x = (xPixel * 10) / divisor - (*p_D2CLIENT_Offset).x;
			int y = (yPixel * 10) / divisor - (*p_D2CLIENT_Offset).y;
			DrawCell(ChestCell, x, y);
			return;
		}
		POINT point;
		Drawing::Hook::ScreenToAutomap(&point, worldX, worldY);
		DrawShape(icon, size, point.x, point.y, look, 1);
	}

	// Kept as the rasters are, for the monster X drawn for every monster.
	void DrawOutline(Icon icon, unsigned int size, int x, int y, unsigned int color) {
		static std::map<std::pair<int, unsigned int>, std::vector<Line>> cache;
		std::pair<int, unsigned int> key((int)icon, size);
		auto found = cache.find(key);
		if (found == cache.end())
			found = cache.emplace(key, Outline(icon, size)).first;
		for (const Line& line : found->second)
			D2GFX_DrawLine(x + line.x1, y + line.y1, x + line.x2, y + line.y2, color, -1);
	}
}
