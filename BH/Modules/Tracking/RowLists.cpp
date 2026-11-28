#include "RowLists.h"

using namespace Drawing;

namespace {
	bool Same(const std::vector<ListRows::Row>& a, const std::vector<ListRows::Row>& b) {
		if (a.size() != b.size())
			return false;
		for (size_t i = 0; i < a.size(); i++) {
			if (a[i].heading != b[i].heading || a[i].summary != b[i].summary ||
					a[i].cells != b[i].cells || a[i].colors != b[i].colors ||
					a[i].key != b[i].key)
				return false;
		}
		return true;
	}

	std::vector<ListRow> Listed(const std::vector<ListRows::Row>& rows) {
		std::vector<ListRow> listed;
		listed.reserve(rows.size());
		for (const ListRows::Row& row : rows) {
			ListRow one(row.cells, row.heading);
			if (row.summary)
				one.colors.assign(row.cells.size(), Grey);
			// Disabled is the list's word for a cell that takes its column's.
			for (int color : row.colors)
				one.colors.push_back(color < 0 ? Disabled : (TextColor)color);
			listed.push_back(one);
		}
		return listed;
	}
}

bool RowLists::Push(Listhook* list, std::vector<ListRows::Row>& pushed,
		const std::vector<ListRows::Row>& rows) {
	if (Same(rows, pushed))
		return false;
	list->SetRows(Listed(rows));
	pushed = rows;
	return true;
}
