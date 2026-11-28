#include "HudBlock.h"
#include <algorithm>

namespace HudBlock {

namespace {
	bool IsLeft(const Cell& cell) {
		return cell.tone == ToneLabel || cell.left;
	}

	int Along(Side side, int offset, int canvas, int block) {
		int at = offset;
		if (side == SideMiddle)
			at = (canvas - block) / 2 + offset;
		else if (side == SideFar)
			at = canvas - block - offset;
		return std::max(0, std::min(at, canvas - block));
	}

	void PinAlong(int at, int block, int canvas, Side& side, int& offset) {
		const int middle = at + block / 2;
		if (middle * 3 < canvas) {
			side = SideNear;
			offset = at;
		} else if (middle * 3 > canvas * 2) {
			side = SideFar;
			offset = canvas - block - at;
		} else {
			side = SideMiddle;
			offset = at - (canvas - block) / 2;
		}
	}

	// Sets one section's rows out from a corner, and says how far right they
	// came. `y` is left under the last of them.
	int ArrangeFrom(const Section& rows, const Metrics& metrics, int& y, std::vector<Placed>& cells) {
		std::vector<int> columns;
		std::vector<std::vector<int>> widths;
		for (const Row& row : rows) {
			widths.push_back(std::vector<int>());
			for (size_t c = 0; c < row.size(); c++) {
				const int width = metrics.width(row[c].text);
				widths.back().push_back(width);
				if (c >= columns.size())
					columns.push_back(0);
				columns[c] = std::max(columns[c], width);
			}
		}

		int right = metrics.padding;
		for (size_t r = 0; r < rows.size(); r++) {
			int x = metrics.padding;
			for (size_t c = 0; c < rows[r].size(); c++) {
				const Cell& cell = rows[r][c];
				// A gap is what the cell before it calls for: a name keeps its
				// figure close, and a figure stands off from whatever comes next.
				if (c > 0)
					x += IsLeft(rows[r][c - 1]) ? metrics.labelGap : metrics.groupGap;
				const int inset = IsLeft(cell) ? 0 : columns[c] - widths[r][c];
				cells.push_back({ { x + inset, y }, cell.text, cell.tone });
				x += columns[c];
			}
			right = std::max(right, x);
			y += metrics.textHeight;
			if (r + 1 < rows.size())
				y += metrics.rowGap;
		}
		return right;
	}
}

Cell Label(const std::string& text) {
	return { text, ToneLabel };
}

Cell Value(const std::string& text) {
	return { text, ToneValue };
}

Cell Dim(const std::string& text) {
	return { text, ToneDim };
}

Block Arrange(const std::vector<Row>& rows, const Metrics& metrics) {
	return ArrangeSections({ rows }, metrics);
}

Block ArrangeSections(const std::vector<Section>& sections, const Metrics& metrics) {
	Block block;
	int right = metrics.padding;
	int y = metrics.padding;
	bool any = false;
	for (const Section& section : sections) {
		if (section.empty())
			continue;
		if (any)
			y += metrics.rowGap + metrics.sectionGap;
		right = std::max(right, ArrangeFrom(section, metrics, y, block.cells));
		any = true;
	}
	block.size = { right + metrics.padding, y + metrics.padding };
	return block;
}

Point Resolve(const Place& place, Size canvas, Size block) {
	return { Along(place.across, place.x, canvas.width, block.width),
		Along(place.down, place.y, canvas.height, block.height) };
}

Place PlaceOf(Point topLeft, Size block, Size canvas) {
	Place place;
	PinAlong(topLeft.x, block.width, canvas.width, place.across, place.x);
	PinAlong(topLeft.y, block.height, canvas.height, place.down, place.y);
	return place;
}

bool UnderPanel(Point topLeft, Size block, Size canvas, bool leftOpen, bool rightOpen) {
	const int middle = canvas.width / 2;
	return (leftOpen && topLeft.x < middle) ||
		(rightOpen && topLeft.x + block.width > middle);
}

}
