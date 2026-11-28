#pragma once
#include <vector>
#include "../../Drawing.h"
#include "ListRows.h"

namespace RowLists {
	// Gives a list its rows where they are not the ones it was last given, which
	// are kept in pushed. Giving a list rows refits every cell and lets go of
	// what is selected in it, so it is done only for a change. True where it was.
	bool Push(Drawing::Listhook* list, std::vector<ListRows::Row>& pushed,
		const std::vector<ListRows::Row>& rows);
}
