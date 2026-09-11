#include "SetTab.h"
#include "../../BH.h"
#include "../../Catalogue/Catalogues.h"
#include "../../Catalogue/SetCatalogue.h"
#include "../../Common.h"
#include "../../ItemDescription.h"
#include "../../ItemRarity.h"
#include "../../StringUtil.h"

using namespace Drawing;

// The piece takes the larger share: its name carries its set's name as well
// ("Tal Rasha's Fine-Spun Cloth" against "Mesh Belt").
#define ST_COL_ITEM_WEIGHT	3
#define ST_COL_BASE_WEIGHT	2
#define ST_COL_GAP			4

SetTab::SetTab(UI* ui) : UIPanel("Sets", ui),
	shownSets(0),
	foldOnPush(true),
	shownSummary(-1),
	catalogueLoaded(false),
	needsRefresh(true) {

	list = new Listhook(tab, UI_CONTENT_MARGIN, 0, 0, 0);
	// Both columns name the same piece, so they share its colour.
	TextColor set = RarityColor(RaritySet);
	std::vector<ListColumn> columns;
	columns.push_back(ListColumn("", 0, ST_COL_ITEM_WEIGHT, 0, set, White));
	columns.push_back(ListColumn("", 0, ST_COL_BASE_WEIGHT, ST_COL_GAP, set, White));
	list->SetColumns(columns);
	list->SetGroupColor(Gold);

	// Placed and switched on by UpdateSummary().
	summary = new Tooltiphook(InGame, 0, 0);
	summary->SetActive(false);

	ApplyLayout();
}

