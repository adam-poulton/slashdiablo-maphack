#include "UniqueTab.h"
#include <algorithm>
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

// The unique's own name takes the larger share, being the longer of the two
// ("Bul-Kathos' Tribal Guardian" against "Ceremonial Javelin").
#define UQ_COL_NAME_WEIGHT	3
#define UQ_COL_BASE_WEIGHT	2
#define UQ_COL_GAP			4

// How many prop/par/min/max groups UniqueItems.txt gives each row.
#define UQ_PROPERTY_COUNT	12

// "index" doubles as the string table key, which is preferred so a localised
// client reads correctly. The raw index is the fallback for realm additions.
static std::string UniqueName(JSONObject* entry) {
	std::string index = Trim(entry->getString("index"));
	std::string localized = StatDescriptions::GetString(index);
	return (localized.length() > 0) ? localized : index;
}

UniqueTab::UniqueTab(UI* ui) : UIPanel("Uniques", ui),
	shownSummary(-1),
	uniquesLoaded(false),
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

void UniqueTab::MpqLoaded() {
	StatDescriptions::Initialize();
	BuildUniques();
}

std::vector<ChatCommand> UniqueTab::GetCommands() {
	return { { "uni", { "uniques" }, "<search>", "Opens the Uniques tab" } };
}

void UniqueTab::BuildUniques() {
	uniques.clear();
	matches.clear();

	// Only the rows flagged enabled; the file keeps unreleased and placeholder
	// items too. A modified file with nothing flagged falls back to every row.
	for (int pass = 0; pass < 2 && uniques.empty(); pass++) {
		bool requireEnabled = (pass == 0);
		for (int i = 0; i < Tables::UniqueItems.size(); i++) {
			JSONObject* entry = Tables::UniqueItems.entryAt(i);
			if (!entry)
				continue;
			if (requireEnabled && entry->getString("enabled").compare("1") != 0)
				continue;

			std::string code = Trim(entry->getString("code"));
			if (code.length() == 0)
				continue;

			UniqueRecord unique;
			unique.statsLoaded = false;
			unique.code = code;
			unique.name = UniqueName(entry);
			if (unique.name.length() == 0)
				continue;

			unique.baseName = ItemDescription::BaseName(code);
			unique.requiredLevel = atoi(entry->getString("lvl req").c_str());

			// What makes a search for "amulet" work; the base's name rarely says.
			const ItemDescription::Base* base = ItemDescription::FindBase(code);
			if (base)
				unique.itemType = base->typeName;

			for (int n = 1; n <= UQ_PROPERTY_COUNT; n++) {
				std::string index = std::to_string(n);
				UniqueProperty property;
				property.code = Trim(entry->getString("prop" + index));
				if (property.code.length() == 0)
					continue;
				property.param = Trim(entry->getString("par" + index));
				property.min = atoi(entry->getString("min" + index).c_str());
				property.max = atoi(entry->getString("max" + index).c_str());
				unique.properties.push_back(property);
			}

			unique.searchKey = ToLower(unique.name + " " + unique.baseName + " " +
				unique.itemType);
			uniques.push_back(unique);
		}
	}

	// The facet jewels share a name across several bases, so the base is the
	// tiebreak and equal names still land next to each other.
	std::sort(uniques.begin(), uniques.end(), [](const UniqueRecord& a, const UniqueRecord& b) {
		std::string nameA = ToLower(a.name), nameB = ToLower(b.name);
		if (nameA != nameB)
			return nameA < nameB;
		return ToLower(a.baseName) < ToLower(b.baseName);
	});

	uniquesLoaded = true;
	needsRefresh = true;
}

