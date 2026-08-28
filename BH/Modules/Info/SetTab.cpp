#include "SetTab.h"
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

// Margins and the gaps between the three bands. Widths and the list height are
// measured from the tab by ApplyLayout().

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

SetTab::SetTab(UI* ui) : UIPanel("Sets", ui),
	shownSets(0),
	foldOnPush(true),
	shownSummary(-1),
	setsLoaded(false),
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

void SetTab::MpqLoaded() {
	StatDescriptions::Initialize();
	BuildSets();
	BuildItems();
}

std::vector<ChatCommand> SetTab::GetCommands() {
	return { { "set", { "sets" }, "<search>", "Opens the Sets tab" } };
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
		item.code = code;
		item.name = TableName(Trim(entry->getString("index")));
		if (item.name.length() == 0)
			continue;

		item.setName = TableName(Trim(entry->getString("set")));
		item.baseName = ItemDescription::BaseName(code);
		item.requiredLevel = atoi(entry->getString("lvl req").c_str());

		std::map<std::string, int>::iterator found =
			setIndexByName.find(ToLower(item.setName));
		if (found != setIndexByName.end())
			item.setIndex = found->second;

		// What makes a search for "amulet" work; the base's name rarely says.
		const ItemDescription::Base* base = ItemDescription::FindBase(code);
		if (base)
			item.itemType = base->typeName;

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

	std::vector<StatDescriptions::StatTotal> totals;
	for (unsigned int i = 0; i < item->own.size(); i++) {
		StatDescriptions::CollectTotals(item->own[i].code, item->own[i].param,
			item->own[i].min, item->own[i].max, totals);
	}
	item->modifiers = ItemDescription::ReadModifiers(totals);
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
}

// ItemDescription orders and spaces the panel the way the game describes an
// item; the tab only says what goes in it.
std::vector<TooltipLine> SetTab::BuildSummaryLines(SetItemRecord* item) {
	TextColor color = RarityColor(RaritySet);

	ItemDescription::Description piece;
	piece.AddTitle(item->name, color);
	piece.AddBase(item->code, color, item->modifiers);

	// A piece can ask for a higher level than the base it is made on does.
	if (item->requiredLevel > piece.requirements.level)
		piece.requirements.level = item->requiredLevel;

	piece.AddStats(item->ownStats, Blue);
	piece.AddStats(item->partialStats, color);

	if (item->setIndex >= 0 && item->setIndex < (int)sets.size()) {
		SetRecord* set = &sets[item->setIndex];
		LoadSetStats(set);

		// The set's other pieces are not listed: they are already on screen either
		// side of the row, and a set as deep as Trang-Oul's would take the panel
		// past the bottom of a 640x480 screen.
		piece.AddSection(set->name, Gold, std::vector<std::string>(), color, true);
		piece.AddSection("Partial Set Bonus", Gold, set->partialStats, color);
		piece.AddSection("Complete Set Bonus", Gold, set->fullStats, color);
	}
	return ItemDescription::Build(piece);
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

		summary->SetLines(BuildSummaryLines(item));
		shownSummary = row;
	}

	// Must follow SetLines(): where it fits depends on how big it turned out.
	summary->PlaceBeside(tab->GetX(), tab->GetY(), tab->GetXSize(), tab->GetYSize());
	summary->SetActive(true);
}

void SetTab::Search(const std::string& text) {
	query = ToLower(Trim(text));
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
	if (!setsLoaded)
		return "Waiting for game data to finish loading...";
	if (matches.empty())
		return "No set items match \"" + query + "\"";

	char line[64];
	sprintf_s(line, sizeof(line), "%u set items in %u set%s",
		(unsigned int)matches.size(), shownSets, (shownSets == 1) ? "" : "s");
	return line;
}

// Row 0 is a heading, so enter takes the first row holding a piece.
void SetTab::OnSearchSubmitted() {
	for (unsigned int i = 0; i < rowItems.size(); i++) {
		if (rowItems[i]) {
			list->SetSelectedRow((int)i);
			break;
		}
	}
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
