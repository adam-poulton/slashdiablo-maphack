#include "SetTab.h"
#include <algorithm>
#include <map>
#include "../../BH.h"
#include "../../Common.h"
#include "../../ItemRarity.h"
#include "../../MPQInit.h"
#include "../../StatDescriptions.h"
#include "../../TableReader.h"
#include "InfoText.h"

using namespace Drawing;
using namespace InfoText;

// Margins and the gaps between the three bands. Widths and the list height are
// measured from the tab by ApplyLayout().
#define ST_MARGIN			6	// down either side, and below the status line
#define ST_SEARCH_Y			3
#define ST_SEARCH_GAP		7	// between the search box and the list
#define ST_FOOTER_GAP		6	// between the list and the status line
#define ST_FOOTER_HEIGHT	8	// the status line itself

// The piece takes the larger share: its name carries its set's name as well
// ("Tal Rasha's Fine-Spun Cloth" against "Mesh Belt").
#define ST_COL_ITEM_WEIGHT	3
#define ST_COL_BASE_WEIGHT	2
#define ST_COL_GAP			4

// How many groups of each shape the tables give a row.
#define ST_OWN_COUNT		9	// prop1-9 on a set item
#define ST_PARTIAL_COUNT	5	// aprop1a-5b on an item, PCode2a-5b on a set
#define ST_FULL_COUNT		8	// FCode1-8 on a set

// add func decides whether an item's aprop entries apply at all. Any other value,
// blank included, means they never do whatever the file lists against them.
#define ST_ADD_TOGETHER		1	// all of them, as soon as one other piece is worn
#define ST_ADD_PROGRESSIVE	2	// aprop N at N+1 pieces

// "index" doubles as the string table key in both tables. The table is preferred
// because several pieces shipped under a working title the files still carry:
// "Tal Rasha's Fire-Spun Cloth" in the file is "Fine-Spun" in game.
static std::string TableName(const std::string& index) {
	if (index.length() == 0)
		return index;
	std::string localized = StatDescriptions::GetString(index);
	return (localized.length() > 0) ? localized : index;
}

// Every property group in both tables has this shape; only the column names and
// the item count differ.
static void ReadProperty(JSONObject* entry, const std::string& codeColumn,
		const std::string& paramColumn, const std::string& minColumn,
		const std::string& maxColumn, int itemCount,
		std::vector<SetProperty>& into) {
	SetProperty property;
	property.code = Trim(entry->getString(codeColumn));
	if (property.code.length() == 0)
		return;
	property.param = Trim(entry->getString(paramColumn));
	property.min = atoi(entry->getString(minColumn).c_str());
	property.max = atoi(entry->getString(maxColumn).c_str());
	property.itemCount = itemCount;
	into.push_back(property);
}

// Rendered a group at a time, not all at once: stats from different groups are
// separate lines on the item, and one pass would merge the belt's own
// "+60 Defense (2 Items)" with its set's "+150 Defense" into a single +210.
static std::vector<std::string> RenderGroup(const std::vector<SetProperty>& properties) {
	std::vector<StatDescriptions::Stat> stats;
	for (unsigned int i = 0; i < properties.size(); i++) {
		StatDescriptions::CollectProperty(properties[i].code, properties[i].param,
			properties[i].min, properties[i].max, stats);
	}
	return StatDescriptions::BuildLines(stats);
}

// A count at a time, so each adds up only among itself.
static std::vector<std::string> RenderCounted(const std::vector<SetProperty>& properties) {
	std::vector<std::string> lines;
	for (int count = 2; count <= ST_PARTIAL_COUNT + 1; count++) {
		std::vector<SetProperty> group;
		for (unsigned int i = 0; i < properties.size(); i++) {
			if (properties[i].itemCount == count)
				group.push_back(properties[i]);
		}
		if (group.empty())
			continue;

		std::string tag = " (" + std::to_string(count) + " Items)";
		std::vector<std::string> rendered = RenderGroup(group);
		for (unsigned int i = 0; i < rendered.size(); i++)
			lines.push_back(rendered[i] + tag);
	}
	return lines;
}

SetTab::SetTab(UI* ui) : InfoTab("Sets", ui),
	shownSets(0),
	foldOnPush(true),
	shownSummary(-1),
	setsLoaded(false),
	needsRefresh(true) {

	searchBox = new Inputhook(tab, ST_MARGIN, ST_SEARCH_Y, 0, "");
	searchBox->SetPlaceholder("Search by set, item name, base item or item type");
	searchBox->SetClearOnFocus(true);

	list = new Listhook(tab, ST_MARGIN, 0, 0, 0);
	// Both columns name the same piece, so they share its colour.
	TextColor set = RarityColor(RaritySet);
	std::vector<ListColumn> columns;
	columns.push_back(ListColumn("", 0, ST_COL_ITEM_WEIGHT, 0, set, White));
	columns.push_back(ListColumn("", 0, ST_COL_BASE_WEIGHT, ST_COL_GAP, set, White));
	list->SetColumns(columns);
	list->SetGroupColor(Gold);

	statusText = new Texthook(tab, ST_MARGIN, 0, "");
	statusText->SetColor(Grey);

	// Placed and switched on by UpdateSummary().
	summary = new Tooltiphook(InGame, 0, 0);
	summary->SetActive(false);

	ApplyLayout();
}

