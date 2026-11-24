#include "doctest.h"
#include "Modules/ItemMover/ItemGrid.h"

#include <climits>
#include <vector>

/*
 * The cells the item mover reads and writes.
 *
 * Item positions and sizes are read from the game while items are moving, so
 * they can be anything. What is checked here is that no position or size leads
 * outside a container's cells, that a mouse outside a grid is over no cell, and
 * that well-formed items are found and placed as before.
 */

using ItemGrid::CellAt;
using ItemGrid::Covers;
using ItemGrid::Fits;
using ItemGrid::Grid;
using ItemGrid::IsVacant;
using ItemGrid::Mark;

// The 1.13c inventory at 800x600: ten by four cells of 29 pixels.
static Grid Inventory() {
	Grid grid = {};
	grid.columns = 10;
	grid.rows = 4;
	grid.left = 419;
	grid.top = 315;
	grid.right = 419 + 10 * 29 - 1;
	grid.bottom = 315 + 4 * 29 - 1;
	grid.cellSize = 29;
	return grid;
}

// A container's cells with a guard cell either side, so a write outside the
// container lands somewhere a test can see.
struct Guarded {
	int columns, rows;
	std::vector<int> storage;

	Guarded(int columns, int rows) : columns(columns), rows(rows), storage(columns * rows + 2, 0) {}

	int* Cells() { return storage.data() + 1; }
	int At(int column, int row) { return Cells()[row * columns + column]; }
	bool GuardsUntouched() { return storage.front() == 0 && storage.back() == 0; }

	int Count(int id) {
		int count = 0;
		for (int i = 0; i < columns * rows; i++) {
			count += Cells()[i] == id;
		}
		return count;
	}
};

TEST_CASE("a point inside the grid is over the cell it is drawn in") {
	Grid grid = Inventory();
	int column = -1, row = -1;

	REQUIRE(CellAt(grid, grid.left, grid.top, &column, &row));
	CHECK(column == 0);
	CHECK(row == 0);

	REQUIRE(CellAt(grid, grid.left + 29 * 3 + 5, grid.top + 29 * 2 + 28, &column, &row));
	CHECK(column == 3);
	CHECK(row == 2);

	REQUIRE(CellAt(grid, grid.right, grid.bottom, &column, &row));
	CHECK(column == 9);
	CHECK(row == 3);
}

TEST_CASE("a point left of or above the grid is over no cell") {
	// Dividing the few pixels by the cell size would give column or row 0.
	Grid grid = Inventory();
	int column = -1, row = -1;

	CHECK_FALSE(CellAt(grid, grid.left - 1, grid.top + 10, &column, &row));
	CHECK_FALSE(CellAt(grid, grid.left - 28, grid.top + 10, &column, &row));
	CHECK_FALSE(CellAt(grid, grid.left + 10, grid.top - 1, &column, &row));
	CHECK_FALSE(CellAt(grid, 0, 0, &column, &row));
	CHECK(column == -1);
	CHECK(row == -1);
}

TEST_CASE("a point right of or below the grid is over no cell") {
	Grid grid = Inventory();
	int column, row;

	CHECK_FALSE(CellAt(grid, grid.right + 1, grid.top + 10, &column, &row));
	CHECK_FALSE(CellAt(grid, grid.left + 10, grid.bottom + 1, &column, &row));
	CHECK_FALSE(CellAt(grid, INT_MAX, INT_MAX, &column, &row));
}

TEST_CASE("a screen rectangle wider than its cells gives no cell past the last") {
	Grid grid = Inventory();
	grid.right += 29;
	grid.bottom += 29;
	int column, row;

	CHECK_FALSE(CellAt(grid, grid.right, grid.top, &column, &row));
	CHECK_FALSE(CellAt(grid, grid.left, grid.bottom, &column, &row));
}

TEST_CASE("a grid not yet laid out is over no cell") {
	Grid grid = Inventory();
	grid.cellSize = 0;
	int column, row;

	CHECK_FALSE(CellAt(grid, grid.left + 10, grid.top + 10, &column, &row));
	CHECK_FALSE(CellAt(Grid(), 0, 0, &column, &row));
}

TEST_CASE("an item fits when every cell it covers is in the grid") {
	CHECK(Fits(10, 4, 0, 0, 1, 1));
	CHECK(Fits(10, 4, 8, 0, 2, 4));
	CHECK(Fits(10, 4, 0, 0, 10, 4));
}

