#include "BaseTab.h"
#include <algorithm>
#include <map>
#include "../../BH.h"
#include "../../Common.h"
#include "../../ItemDescription.h"
#include "../../ItemRarity.h"
#include "../../StatDescriptions.h"
#include "../../TableReader.h"
#include "InfoText.h"

using namespace Drawing;
using namespace InfoText;

// The name takes the larger share; the tier beside it is one short word.
#define BT_COL_NAME_WEIGHT	3
#define BT_COL_TIER_WEIGHT	1
#define BT_COL_GAP			4

// The game never writes a tier out anywhere, so the words are ours. They are the
// ones the recipes panel already uses for the same three tiers.
static std::string TierName(ItemDescription::Tier tier) {
	switch (tier) {
		case ItemDescription::TierExceptional:	return "Exceptional";
		case ItemDescription::TierElite:		return "Elite";
		default:								return "Normal";
	}
}

BaseTab::BaseTab(UI* ui) : UIPanel("Bases", ui),
	shownTypes(0),
	foldOnPush(true),
	shownSummary(-1),
	basesLoaded(false),
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

void BaseTab::MpqLoaded() {
	StatDescriptions::Initialize();
	BuildItems();
}

std::vector<ChatCommand> BaseTab::GetCommands() {
	return { { "base", { "bases" }, "<search>", "Opens the Bases tab" } };
}

void BaseTab::BuildItems() {
	items.clear();
	matches.clear();

	const std::vector<const ItemDescription::Base*>& all = ItemDescription::AllBases();

	// Only what the game drops, which leaves out the quest pieces and the rows
	// that were never finished. A table carrying no such column would leave
	// nothing at all, so a second pass takes everything rather than an empty tab.
	for (int pass = 0; pass < 2 && items.empty(); pass++) {
		bool requireSpawnable = (pass == 0);
		for (unsigned int i = 0; i < all.size(); i++) {
			const ItemDescription::Base* base = all[i];
			if (requireSpawnable && !base->spawnable)
				continue;
			if (base->name.length() == 0 || base->typeName.length() == 0)
				continue;

			BaseItemRecord item;
			item.code = base->code;
			item.name = base->name;
			item.typeName = base->typeName;
			item.tierName = TierName(base->tier);
			item.tier = base->tier;
			item.level = base->level;
			item.searchKey = ToLower(item.name + " " + item.typeName + " " +
				item.tierName + " " + item.code);
			items.push_back(item);
		}
	}

	// Headings in the order the tables first reach them, which walks the weapons,
	// then the armour, then everything else, rather than scattering the two
	// hundred armour bases through the alphabet.
	std::map<std::string, int> groupOrder;
	for (unsigned int i = 0; i < items.size(); i++) {
		if (groupOrder.find(items[i].typeName) == groupOrder.end()) {
			int next = (int)groupOrder.size();
			groupOrder[items[i].typeName] = next;
		}
	}
	for (unsigned int i = 0; i < items.size(); i++)
		items[i].group = groupOrder[items[i].typeName];

	// Stable, so bases sharing a tier and a level keep their table order, which
	// is the game's own progression through them.
	std::stable_sort(items.begin(), items.end(),
			[](const BaseItemRecord& a, const BaseItemRecord& b) {
		if (a.group != b.group)
			return a.group < b.group;
		if (a.tier != b.tier)
			return a.tier < b.tier;
		return a.level < b.level;
	});

	basesLoaded = true;
	needsRefresh = true;
}

void BaseTab::ApplyFilter() {
	matches.clear();
	for (unsigned int i = 0; i < items.size(); i++) {
		if (query.empty() || items[i].searchKey.find(query) != std::string::npos)
			matches.push_back(&items[i]);
	}
}

// Headings are driven off the rows rather than the data, so a filter that cuts a
// type's first item still names the type above whichever is now first.
void BaseTab::PushRows() {
	unsigned int mostRows = (unsigned int)(matches.size() * 2);
	std::vector<ListRow> rows;
	rows.reserve(mostRows);
	rowItems.clear();
	rowItems.reserve(mostRows);
	shownTypes = 0;

	for (unsigned int i = 0; i < matches.size(); i++) {
		if (i == 0 || matches[i]->typeName != matches[i - 1]->typeName) {
			rows.push_back(ListRow({ matches[i]->typeName }, true));
			rowItems.push_back(NULL);
			shownTypes++;
		}

		rows.push_back(ListRow({ matches[i]->name, matches[i]->tierName }));
		rowItems.push_back(matches[i]);
	}

	list->SetRows(rows);	// also clears the selection

	// Not on open, which is usually before the game data has loaded, and not on a
	// filtered list, which would leave every type it did not match unfolded.
	if (foldOnPush && query.empty() && !rows.empty()) {
		list->FoldAllGroups();
		foldOnPush = false;
	}
	shownSummary = -1;
}

// ItemDescription orders and spaces the panel the way the game describes an
// item; the tab only says what goes in it.
std::vector<TooltipLine> BaseTab::BuildSummaryLines(const BaseItemRecord* item) {
	ItemDescription::Description base;
	base.AddBase(item->code, White);
	base.AddTitle(item->tierName + " " + item->typeName, Grey);
	base.AddBaseLimits(item->code);
	return ItemDescription::Build(base);
}

// Follows the mouse, falling back to the selection. Rebuilt only when the row
// changes, since the mouse sits on one row for many frames.
void BaseTab::UpdateSummary() {
	int row = list->GetHoveredRow();
	if (row < 0)
		row = list->GetSelectedRow();

	// A heading describes nothing: it is only the name of the type below it.
	bool describable = IsActive() && row >= 0 && row < (int)rowItems.size() &&
		rowItems[row] != NULL;
	if (!describable) {
		summary->SetActive(false);
		shownSummary = -1;
		return;
	}

	if (row != shownSummary) {
		summary->SetLines(BuildSummaryLines(rowItems[row]));
		shownSummary = row;
	}

	// Must follow SetLines(): where it fits depends on how big it turned out.
	summary->PlaceBeside(tab->GetX(), tab->GetY(), tab->GetXSize(), tab->GetYSize());
	summary->SetActive(true);
}

void BaseTab::Search(const std::string& text) {
	query = ToLower(Trim(text));
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
	if (!basesLoaded)
		return "Waiting for game data to finish loading...";
	if (matches.empty())
		return "No base items match \"" + query + "\"";

	char line[64];
	sprintf_s(line, sizeof(line), "%u base items in %u type%s",
		(unsigned int)matches.size(), shownTypes, (shownTypes == 1) ? "" : "s");
	return line;
}

// Row 0 is a heading, so enter takes the first row holding an item.
void BaseTab::OnSearchSubmitted() {
	for (unsigned int i = 0; i < rowItems.size(); i++) {
		if (rowItems[i]) {
			list->SetSelectedRow((int)i);
			break;
		}
	}
}

void BaseTab::OnDraw() {
	if (tab->GetXSize() != laidOutWidth || tab->GetYSize() != laidOutHeight)
		ApplyLayout();

	// MpqLoaded can fire before this tab exists.
	if (!basesLoaded && Tables::isInitialized()) {
		StatDescriptions::Initialize();
		BuildItems();
	}

	if (needsRefresh) {
		ApplyFilter();
		// Suspended rather than cleared, so clearing the search restores the
		// user's folds.
		list->SetFoldingSuspended(!query.empty());
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