// The list takes whatever height is left between the search box and the status
// line, so a resize needs nothing but this.
void SetTab::ApplyLayout() {
	laidOutWidth = tab->GetXSize();
	laidOutHeight = tab->GetYSize();

	unsigned int contentWidth = (laidOutWidth > 2 * ST_MARGIN) ?
		(laidOutWidth - (2 * ST_MARGIN)) : 0;

	// Measured off the box rather than guessed, since its height follows its font.
	unsigned int listY = ST_SEARCH_Y + searchBox->GetYSize() + ST_SEARCH_GAP;
	unsigned int footerBand = ST_FOOTER_GAP + ST_FOOTER_HEIGHT + ST_MARGIN;
	unsigned int listHeight = (laidOutHeight > listY + footerBand) ?
		(laidOutHeight - listY - footerBand) : 0;

	searchBox->SetXSize(contentWidth);
	list->SetBaseY(listY);
	list->SetSize(contentWidth, listHeight);
	statusText->SetBaseY(listY + listHeight + ST_FOOTER_GAP);
	summary->SetMaxWidth(contentWidth);
}

void SetTab::MpqLoaded() {
	StatDescriptions::Initialize();
	BuildSets();
	BuildItems();
}

bool SetTab::HandlesCommand(const std::string& command) {
	return command.compare("set") == 0 || command.compare("sets") == 0;
}

void SetTab::BuildSets() {
	sets.clear();

	for (int i = 0; i < Tables::Sets.size(); i++) {
		JSONObject* entry = Tables::Sets.entryAt(i);
		if (!entry)
			continue;

		std::string index = Trim(entry->getString("index"));
		if (index.length() == 0)
			continue;

		SetRecord set;
		set.name = TableName(index);
		if (set.name.length() == 0)
			continue;

		// PCodeN unlocks at N pieces, two slots each. Only Trang-Oul's uses the
		// second, for its three oskills.
		for (int count = 2; count <= ST_PARTIAL_COUNT; count++) {
			std::string n = std::to_string(count);
			const char* slots[] = { "a", "b" };
			for (int slot = 0; slot < 2; slot++) {
				std::string s = slots[slot];
				ReadProperty(entry, "PCode" + n + s, "PParam" + n + s,
					"PMin" + n + s, "PMax" + n + s, count, set.partial);
			}
		}

		for (int n = 1; n <= ST_FULL_COUNT; n++) {
			std::string slot = std::to_string(n);
			ReadProperty(entry, "FCode" + slot, "FParam" + slot,
				"FMin" + slot, "FMax" + slot, 0, set.full);
		}

		sets.push_back(set);
	}

	std::sort(sets.begin(), sets.end(), [](const SetRecord& a, const SetRecord& b) {
		return ToLower(a.name) < ToLower(b.name);
	});
}