TEST_CASE("an item that overhangs the grid does not fit") {
	CHECK_FALSE(Fits(10, 4, 9, 0, 2, 1));
	CHECK_FALSE(Fits(10, 4, 0, 1, 1, 4));
	CHECK_FALSE(Fits(10, 4, -1, 0, 2, 1));
	CHECK_FALSE(Fits(10, 4, 0, -1, 1, 2));
	CHECK_FALSE(Fits(10, 4, INT_MAX, 0, 2, 1));
	CHECK_FALSE(Fits(10, 4, 0, INT_MIN, 1, 1));
}

TEST_CASE("an item with no cells fits nowhere") {
	CHECK_FALSE(Fits(10, 4, 0, 0, 0, 1));
	CHECK_FALSE(Fits(10, 4, 0, 0, 1, 0));
}

TEST_CASE("an item covers exactly the cells from its position across its size") {
	CHECK(Covers(2, 1, 2, 3, 2, 1));
	CHECK(Covers(2, 1, 2, 3, 3, 3));
	CHECK_FALSE(Covers(2, 1, 2, 3, 1, 1));
	CHECK_FALSE(Covers(2, 1, 2, 3, 4, 1));
	CHECK_FALSE(Covers(2, 1, 2, 3, 2, 4));
	CHECK_FALSE(Covers(2, 1, 0, 0, 2, 1));
}

TEST_CASE("covering does not overflow for positions far from the grid") {
	CHECK_FALSE(Covers(INT_MIN, 0, 2, 1, INT_MAX, 0));
	CHECK_FALSE(Covers(INT_MAX, 0, 2, 1, INT_MIN, 0));
	CHECK_FALSE(Covers(INT_MAX, 0, 255, 255, 0, 0));
}

TEST_CASE("marking an item inside the grid fills its cells") {
	Guarded grid(10, 4);
	Mark(grid.Cells(), 10, 4, 3, 1, 2, 3, 7);

	CHECK(grid.Count(7) == 6);
	CHECK(grid.At(3, 1) == 7);
	CHECK(grid.At(4, 3) == 7);
	CHECK(grid.At(5, 1) == 0);
	CHECK(grid.GuardsUntouched());
}

TEST_CASE("marking an item that overhangs the grid fills only the cells inside") {
	Guarded right(10, 4);
	Mark(right.Cells(), 10, 4, 9, 3, 2, 4, 7);
	CHECK(right.Count(7) == 1);
	CHECK(right.At(9, 3) == 7);
	CHECK(right.GuardsUntouched());

	Guarded left(10, 4);
	Mark(left.Cells(), 10, 4, -1, -1, 2, 2, 7);
	CHECK(left.Count(7) == 1);
	CHECK(left.At(0, 0) == 7);
	CHECK(left.GuardsUntouched());
}

TEST_CASE("marking an item nowhere near the grid fills nothing") {
	Guarded grid(10, 4);
	Mark(grid.Cells(), 10, 4, 10, 0, 1, 1, 7);
	Mark(grid.Cells(), 10, 4, 0, 4, 1, 1, 7);
	Mark(grid.Cells(), 10, 4, INT_MAX, INT_MAX, 255, 255, 7);
	Mark(grid.Cells(), 10, 4, INT_MIN, INT_MIN, 255, 255, 7);
	Mark(grid.Cells(), 10, 4, 0, 0, 0, 0, 7);

	CHECK(grid.Count(7) == 0);
	CHECK(grid.GuardsUntouched());
}

TEST_CASE("a spot is vacant when the item fits there over empty cells") {
	Guarded grid(10, 4);
	Mark(grid.Cells(), 10, 4, 0, 0, 2, 2, 7);

	CHECK(IsVacant(grid.Cells(), 10, 4, 2, 0, 2, 2));
	CHECK(IsVacant(grid.Cells(), 10, 4, 0, 2, 2, 2));
	CHECK_FALSE(IsVacant(grid.Cells(), 10, 4, 1, 1, 2, 2));
	CHECK_FALSE(IsVacant(grid.Cells(), 10, 4, 0, 0, 1, 1));
}

TEST_CASE("a spot the item would overhang is never vacant") {
	Guarded grid(10, 4);

	CHECK_FALSE(IsVacant(grid.Cells(), 10, 4, 9, 0, 2, 1));
	CHECK_FALSE(IsVacant(grid.Cells(), 10, 4, 0, 3, 1, 2));
	CHECK_FALSE(IsVacant(grid.Cells(), 10, 4, -1, 0, 1, 1));
	CHECK_FALSE(IsVacant(grid.Cells(), 10, 4, 0, -1, 1, 1));
}

TEST_CASE("an item with no cells has no vacant spot") {
	Guarded grid(10, 4);

	CHECK_FALSE(IsVacant(grid.Cells(), 10, 4, 0, 0, 0, 0));
	CHECK_FALSE(IsVacant(grid.Cells(), 10, 4, 0, 0, 1, 0));
}
