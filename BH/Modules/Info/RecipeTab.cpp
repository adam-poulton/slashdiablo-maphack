#include "RecipeTab.h"
#include "../../BH.h"
#include "../../Catalogue/Catalogues.h"
#include "../../Catalogue/RecipeCatalogue.h"
#include "../../Common.h"
#include "../../ItemDescription.h"
#include "../../ItemRarity.h"
#include "../../StringUtil.h"

using namespace Drawing;

// Separate what a recipe makes and what it is made from
#define RC_SEPARATOR		": "
#define RC_COL_GAP			1

RecipeTab::RecipeTab(UI* ui) : UIPanel("Recipes", ui),
	shownHeadings(0),
	foldOnPush(true),
	shownSummary(-1),
	catalogueLoaded(false),
	needsRefresh(true) {

	list = new Listhook(tab, UI_CONTENT_MARGIN, 0, 0, 0);
	// One line in two colours: what the recipe makes, in the colour of the item
	// it makes, and then what it is made from flowing straight on from it.
	//
	// A recipe makes whatever quality of item it makes, so the result's colour
	// comes from the row rather than from the column. Items that would render
	// white are made gold to allow for white on hover.
	std::vector<ListColumn> columns;
	columns.push_back(ListColumn("", 0, 1, 0, Gold, White));
	columns.push_back(ListColumn("", 0, 0, RC_COL_GAP, Grey, White, true));
	list->SetColumns(columns);
	list->SetGroupColor(Gold);

	// Placed and switched on by UpdateSummary().
	summary = new Tooltiphook(InGame, 0, 0);
	summary->SetActive(false);

	ApplyLayout();
}

// The list takes whatever height the window leaves it, so a resize needs
// nothing but this.
void RecipeTab::ApplyLayout() {
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

std::vector<ChatCommand> RecipeTab::GetCommands() {
	return { { "cube", { "recipe", "recipes" }, "<search>",
		"Opens the Recipes tab" } };
}

// One text criterion, scoped to the recipes. An empty search is carried by
// every source, which is what shows the whole list.
void RecipeTab::RunQuery() {
	StatIndex::Query query;
	query.kind = RecipeCatalogue::Kind;
	query.criteria.push_back(StatIndex::Criterion::OnText(search));
	results = StatIndex::Find(query);
}

// The index answers in the order the catalogue lists its recipes, which already
// has the ones sharing a heading next to each other, so a heading is pushed
// wherever the answer moves on to the next one.
void RecipeTab::PushRows() {
	std::vector<ListRow> rows;
	rows.reserve(results.size() * 2);
	rowRecipes.clear();
	rowRecipes.reserve(results.size() * 2);
	shownHeadings = 0;

	for (unsigned int i = 0; i < results.size(); i++) {
		const Catalogue::Source& recipe = *results[i].entry->source;
		if (i == 0 || recipe.heading != results[i - 1].entry->source->heading) {
			rows.push_back(ListRow({ recipe.heading }, true));
			rowRecipes.push_back(NULL);
			shownHeadings++;
		}

		// The result carries its own colour; the ingredients keep the column's.
		ListRow row({ recipe.name, RC_SEPARATOR + recipe.ingredients });
		row.colors.push_back(NameColor(recipe.rarity));
		rows.push_back(row);
		rowRecipes.push_back(&recipe);
	}

	list->SetRows(rows);	// also clears the selection

	// Not on open, which is usually before the game data has loaded, and not on
	// a filtered list, which would leave every heading it did not match unfolded.
	if (foldOnPush && search.empty() && !rows.empty()) {
		list->FoldAllGroups();
		foldOnPush = false;
	}
	shownSummary = -1;
}

// ItemDescription orders and spaces the panel the way the game describes a
// recipe; the tab only says what goes in it.
std::vector<TooltipLine> RecipeTab::BuildSummaryLines(
		const Catalogue::Source& recipe) {
	ItemDescription::Recipe cube;
	cube.name = recipe.name;
	cube.nameColor = NameColor(recipe.rarity);
	cube.ingredients = recipe.ingredients;
	cube.AddStats(recipe.lines, Blue);
	cube.AddStats(recipe.notes, Grey, true);
	return ItemDescription::Build(cube);
}

// Follows the mouse, falling back to the selection. Rebuilt only when the row
// changes, since the mouse sits on one row for many frames.
void RecipeTab::UpdateSummary() {
	int row = list->GetHoveredRow();
	if (row < 0)
		row = list->GetSelectedRow();

	// A heading describes nothing of its own; it only gathers its recipes up.
	bool describable = IsActive() && row >= 0 && row < (int)rowRecipes.size() &&
		rowRecipes[row] != NULL;
	if (!describable) {
		summary->SetActive(false);
		shownSummary = -1;
		return;
	}

	if (row != shownSummary) {
		summary->SetLines(BuildSummaryLines(*rowRecipes[row]));
		shownSummary = row;
	}

	// Must follow SetLines(): where it fits depends on how big it turned out.
	summary->PlaceBeside(tab->GetX(), tab->GetY(), tab->GetXSize(), tab->GetYSize());
	summary->SetActive(true);
}

void RecipeTab::Search(const std::string& text) {
	search = ToLower(Trim(text));
	list->SetScrollTop(0);
	needsRefresh = true;
}

void RecipeTab::OnClose() {
	summary->SetActive(false);
	shownSummary = -1;
	foldOnPush = true;
	Search("");
}

// The hint the window's search box shows while this panel is in front.
std::string RecipeTab::GetSearchPlaceholder() {
	return "Search by what a recipe makes or what it takes";
}

std::string RecipeTab::GetStatus() {
	if (!catalogueLoaded)
		return "Waiting for game data to finish loading...";
	if (results.empty())
		return "No recipes match \"" + search + "\"";

	char line[64];
	sprintf_s(line, sizeof(line), "%u recipes in %u group%s",
		(unsigned int)results.size(), shownHeadings, (shownHeadings == 1) ? "" : "s");
	return line;
}

// Row 0 is a heading, so enter takes the first row holding a recipe.
void RecipeTab::OnSearchSubmitted() {
	for (unsigned int i = 0; i < rowRecipes.size(); i++) {
		if (rowRecipes[i]) {
			list->SetSelectedRow((int)i);
			break;
		}
	}
}

void RecipeTab::OnDraw() {
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

bool RecipeTab::OnKey(bool up, BYTE key) {
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
				// Already inside the group, so there is nowhere further in to go.
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
