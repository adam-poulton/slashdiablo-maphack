#include "UniqueTab.h"
#include "../../BH.h"
#include "../../Catalogue/Catalogues.h"
#include "../../Catalogue/UniqueCatalogue.h"
#include "../../Common.h"
#include "../../ItemDescription.h"
#include "../../ItemRarity.h"
#include "../../StringUtil.h"

using namespace Drawing;

// The unique's own name takes the larger share, being the longer of the two
// ("Bul-Kathos' Tribal Guardian" against "Ceremonial Javelin").
#define UQ_COL_NAME_WEIGHT	3
#define UQ_COL_BASE_WEIGHT	2
#define UQ_COL_GAP			4

UniqueTab::UniqueTab(UI* ui) : UIPanel("Uniques", ui),
	shownSummary(-1),
	catalogueLoaded(false),
	needsRefresh(true) {

	list = new Listhook(tab, UI_CONTENT_MARGIN, 0, 0, 0);
	// Both columns name the same item, so they share its colour.
	TextColor unique = RarityColor(RarityUnique);
	std::vector<ListColumn> columns;
	columns.push_back(ListColumn("", 0, UQ_COL_NAME_WEIGHT, 0, unique, White));
	columns.push_back(ListColumn("", 0, UQ_COL_BASE_WEIGHT, UQ_COL_GAP, unique, White));
	list->SetColumns(columns);

	// Placed and switched on by UpdateSummary().
	summary = new Tooltiphook(InGame, 0, 0);
	summary->SetActive(false);

	ApplyLayout();
}

// The list takes whatever height the window leaves it, so a resize needs
// nothing but this.
void UniqueTab::ApplyLayout() {
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

std::vector<ChatCommand> UniqueTab::GetCommands() {
	return { { "uni", { "uniques" }, "<search>", "Opens the Uniques tab" } };
}

// One text criterion, scoped to the uniques. An empty search is carried by
// every source, which is what shows the whole list.
void UniqueTab::RunQuery() {
	StatIndex::Query query;
	query.kind = UniqueCatalogue::Kind;
	query.criteria.push_back(StatIndex::Criterion::OnText(search));
	results = StatIndex::Find(query);
}

void UniqueTab::PushRows() {
	std::vector<std::vector<std::string>> rows;
	rows.reserve(results.size());
	for (unsigned int i = 0; i < results.size(); i++) {
		const Catalogue::Source& source = *results[i].entry->source;
		std::vector<std::string> row;
		row.push_back(source.name);
		row.push_back(source.baseName);
		rows.push_back(row);
	}
	list->SetRows(rows);	// also clears the selection
	shownSummary = -1;
}

// ItemDescription orders and spaces the panel the way the game describes an
// item; the tab only says what goes in it.
std::vector<TooltipLine> UniqueTab::BuildSummaryLines(const StatIndex::Entry& entry) {
	const Catalogue::Source& source = *entry.source;
	TextColor color = RarityColor(source.rarity);

	ItemDescription::Description item;
	item.AddTitle(source.name, color);
	// What the source's properties do to the numbers its base carries. The
	// index already holds the totals they add up to.
	item.AddBase(source.baseCode, color, ItemDescription::ReadModifiers(entry.totals));

	// A unique can ask for a higher level than the base it is made on does.
	if (source.requiredLevel > item.requirements.level)
		item.requirements.level = source.requiredLevel;

	item.AddStats(source.lines, Blue);
	return ItemDescription::Build(item);
}

// Follows the mouse, falling back to the selection. Rebuilt only when the row
// changes, since the mouse sits on one row for many frames.
void UniqueTab::UpdateSummary() {
	int row = list->GetHoveredRow();
	if (row < 0)
		row = list->GetSelectedRow();

	if (!IsActive() || row < 0 || row >= (int)results.size()) {
		summary->SetActive(false);
		shownSummary = -1;
		return;
	}

	if (row != shownSummary) {
		summary->SetLines(BuildSummaryLines(*results[row].entry));
		shownSummary = row;
	}

	// Must follow SetLines(): where it fits depends on how big it turned out.
	summary->PlaceBeside(tab->GetX(), tab->GetY(), tab->GetXSize(), tab->GetYSize());
	summary->SetActive(true);
}

void UniqueTab::Search(const std::string& text) {
	search = ToLower(Trim(text));
	list->SetScrollTop(0);
	needsRefresh = true;
}

void UniqueTab::OnClose() {
	summary->SetActive(false);
	shownSummary = -1;
	Search("");
}

// The hint the window's search box shows while this panel is in front.
std::string UniqueTab::GetSearchPlaceholder() {
	return "Search by unique name, base item or item type";
}

// Follows the scroll position as well as the rows, so it is read per frame.
std::string UniqueTab::GetStatus() {
	if (!catalogueLoaded)
		return "Waiting for game data to finish loading...";
	if (results.empty())
		return "No uniques match \"" + search + "\"";

	char line[64];
	if (list->GetMaxScrollTop() > 0) {
		sprintf_s(line, sizeof(line), "%u - %u of %u uniques",
			list->GetFirstVisibleRow() + 1, list->GetLastVisibleRow(),
			(unsigned int)results.size());
	} else {
		sprintf_s(line, sizeof(line), "%u uniques", (unsigned int)results.size());
	}
	return line;
}

// Enter picks the first match rather than typing a newline.
void UniqueTab::OnSearchSubmitted() {
	if (!results.empty())
		list->SetSelectedRow(0);
}

void UniqueTab::OnDraw() {
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
		PushRows();
		needsRefresh = false;
	}

	// The mouse and the scroll position move on the input thread, so catch up here.
	UpdateSummary();
}

bool UniqueTab::OnKey(bool up, BYTE key) {
	switch (key) {
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