// The list takes whatever height the window leaves it, so a resize needs
// nothing but this.
void SetTab::ApplyLayout() {
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

std::vector<ChatCommand> SetTab::GetCommands() {
	return { { "set", { "sets" }, "<search>", "Opens the Sets tab" } };
}

// One text criterion, scoped to the pieces of a set. An empty search is carried
// by every source, which is what shows the whole list.
void SetTab::RunQuery() {
	StatIndex::Query query;
	query.kind = SetCatalogue::Kind;
	query.criteria.push_back(StatIndex::Criterion::OnText(search));
	results = StatIndex::Find(query);
}

// Headings are driven off the rows rather than the data, so a filter that cuts a
// set's first piece still names the set above whichever is now first.
void SetTab::PushRows() {
	unsigned int mostRows = (unsigned int)(2 * results.size());
	std::vector<ListRow> rows;
	rows.reserve(mostRows);
	rowPieces.clear();
	rowPieces.reserve(mostRows);
	shownSets = 0;

	for (unsigned int i = 0; i < results.size(); i++) {
		const Catalogue::Source& piece = *results[i].entry->source;
		if (i == 0 || piece.setName != results[i - 1].entry->source->setName) {
			rows.push_back(ListRow({ piece.setName }, true));
			rowPieces.push_back(NULL);
			shownSets++;
		}

		rows.push_back(ListRow({ piece.name, piece.baseName }));
		rowPieces.push_back(&piece);
	}

	list->SetRows(rows);	// also clears the selection

	// Not on open, which is usually before the game data has loaded, and not on a
	// filtered list, which would leave every set it did not match unfolded.
	if (foldOnPush && search.empty() && !rows.empty()) {
		list->FoldAllGroups();
		foldOnPush = false;
	}
	shownSummary = -1;
}

// ItemDescription orders and spaces the panel the way the game describes an
// item; the tab only says what goes in it.
std::vector<TooltipLine> SetTab::BuildSummaryLines(const Catalogue::Source& piece) {
	TextColor color = RarityColor(piece.rarity);

	ItemDescription::Description item;
	item.AddTitle(piece.name, color);
	item.AddBase(piece.baseCode, color, piece.modifiers);

	// A piece can ask for a higher level than the base it is made on does.
	if (piece.requiredLevel > item.requirements.level)
		item.requirements.level = piece.requiredLevel;

	item.AddStats(piece.lines, Blue);
	item.AddStats(piece.partialLines, color);

	const Catalogue::Source* set = SetCatalogue::FindBonus(piece.setCode);
	if (set) {
		// The set's other pieces are not listed: they are already on screen either
		// side of the row, and a set as deep as Trang-Oul's would take the panel
		// past the bottom of a 640x480 screen.
		item.AddSection(set->name, Gold, std::vector<std::string>(), color, true);
		item.AddSection("Partial Set Bonus", Gold, set->partialLines, color);
		item.AddSection("Complete Set Bonus", Gold, set->lines, color);
	}
	return ItemDescription::Build(item);
}

// Follows the mouse, falling back to the selection. Rebuilt only when the row
// changes, since the mouse sits on one row for many frames.
void SetTab::UpdateSummary() {
	int row = list->GetHoveredRow();
	if (row < 0)
		row = list->GetSelectedRow();

	// A heading describes nothing: its bonuses are on every one of its pieces.
	bool describable = IsActive() && row >= 0 && row < (int)rowPieces.size() &&
		rowPieces[row] != NULL;
	if (!describable) {
		summary->SetActive(false);
		shownSummary = -1;
		return;
	}

	if (row != shownSummary) {
		summary->SetLines(BuildSummaryLines(*rowPieces[row]));
		shownSummary = row;
	}

	// Must follow SetLines(): where it fits depends on how big it turned out.
	summary->PlaceBeside(tab->GetX(), tab->GetY(), tab->GetXSize(), tab->GetYSize());
	summary->SetActive(true);
}

void SetTab::Search(const std::string& text) {
	search = ToLower(Trim(text));
	list->SetScrollTop(0);
	needsRefresh = true;
}

void SetTab::OnClose() {
	summary->SetActive(false);
	shownSummary = -1;
	foldOnPush = true;
	Search("");
}

// The hint the window's search box shows while this panel is in front.
std::string SetTab::GetSearchPlaceholder() {
	return "Search by set, item name, base item or item type";
}

// Counted in pieces and sets rather than rows: the list interleaves headings with
// pieces, so "3 - 12 of 127" would be a range the user cannot check.
std::string SetTab::GetStatus() {
	if (!catalogueLoaded)
		return "Waiting for game data to finish loading...";
	if (results.empty())
		return "No set items match \"" + search + "\"";

	char line[64];
	sprintf_s(line, sizeof(line), "%u set items in %u set%s",
		(unsigned int)results.size(), shownSets, (shownSets == 1) ? "" : "s");
	return line;
}

// Row 0 is a heading, so enter takes the first row holding a piece.
void SetTab::OnSearchSubmitted() {
	for (unsigned int i = 0; i < rowPieces.size(); i++) {
		if (rowPieces[i]) {
			list->SetSelectedRow((int)i);
			break;
		}
	}
}

void SetTab::OnDraw() {
	if (tab->GetXSize() != laidOutWidth || tab->GetYSize() != laidOutHeight)
		ApplyLayout();

	// The catalogues are read on the thread that read the tables, which can
	// finish either before this tab exists or after it has drawn a frame.
	if (!catalogueLoaded && Catalogue::Loaded()) {
		catalogueLoaded = true;
		needsRefresh = true;
	}

	if (needsRefresh) {
		// Suspended rather than cleared, so clearing the search restores the
		// user's folds.
		list->SetFoldingSuspended(!search.empty());
		RunQuery();
		PushRows();
		needsRefresh = false;
	}

	// The mouse and the selection move on the input thread, so catch up here.
	UpdateSummary();
}

bool SetTab::OnKey(bool up, BYTE key) {
	switch (key) {
		// The selection ends on the set rather than being let go of, so folding and
		// unfolding are both reachable from wherever the last press left it.
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
				// Already inside the set, so there is nowhere further in to go.
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
