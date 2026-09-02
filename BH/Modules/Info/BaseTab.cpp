#include "BaseTab.h"
#include "../../BH.h"
#include "../../Catalogue/BaseItemCatalogue.h"
#include "../../Catalogue/Catalogues.h"
#include "../../Common.h"
#include "../../ItemDescription.h"
#include "../../StringUtil.h"

using namespace Drawing;

// The name takes the larger share; the tier beside it is one short word.
#define BT_COL_NAME_WEIGHT	3
#define BT_COL_TIER_WEIGHT	1
#define BT_COL_GAP			4

BaseTab::BaseTab(UI* ui) : UIPanel("Bases", ui),
	shownTypes(0),
	foldOnPush(true),
	shownSummary(-1),
	catalogueLoaded(false),
	needsRefresh(true) {

	list = new Listhook(tab, UI_CONTENT_MARGIN, 0, 0, 0);
	std::vector<ListColumn> columns;
	columns.push_back(ListColumn("", 0, BT_COL_NAME_WEIGHT, 0, White, White));
	columns.push_back(ListColumn("", 0, BT_COL_TIER_WEIGHT, BT_COL_GAP, Grey, White));
	list->SetColumns(columns);
	list->SetGroupColor(Gold);

	// Placed and switched on by UpdateSummary().
	summary = new Tooltiphook(InGame, 0, 0);
	summary->SetActive(false);

	ApplyLayout();
}

// The list takes whatever height the window leaves it, so a resize needs
// nothing but this.
void BaseTab::ApplyLayout() {
	laidOutWidth = tab->GetXSize();
	laidOutHeight = tab->GetYSize();

	unsigned int contentWidth = (laidOutWidth > 2 * UI_CONTENT_MARGIN) ?
		(laidOutWidth - (2 * UI_CONTENT_MARGIN)) : 0;

	// The window has already taken its search box and its footer out of the
	// height, so the list has all of what is left.
	list->SetBaseY(0);
	list->SetSize(contentWidth, laidOutHeight);
	summary->SetMaxWidth(contentWidth);
}

std::vector<ChatCommand> BaseTab::GetCommands() {
	return { { "base", { "bases" }, "<search>", "Opens the Bases tab" } };
}

// One text criterion, scoped to the bases. An empty search is carried by every
// source, which is what shows the whole list.
void BaseTab::RunQuery() {
	StatIndex::Query query;
	query.kind = BaseItemCatalogue::Kind;
	query.criteria.push_back(StatIndex::Criterion::OnText(search));
	results = StatIndex::Find(query);
}

// Headings are driven off the rows rather than the data, so a search that cuts a
// type's first base still names the type above whichever is now first.
void BaseTab::PushRows() {
	unsigned int mostRows = (unsigned int)(results.size() * 2);
	std::vector<ListRow> rows;
	rows.reserve(mostRows);
	rowBases.clear();
	rowBases.reserve(mostRows);
	shownTypes = 0;

	for (unsigned int i = 0; i < results.size(); i++) {
		const Catalogue::Source& source = *results[i].entry->source;
		if (i == 0 || source.itemType != results[i - 1].entry->source->itemType) {
			rows.push_back(ListRow({ source.itemType }, true));
			rowBases.push_back(NULL);
			shownTypes++;
		}

		rows.push_back(ListRow({ source.name,
			ItemDescription::TierName(source.tier) }));
		rowBases.push_back(&source);
	}

	list->SetRows(rows);	// also clears the selection

	// Not on open, which is usually before the game data has loaded, and not on a
	// filtered list, which would leave every type it did not match unfolded.
	if (foldOnPush && search.empty() && !rows.empty()) {
		list->FoldAllGroups();
		foldOnPush = false;
	}
	shownSummary = -1;
}

// ItemDescription orders and spaces the panel the way the game describes an
// item; the tab only says what goes in it.
std::vector<TooltipLine> BaseTab::BuildSummaryLines(const Catalogue::Source& source) {
	ItemDescription::Description base;
	base.AddBase(source.baseCode, White);
	base.AddTitle(ItemDescription::TierName(source.tier) + " " + source.itemType, Grey);
	base.AddBaseLimits(source.baseCode);
	return ItemDescription::Build(base);
}