void UniqueTab::LoadStats(UniqueRecord* unique) {
	if (unique->statsLoaded)
		return;
	unique->statsLoaded = true;
	StatDescriptions::Initialize();

	std::vector<StatDescriptions::Stat> stats;
	std::vector<StatDescriptions::StatTotal> totals;
	for (unsigned int i = 0; i < unique->properties.size(); i++) {
		const UniqueProperty& property = unique->properties[i];
		StatDescriptions::CollectProperty(property.code, property.param,
			property.min, property.max, stats);
		StatDescriptions::CollectTotals(property.code, property.param,
			property.min, property.max, totals);
	}
	unique->stats = StatDescriptions::BuildLines(stats);
	unique->modifiers = ItemDescription::ReadModifiers(totals);
}

void UniqueTab::ApplyFilter() {
	matches.clear();
	for (unsigned int i = 0; i < uniques.size(); i++) {
		if (query.empty() || uniques[i].searchKey.find(query) != std::string::npos)
			matches.push_back(&uniques[i]);
	}
}

void UniqueTab::PushRows() {
	std::vector<std::vector<std::string>> rows;
	rows.reserve(matches.size());
	for (unsigned int i = 0; i < matches.size(); i++) {
		std::vector<std::string> row;
		row.push_back(matches[i]->name);
		row.push_back(matches[i]->baseName);
		rows.push_back(row);
	}
	list->SetRows(rows);	// also clears the selection
	shownSummary = -1;
}

// ItemDescription orders and spaces the panel the way the game describes an
// item; the tab only says what goes in it.
std::vector<TooltipLine> UniqueTab::BuildSummaryLines(UniqueRecord* unique) {
	TextColor color = RarityColor(RarityUnique);

	ItemDescription::Description item;
	item.AddTitle(unique->name, color);
	item.AddBase(unique->code, color, unique->modifiers);

	// A unique can ask for a higher level than the base it is made on does.
	if (unique->requiredLevel > item.requirements.level)
		item.requirements.level = unique->requiredLevel;

	item.AddStats(unique->stats, Blue);
	return ItemDescription::Build(item);
}

// Follows the mouse, falling back to the selection. Rebuilt only when the row
// changes, since the mouse sits on one row for many frames.
void UniqueTab::UpdateSummary() {
	int row = list->GetHoveredRow();
	if (row < 0)
		row = list->GetSelectedRow();

	if (!IsActive() || row < 0 || row >= (int)matches.size()) {
		summary->SetActive(false);
		shownSummary = -1;
		return;
	}

	if (row != shownSummary) {
		// uniques owns the records; matches only points into it.
		UniqueRecord* unique = const_cast<UniqueRecord*>(matches[row]);
		LoadStats(unique);

		summary->SetLines(BuildSummaryLines(unique));
		shownSummary = row;
	}

	// Must follow SetLines(): where it fits depends on how big it turned out.
	summary->PlaceBeside(tab->GetX(), tab->GetY(), tab->GetXSize(), tab->GetYSize());
	summary->SetActive(true);
}

void UniqueTab::Search(const std::string& text) {
	query = ToLower(Trim(text));
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
	if (!uniquesLoaded)
		return "Waiting for game data to finish loading...";
	if (matches.empty())
		return "No uniques match \"" + query + "\"";

	char line[64];
	if (list->GetMaxScrollTop() > 0) {
		sprintf_s(line, sizeof(line), "%u - %u of %u uniques",
			list->GetFirstVisibleRow() + 1, list->GetLastVisibleRow(),
			(unsigned int)matches.size());
	} else {
		sprintf_s(line, sizeof(line), "%u uniques", (unsigned int)matches.size());
	}
	return line;
}

// Enter picks the first match rather than typing a newline.
void UniqueTab::OnSearchSubmitted() {
	if (!matches.empty())
		list->SetSelectedRow(0);
}

void UniqueTab::OnDraw() {
	if (tab->GetXSize() != laidOutWidth || tab->GetYSize() != laidOutHeight)
		ApplyLayout();

	// MpqLoaded can fire before this tab exists.
	if (!uniquesLoaded && Tables::isInitialized()) {
		StatDescriptions::Initialize();
		BuildUniques();
	}

	if (needsRefresh) {
		ApplyFilter();
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
