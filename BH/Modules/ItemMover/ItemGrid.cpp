#include "ItemGrid.h"

namespace ItemGrid {

bool CellAt(const Grid& grid, int x, int y, int* column, int* row) {
	if (grid.cellSize <= 0 || x < grid.left || x > grid.right || y < grid.top || y > grid.bottom) {
		return false;
	}
	int cellColumn = (x - grid.left) / grid.cellSize;
	int cellRow = (y - grid.top) / grid.cellSize;
	if (cellColumn >= grid.columns || cellRow >= grid.rows) {
		return false;
	}
	*column = cellColumn;
	*row = cellRow;
	return true;
}

bool Fits(int columns, int rows, int column, int row, int width, int height) {
	return width > 0 && height > 0 && column >= 0 && row >= 0 &&
		width <= columns - column && height <= rows - row;
}

// In 64 bits so that a position read from a moving item cannot overflow.
bool Covers(int column, int row, int width, int height, int cellColumn, int cellRow) {
	long long dx = (long long)cellColumn - column;
	long long dy = (long long)cellRow - row;
	return dx >= 0 && dx < width && dy >= 0 && dy < height;
}

void Mark(int* cells, int columns, int rows, int column, int row, int width, int height, int id) {
	for (int x = 0; x < columns; x++) {
		for (int y = 0; y < rows; y++) {
			if (Covers(column, row, width, height, x, y)) {
				cells[y * columns + x] = id;
			}
		}
	}
}

bool IsVacant(const int* cells, int columns, int rows, int column, int row, int width, int height) {
	if (!Fits(columns, rows, column, row, width, height)) {
		return false;
	}
	for (int x = column; x < column + width; x++) {
		for (int y = row; y < row + height; y++) {
			if (cells[y * columns + x]) {
				return false;
			}
		}
	}
	return true;
}

}