// Table order within a set is the game's own head to toe order.
void SetTab::BuildItems() {
	items.clear();
	matches.clear();

	std::map<std::string, int> setIndexByName;
	for (unsigned int i = 0; i < sets.size(); i++)
		setIndexByName[ToLower(sets[i].name)] = (int)i;

	// Table order has to survive until the pieces are grouped.
	std::vector<SetItemRecord> parsed;
	for (int i = 0; i < Tables::SetItems.size(); i++) {
		JSONObject* entry = Tables::SetItems.entryAt(i);
		if (!entry)
			continue;

		std::string code = Trim(entry->getString("item"));
		if (code.length() == 0)
			continue;

		SetItemRecord item;
		item.name = TableName(Trim(entry->getString("index")));
		if (item.name.length() == 0)
			continue;

		item.setName = TableName(Trim(entry->getString("set")));
		item.baseName = ItemName(code);
		item.requiredLevel = atoi(entry->getString("lvl req").c_str());

		std::map<std::string, int>::iterator found =
			setIndexByName.find(ToLower(item.setName));
		if (found != setIndexByName.end())
			item.setIndex = found->second;

		// What makes a search for "amulet" work; the base's name rarely says.
		std::map<std::string, ItemAttributes*>::iterator attrs =
			ItemAttributeMap.find(code);
		if (attrs != ItemAttributeMap.end() && attrs->second)
			item.itemType = ItemTypeName(attrs->second->category);

		for (int n = 1; n <= ST_OWN_COUNT; n++) {
			std::string index = std::to_string(n);
			ReadProperty(entry, "prop" + index, "par" + index,
				"min" + index, "max" + index, 0, item.own);
		}

		// A blank add func is not merely a piece with no aprops listed: Civerb's
		// Cudgel lists a per level damage bonus the game has never granted it.
		int addFunc = atoi(entry->getString("add func").c_str());
		if (addFunc == ST_ADD_TOGETHER || addFunc == ST_ADD_PROGRESSIVE) {
			for (int n = 1; n <= ST_PARTIAL_COUNT; n++) {
				std::string index = std::to_string(n);
				// apropN counts pieces besides this one, so it unlocks at N + 1.
				int count = (addFunc == ST_ADD_PROGRESSIVE) ? (n + 1) : 2;
				const char* slots[] = { "a", "b" };
				for (int slot = 0; slot < 2; slot++) {
					std::string s = slots[slot];
					ReadProperty(entry, "aprop" + index + s, "apar" + index + s,
						"amin" + index + s, "amax" + index + s, count,
						item.partial);
				}
			}
		}

		item.searchKey = ToLower(item.name + " " + item.baseName + " " +
			item.itemType + " " + item.setName);
		parsed.push_back(item);
	}

	// A piece whose set the sets table does not carry still goes in, at the end.
	for (unsigned int i = 0; i < sets.size(); i++) {
		for (unsigned int n = 0; n < parsed.size(); n++) {
			if (parsed[n].setIndex == (int)i)
				items.push_back(parsed[n]);
		}
	}
	for (unsigned int n = 0; n < parsed.size(); n++) {
		if (parsed[n].setIndex < 0)
			items.push_back(parsed[n]);
	}

	setsLoaded = true;
	needsRefresh = true;
}

void SetTab::LoadItemStats(SetItemRecord* item) {
	if (item->statsLoaded)
		return;
	item->statsLoaded = true;
	StatDescriptions::Initialize();

	item->ownStats = RenderGroup(item->own);
	item->partialStats = RenderCounted(item->partial);
}

// Cached on the set, so pointing at each of Immortal King's six pieces in turn
// renders its set bonus once.
void SetTab::LoadSetStats(SetRecord* set) {
	if (set->statsLoaded)
		return;
	set->statsLoaded = true;
	StatDescriptions::Initialize();

	set->partialStats = RenderCounted(set->partial);
	set->fullStats = RenderGroup(set->full);
}

void SetTab::ApplyFilter() {
	matches.clear();
	for (unsigned int i = 0; i < items.size(); i++) {
		if (query.empty() || items[i].searchKey.find(query) != std::string::npos)
			matches.push_back(&items[i]);
	}
}

// Headings are driven off the rows rather than the data, so a filter that cuts a
// set's first piece still names the set above whichever is now first.
void SetTab::PushRows() {
	unsigned int mostRows = (unsigned int)(matches.size() + sets.size());
	std::vector<ListRow> rows;
	rows.reserve(mostRows);
	rowItems.clear();
	rowItems.reserve(mostRows);
	shownSets = 0;

	for (unsigned int i = 0; i < matches.size(); i++) {
		if (i == 0 || matches[i]->setName != matches[i - 1]->setName) {
			rows.push_back(ListRow({ matches[i]->setName }, true));
			rowItems.push_back(NULL);
			shownSets++;
		}

		rows.push_back(ListRow({ matches[i]->name, matches[i]->baseName }));
		rowItems.push_back(matches[i]);
	}

	list->SetRows(rows);	// also clears the selection

	// Not on open, which is usually before the game data has loaded, and not on a
	// filtered list, which would leave every set it did not match unfolded.
	if (foldOnPush && query.empty() && !rows.empty()) {
		list->FoldAllGroups();
		foldOnPush = false;
	}
	shownSummary = -1;
	UpdateStatus();
}

// Counted in pieces and sets rather than rows: the list interleaves headings with
// pieces, so "3 - 12 of 127" would be a range the user cannot check.
void SetTab::UpdateStatus() {
	if (!setsLoaded) {
		statusText->SetText("Waiting for game data to finish loading...");
	} else if (matches.empty()) {
		statusText->SetText("No set items match \"%s\"", query.c_str());
	} else {
		statusText->SetText("%u set items in %u set%s",
			(unsigned int)matches.size(), shownSets,
			(shownSets == 1) ? "" : "s");
	}
}

