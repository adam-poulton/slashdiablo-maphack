#include "MapIcon.h"
#include <string.h>
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

		void DrawShape(Icon icon, unsigned int size, int x, int y, unsigned int color) {
			Shape shape = Geometry(icon, size);
			for (const Fill& fill : shape.fills)
				D2GFX_DrawRectangle(x + fill.x, y + fill.y, x + fill.x + fill.width,
					y + fill.y + fill.height, color, Drawing::BTHighlight);
			for (const Line& line : shape.lines)
				D2GFX_DrawLine(x + line.x1, y + line.y1, x + line.x2, y + line.y2,
					color, -1);
		}

		// A cell is drawn from the bottom left of its frame. The super chest's
		// frame is 16 by 32 with what shows of it in columns 2 to 15 and rows 7 to
		// 29 (MaxiMap.dc6, frame 318), so it is placed with that centred on the
		// point, leaning a pixel left as the shapes do.
		const int GameCentreX = 9;
		const int GameCentreY = 14;
	}

	void Draw(Icon icon, unsigned int size, int x, int y, unsigned int color) {
		if (icon == Game)
			DrawCell(GameCell, x - GameCentreX, y + GameCentreY);
		else
			DrawShape(icon, size, x, y, color);
	}

	void DrawAt(Icon icon, unsigned int size, int worldX, int worldY,
			unsigned int color) {
		if (icon == Game) {
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
			DrawCell(GameCell, x, y);
			return;
		}
		POINT point;
		Drawing::Hook::ScreenToAutomap(&point, worldX, worldY);
		DrawShape(icon, size, point.x, point.y, color);
	}
}