// Follows the mouse, falling back to the selection. Rebuilt only when the row
// changes, since the mouse sits on one row for many frames.
void BaseTab::UpdateSummary() {
	int row = list->GetHoveredRow();
	if (row < 0)
		row = list->GetSelectedRow();

	// A heading describes nothing: it is only the name of the type below it.
	bool describable = IsActive() && row >= 0 && row < (int)rowBases.size() &&
		rowBases[row] != NULL;
	if (!describable) {
		summary->SetActive(false);
		shownSummary = -1;
		return;
	}

	if (row != shownSummary) {
		summary->SetLines(BuildSummaryLines(*rowBases[row]));
		shownSummary = row;
	}

	// Must follow SetLines(): where it fits depends on how big it turned out.
	summary->PlaceBeside(tab->GetX(), tab->GetY(), tab->GetXSize(), tab->GetYSize());
	summary->SetActive(true);
}

void BaseTab::Search(const std::string& text) {
	search = ToLower(Trim(text));
	list->SetScrollTop(0);
	needsRefresh = true;
}

void BaseTab::OnClose() {
	summary->SetActive(false);
	shownSummary = -1;
	foldOnPush = true;
	Search("");
}

// The hint the window's search box shows while this panel is in front.
std::string BaseTab::GetSearchPlaceholder() {
	return "Search by base item name, item type or tier";
}

// Counted in items and types rather than rows: the list interleaves headings
// with items, so a range of row numbers would be one the user cannot check.
std::string BaseTab::GetStatus() {
	if (!catalogueLoaded)
		return "Waiting for game data to finish loading...";
	if (results.empty())
		return "No base items match \"" + search + "\"";

	char line[64];
	sprintf_s(line, sizeof(line), "%u base items in %u type%s",
		(unsigned int)results.size(), shownTypes, (shownTypes == 1) ? "" : "s");
	return line;
}

// Row 0 is a heading, so enter takes the first row holding a base.
void BaseTab::OnSearchSubmitted() {
	for (unsigned int i = 0; i < rowBases.size(); i++) {
		if (rowBases[i]) {
			list->SetSelectedRow((int)i);
			break;
		}
	}
}

void BaseTab::OnDraw() {
	if (tab->GetXSize() != laidOutWidth || tab->GetYSize() != laidOutHeight)
		ApplyLayout();

	// The catalogues are read on the thread that read the tables, which can
	// finish either before this tab exists or after it has drawn a frame.
	if (!catalogueLoaded && Catalogue::Loaded()) {
		catalogueLoaded = true;
		needsRefresh = true;
	}

	if (needsRefresh) {
		RunQuery();
		// Suspended rather than cleared, so clearing the search restores the
		// user's folds.
		list->SetFoldingSuspended(!search.empty());
		PushRows();
		needsRefresh = false;
	}

	// The mouse and the selection move on the input thread, so catch up here.
	UpdateSummary();
}

bool BaseTab::OnKey(bool up, BYTE key) {
	switch (key) {
		// The selection ends on the heading rather than being let go of, so
		// folding and unfolding are both reachable from wherever the last press
		// left it.
		case VK_LEFT:
		case VK_RIGHT: {
			if (up)
				return true;

			int row = list->GetSelectedRow();
			int group = list->GetGroupRowFor(row);
			if (group < 0)
				return true;

			if (key == VK_LEFT) {
				list->SetSelectedRow(group);
				list->SetGroupFolded(group, true);
			} else if (row != group) {
				// Already inside the type, so there is nowhere further in to go.
			} else if (list->IsGroupFolded(group)) {
				list->SetGroupFolded(group, false);
			} else {
				list->MoveSelection(1);		// open already, so step into it
			}
			return true;
		}

		case VK_UP:
		case VK_DOWN:
		case VK_PRIOR:
		case VK_NEXT:
		case VK_HOME:
		case VK_END: {
			if (up)
				return true;

			// A screenful less one row, so the row being read stays on screen.
			int visible = (int)list->GetVisibleRows();
			int step = (visible > 1) ? (visible - 1) : 1;
			int count = (int)list->GetRowCount();
			switch (key) {
				case VK_UP:		list->MoveSelection(-1); break;
				case VK_DOWN:	list->MoveSelection(1); break;
				case VK_PRIOR:	list->MoveSelection(-step); break;
				case VK_NEXT:	list->MoveSelection(step); break;
				case VK_HOME:	list->SetSelectedRow(0); break;
				case VK_END:	list->SetSelectedRow(count - 1); break;
			}
			return true;
		}
	}
	return false;
}