// Ordered the way the game describes a set item. The panel wraps and sizes itself
// to whatever it is handed.
void SetTab::BuildSummaryLines(SetItemRecord* item,
		std::vector<TooltipLine>& lines) {
	TextColor color = RarityColor(RaritySet);
	lines.push_back(TooltipLine(item->name, color));
	lines.push_back(TooltipLine(item->baseName, color));
	if (item->requiredLevel > 0) {
		char required[64];
		sprintf_s(required, "Required level: %d", item->requiredLevel);
		lines.push_back(TooltipLine(required, White));
	}
	lines.push_back(TooltipLine("", White));

	for (unsigned int i = 0; i < item->ownStats.size(); i++)
		lines.push_back(TooltipLine(item->ownStats[i], Blue));
	for (unsigned int i = 0; i < item->partialStats.size(); i++)
		lines.push_back(TooltipLine(item->partialStats[i], color));

	if (item->setIndex < 0 || item->setIndex >= (int)sets.size())
		return;
	SetRecord* set = &sets[item->setIndex];
	LoadSetStats(set);

	// The set's other pieces are not listed: they are already on screen either
	// side of the row, and a set as deep as Trang-Oul's would take the panel past
	// the bottom of a 640x480 screen.
	lines.push_back(TooltipLine("", White));
	lines.push_back(TooltipLine(set->name, Gold));

	if (!set->partialStats.empty()) {
		lines.push_back(TooltipLine("Partial Set Bonus", Gold));
		for (unsigned int i = 0; i < set->partialStats.size(); i++)
			lines.push_back(TooltipLine(set->partialStats[i], color));
	}
	if (!set->fullStats.empty()) {
		lines.push_back(TooltipLine("Complete Set Bonus", Gold));
		for (unsigned int i = 0; i < set->fullStats.size(); i++)
			lines.push_back(TooltipLine(set->fullStats[i], color));
	}
}

// Follows the mouse, falling back to the selection. Rebuilt only when the row
// changes, since the mouse sits on one row for many frames.
void SetTab::UpdateSummary() {
	int row = list->GetHoveredRow();
	if (row < 0)
		row = list->GetSelectedRow();

	// A heading describes nothing: its bonuses are on every one of its pieces.
	bool describable = IsActive() && row >= 0 && row < (int)rowItems.size() &&
		rowItems[row] != NULL;
	if (!describable) {
		summary->SetActive(false);
		shownSummary = -1;
		return;
	}

	if (row != shownSummary) {
		// items owns the records; rowItems only points into it.
		SetItemRecord* item = const_cast<SetItemRecord*>(rowItems[row]);
		LoadItemStats(item);

		std::vector<TooltipLine> lines;
		BuildSummaryLines(item, lines);
		summary->SetLines(lines);
		shownSummary = row;
	}

	// Must follow SetLines(): where it fits depends on how big it turned out.
	summary->PlaceBeside(tab->GetX(), tab->GetY(), tab->GetXSize(), tab->GetYSize());
	summary->SetActive(true);
}

void SetTab::Search(const std::string& text) {
	std::string trimmed = Trim(text);
	query = ToLower(trimmed);
	searchBox->SetText("%s", trimmed.c_str());
	searchBox->SetTextPos(0);
	searchBox->ResetSelection();
	searchBox->SetCursorPosition(searchBox->GetText().length());
	lastBoxText = searchBox->GetText();
	list->SetScrollTop(0);
	needsRefresh = true;
}

// The caret goes straight in the search box. A search that arrived with the
// window, from the chat command, is left alone: the box only clears on a click.
void SetTab::OnOpen() {
	searchBox->SetCursorPosition(searchBox->GetText().length());
	searchBox->SetFocused(true);
}

void SetTab::OnClose() {
	searchBox->SetFocused(false);
	summary->SetActive(false);
	shownSummary = -1;
	foldOnPush = true;
	Search("");
}

void SetTab::OnDraw() {
	if (tab->GetXSize() != laidOutWidth || tab->GetYSize() != laidOutHeight)
		ApplyLayout();

	// MpqLoaded can fire before this tab exists.
	if (!setsLoaded && Tables::isInitialized()) {
		StatDescriptions::Initialize();
		BuildSets();
		BuildItems();
	}

	if (searchBox->GetText() != lastBoxText) {
		lastBoxText = searchBox->GetText();
		query = ToLower(Trim(lastBoxText));
		list->SetScrollTop(0);
		needsRefresh = true;
	}

	if (needsRefresh) {
		ApplyFilter();
		// Suspended rather than cleared, so clearing the search restores the
		// user's folds.
		list->SetFoldingSuspended(!query.empty());
		PushRows();
		needsRefresh = false;
	}

	// Row 0 is a heading, so enter takes the first row holding a piece.
	if (searchBox->TakeSubmitted()) {
		for (unsigned int i = 0; i < rowItems.size(); i++) {
			if (rowItems[i]) {
				list->SetSelectedRow((int)i);
				break;
			}
		}
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
