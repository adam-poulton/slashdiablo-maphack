#pragma once

// The cells of an item container (inventory, stash or cube), and the items in
// them.
//
// Items are read from the game while they move, so a position or size can be
// anything: every function here stays inside the grid whatever it is given.
namespace ItemGrid {

// A container's cells, columns by rows, and the screen rectangle they are drawn
// in. right and bottom are the last pixel inside, as the game's layouts give
// them.
struct Grid {
	int columns, rows;
	int left, top, right, bottom;
	int cellSize;
};

// The cell under a screen point. False when the point is outside the grid, or
// the grid has no cells.
bool CellAt(const Grid& grid, int x, int y, int* column, int* row);

// Whether an item of width by height cells at column, row lies wholly inside a
// grid of columns by rows. An item with no cells fits nowhere.
bool Fits(int columns, int rows, int column, int row, int width, int height);

// Whether an item at column, row of width by height cells covers a cell.
bool Covers(int column, int row, int width, int height, int cellColumn, int cellRow);

// Writes id into every cell of a columns by rows grid that an item at column, row
// of width by height cells covers. Cells of the item outside the grid are skipped.
void Mark(int* cells, int columns, int rows, int column, int row, int width, int height, int id);

// Whether an item of width by height cells at column, row fits in the grid with
// no other item under it.
bool IsVacant(const int* cells, int columns, int rows, int column, int row, int width, int height);

}
