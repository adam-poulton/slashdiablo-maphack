#include "RunewordTab.h"
#include "../../BH.h"
#include "../../Catalogue/Catalogues.h"
#include "../../Catalogue/RunewordCatalogue.h"
#include "../../Common.h"
#include "../../ItemDescription.h"
#include "../../ItemRarity.h"
#include "../../StringUtil.h"

using namespace Drawing;

// Margins and the gaps between the three bands. Widths and the list height are
// measured from the tab by ApplyLayout().

// The name column is fixed at the longest runeword name and the runes take the
// rest, since "Jah + Mal + Jah + Sur + Jah + Ber" is what needs the room.
#define RW_COL_NAME_W		136
#define RW_COL_GAP			4

RunewordTab::RunewordTab(UI* ui) : UIPanel("Runewords", ui),
	shownSummary(-1),
	catalogueLoaded(false),
	needsRefresh(true) {

	list = new Listhook(tab, UI_CONTENT_MARGIN, 0, 0, 0);
	// The name as the item it makes, the runes in the colour a rune is given.
	std::vector<ListColumn> columns;
	columns.push_back(ListColumn("", RW_COL_NAME_W, 0, 0,
		RarityColor(RarityRuneword), White));
	columns.push_back(ListColumn("", 0, 1, RW_COL_GAP, RarityColor(RarityRune)));
	list->SetColumns(columns);

	// Placed and switched on by UpdateSummary().
	summary = new Tooltiphook(InGame, 0, 0);
	summary->SetActive(false);

	ApplyLayout();
}

// The list takes whatever height the window leaves it, so a resize needs
// nothing but this.
void RunewordTab::ApplyLayout() {
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

std::vector<ChatCommand> RunewordTab::GetCommands() {
	return { { "rw", { "runewords" }, "<search>", "Opens the Runewords tab" } };
}

// One text criterion, scoped to the runewords. An empty search is carried by
// every source, which is what shows the whole list.
void RunewordTab::RunQuery() {
	StatIndex::Query query;
	query.kind = RunewordCatalogue::Kind;
	query.criteria.push_back(StatIndex::Criterion::OnText(search));
	results = StatIndex::Find(query);
}

void RunewordTab::PushRows() {
	std::vector<std::vector<std::string>> rows;
	rows.reserve(results.size());
	for (unsigned int i = 0; i < results.size(); i++) {
		const Catalogue::Source& source = *results[i].entry->source;
		std::vector<std::string> row;
		row.push_back(source.name);
		row.push_back(source.ingredients);
		row.push_back(source.itemType);
		rows.push_back(row);
	}
	list->SetRows(rows);	// also clears the selection
	shownSummary = -1;
}

// ItemDescription orders and spaces the panel the way the game describes a
// recipe; the tab only says what goes in it.
std::vector<TooltipLine> RunewordTab::BuildSummaryLines(
		const Catalogue::Source& source) {
	ItemDescription::Recipe runeword;
	runeword.name = source.name;
	runeword.nameColor = RarityColor(source.rarity);
	runeword.appliesTo = source.itemType;
	runeword.ingredients = source.ingredients;
	runeword.ingredientColor = RarityColor(RarityRune);
	// A runeword goes in any of a range of bases, so it carries no strength or
	// dexterity of its own.
	runeword.requirements.level = source.requiredLevel;
	runeword.AddStats(source.lines, Blue);
	return ItemDescription::Build(runeword);
}

// Follows the mouse, falling back to the selection. Rebuilt only when the row
// changes, since the mouse sits on one row for many frames.
void RunewordTab::UpdateSummary() {
	int row = list->GetHoveredRow();
	if (row < 0)
		row = list->GetSelectedRow();

	if (!IsActive() || row < 0 || row >= (int)results.size()) {
		summary->SetActive(false);
		shownSummary = -1;
		return;
	}

	if (row != shownSummary) {
		summary->SetLines(BuildSummaryLines(*results[row].entry->source));
		shownSummary = row;
	}

	// Must follow SetLines(): where it fits depends on how big it turned out.
	summary->PlaceBeside(tab->GetX(), tab->GetY(), tab->GetXSize(), tab->GetYSize());
	summary->SetActive(true);
}

void RunewordTab::Search(const std::string& text) {
	search = ToLower(Trim(text));
	list->SetScrollTop(0);
	needsRefresh = true;
}

void RunewordTab::OnClose() {
	summary->SetActive(false);
	shownSummary = -1;
	Search("");
}

// The hint the window's search box shows while this panel is in front.
std::string RunewordTab::GetSearchPlaceholder() {
	return "Search by runeword name, rune or item type";
}

// Follows the scroll position as well as the rows, so it is read per frame.
std::string RunewordTab::GetStatus() {
	if (!catalogueLoaded)
		return "Waiting for game data to finish loading...";
	if (results.empty())
		return "No runewords match \"" + search + "\"";

	char line[64];
	if (list->GetMaxScrollTop() > 0) {
		sprintf_s(line, sizeof(line), "%u - %u of %u runewords",
			list->GetFirstVisibleRow() + 1, list->GetLastVisibleRow(),
			(unsigned int)results.size());
	} else {
		sprintf_s(line, sizeof(line), "%u runewords", (unsigned int)results.size());
	}
	return line;
}

// Enter picks the first match rather than typing a newline.
void RunewordTab::OnSearchSubmitted() {
	if (!results.empty())
		list->SetSelectedRow(0);
}

void RunewordTab::OnDraw() {
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

bool RunewordTab::OnKey(bool up, BYTE key) {
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
