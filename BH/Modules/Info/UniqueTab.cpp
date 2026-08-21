#include "UniqueTab.h"
#include <algorithm>
#include "../../BH.h"
#include "../../Common.h"
#include "../../MPQInit.h"
#include "../../StatDescriptions.h"
#include "../../TableReader.h"
#include "InfoText.h"

using namespace Drawing;
using namespace InfoText;

// Layout, relative to the tab's content area. Only the margins and the gaps
// between the three bands are given here; the widths and the height of the list
// are measured from the tab by ApplyLayout(), so the window can be resized
// without any of this being restated.
#define UQ_MARGIN			6	// down either side, and below the status line
#define UQ_SEARCH_Y			3
#define UQ_SEARCH_GAP		7	// between the search box and the list
#define UQ_FOOTER_GAP		6	// between the list and the status line
#define UQ_FOOTER_HEIGHT	8	// the status line itself

// Column proportions. Both columns hold a name, so both are given a share of the
// width rather than a fixed size, and the unique's own name takes the larger
// share since it is the longer of the two ("Bul-Kathos' Tribal Guardian" against
// "Ceremonial Javelin").
#define UQ_COL_NAME_WEIGHT	3
#define UQ_COL_BASE_WEIGHT	2
#define UQ_COL_GAP			4

// How many prop/par/min/max groups UniqueItems.txt gives each row.
#define UQ_PROPERTY_COUNT	12

// UniqueItems.txt keeps the item's name in "index", which doubles as its string
// table key. The string table is preferred so a localised client reads correctly,
// and the raw index is the fallback for realm additions that have no entry.
static std::string UniqueName(JSONObject* entry) {
	std::string index = Trim(entry->getString("index"));
	std::string localized = StatDescriptions::GetString(index);
	return (localized.length() > 0) ? localized : index;
}

UniqueTab::UniqueTab(UI* ui) : InfoTab("Uniques", ui),
	shownSummary(-1),
	uniquesLoaded(false),
	needsRefresh(true) {

	searchBox = new Inputhook(tab, UQ_MARGIN, UQ_SEARCH_Y, 0, "");
	searchBox->SetPlaceholder("Search by unique name, base item or item type");
	// The box holds a whole search rather than something you edit a word of, so
	// clicking into it starts a new one.
	searchBox->SetClearOnFocus(true);

	// Sized by ApplyLayout() below, along with everything else here.
	list = new Listhook(tab, UQ_MARGIN, 0, 0, 0);
	std::vector<ListColumn> columns;
	columns.push_back(ListColumn("", 0, UQ_COL_NAME_WEIGHT, 0, Gold, White));
	columns.push_back(ListColumn("", 0, UQ_COL_BASE_WEIGHT, UQ_COL_GAP, Orange));
	list->SetColumns(columns);

	statusText = new Texthook(tab, UQ_MARGIN, 0, "");
	statusText->SetColor(Grey);

	// Not part of the tab: the summary sits beside the window, which a hook
	// belonging to the tab could not reach. Positioned and switched on by
	// UpdateSummary() once there is a row to describe.
	summary = new Tooltiphook(InGame, 0, 0);
	summary->SetActive(false);

	ApplyLayout();
}

// Fits the contents to however big the tab currently is: a search box across the
// top, the list taking whatever height is left between it and the status line,
// and the summary reading at the same measure as the list. Nothing here is a
// fixed size, so the window can be resized and this is all it takes to follow.
void UniqueTab::ApplyLayout() {
	laidOutWidth = tab->GetXSize();
	laidOutHeight = tab->GetYSize();

	unsigned int contentWidth = (laidOutWidth > 2 * UQ_MARGIN) ?
		(laidOutWidth - (2 * UQ_MARGIN)) : 0;

	// The search box is as tall as its own font, so the list starts below
	// wherever it actually ends rather than at a guessed offset.
	unsigned int listY = UQ_SEARCH_Y + searchBox->GetYSize() + UQ_SEARCH_GAP;
	unsigned int footerBand = UQ_FOOTER_GAP + UQ_FOOTER_HEIGHT + UQ_MARGIN;
	unsigned int listHeight = (laidOutHeight > listY + footerBand) ?
		(laidOutHeight - listY - footerBand) : 0;

	searchBox->SetXSize(contentWidth);
	list->SetBaseY(listY);
	list->SetSize(contentWidth, listHeight);
	statusText->SetBaseY(listY + listHeight + UQ_FOOTER_GAP);
	summary->SetMaxWidth(contentWidth);
}

void UniqueTab::MpqLoaded() {
	StatDescriptions::Initialize();
	BuildUniques();
}

bool UniqueTab::HandlesCommand(const std::string& command) {
	return command.compare("uni") == 0 || command.compare("uniques") == 0;
}

void UniqueTab::BuildUniques() {
	uniques.clear();
	matches.clear();

	// UniqueItems.txt keeps unreleased and placeholder items around, so only list
	// the ones flagged enabled. If nothing is flagged (a modified file), fall
	// back to every row that names a base item.
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
			unique.name = UniqueName(entry);
			if (unique.name.length() == 0)
				continue;

			unique.baseName = ItemName(code);
			unique.requiredLevel = atoi(entry->getString("lvl req").c_str());

			// The base's item type is what makes a search for "amulet" or "boots"
			// work, since the base item's own name rarely says which it is.
			std::map<std::string, ItemAttributes*>::iterator attrs =
				ItemAttributeMap.find(code);
			if (attrs != ItemAttributeMap.end() && attrs->second)
				unique.itemType = ItemTypeName(attrs->second->category);

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

	// A handful of items share a name across several bases (the facet jewels), so
	// the base is the tiebreak and equal names still land next to each other.
	std::sort(uniques.begin(), uniques.end(), [](const UniqueRecord& a, const UniqueRecord& b) {
		std::string nameA = ToLower(a.name), nameB = ToLower(b.name);
		if (nameA != nameB)
			return nameA < nameB;
		return ToLower(a.baseName) < ToLower(b.baseName);
	});

	uniquesLoaded = true;
	needsRefresh = true;
}

// Renders an item's stats the first time it is looked at. The game adds equal
// stats together rather than listing them twice, so the same is done here before
// rendering.
void UniqueTab::LoadStats(UniqueRecord* unique) {
	if (unique->statsLoaded)
		return;
	unique->statsLoaded = true;
	StatDescriptions::Initialize();

	std::vector<StatDescriptions::Stat> stats;
	for (unsigned int i = 0; i < unique->properties.size(); i++) {
		const UniqueProperty& property = unique->properties[i];
		StatDescriptions::CollectProperty(property.code, property.param,
			property.min, property.max, stats);
	}
	StatDescriptions::MergeStats(stats);

	for (unsigned int i = 0; i < stats.size(); i++) {
		std::string line = StatDescriptions::Render(stats[i]);
		if (line.length() > 0)
			unique->stats.push_back(line);
	}
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
	// Row numbers now mean different items, so whatever the summary was built
	// for no longer holds.
	shownSummary = -1;
	UpdateStatus();
}

// The status line, which follows the scroll position as well as the rows, so it
// is refreshed every frame rather than only when the filter changes.
void UniqueTab::UpdateStatus() {
	if (!uniquesLoaded) {
		statusText->SetText("Waiting for game data to finish loading...");
	} else if (matches.empty()) {
		statusText->SetText("No uniques match \"%s\"", query.c_str());
	} else if (list->GetMaxScrollTop() > 0) {
		statusText->SetText("%u - %u of %u uniques",
			list->GetFirstVisibleRow() + 1,
			list->GetLastVisibleRow(),
			(unsigned int)matches.size());
	} else {
		statusText->SetText("%u uniques", (unsigned int)matches.size());
	}
}

// Built the way the game describes an item: what it is, then what it needs, then
// what it does. Nothing here places or measures anything; the panel wraps these
// to its own width and sizes itself to what it ends up holding.
void UniqueTab::BuildSummaryLines(UniqueRecord* unique,
		std::vector<TooltipLine>& lines) {
	lines.push_back(TooltipLine(unique->name, Gold));
	lines.push_back(TooltipLine(unique->baseName, Orange));
	if (unique->requiredLevel > 0) {
		char required[64];
		sprintf_s(required, "Required level: %d", unique->requiredLevel);
		lines.push_back(TooltipLine(required, White));
	}
	lines.push_back(TooltipLine("", White));

	for (unsigned int i = 0; i < unique->stats.size(); i++)
		lines.push_back(TooltipLine(unique->stats[i], White));
}

// The summary describes whichever row is being pointed at, and falls back to the
// selected one so the arrow keys are worth using. Rebuilt only when the row it is
// describing changes, since rendering an item is not free and the mouse sits on
// one row for many frames.
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
		// The unique list owns the records; matches only points into it.
		UniqueRecord* unique = const_cast<UniqueRecord*>(matches[row]);
		LoadStats(unique);

		std::vector<TooltipLine> lines;
		BuildSummaryLines(unique, lines);
		summary->SetLines(lines);
		shownSummary = row;
	}

	// Where it fits depends on how big it turned out, so this follows SetLines().
	summary->PlaceBeside(tab->GetX(), tab->GetY(), tab->GetXSize(), tab->GetYSize());
	summary->SetActive(true);
}

void UniqueTab::Search(const std::string& text) {
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

// Opening the window puts the caret straight in the search box, so an item can be
// looked up without reaching for the mouse. The box only empties itself when it
// is clicked into, so a search that arrived with the window - from the chat
// command - is left alone and simply ready to edit.
void UniqueTab::OnOpen() {
	searchBox->SetCursorPosition(searchBox->GetText().length());
	searchBox->SetFocused(true);
}

// Reopening the window shouldn't land on someone else's search, and the summary
// must not be left on screen after the window that raised it has gone.
void UniqueTab::OnClose() {
	searchBox->SetFocused(false);
	summary->SetActive(false);
	shownSummary = -1;
	Search("");
}

void UniqueTab::OnDraw() {
	// The window can be resized under us, so keep the contents fitted to it.
	if (tab->GetXSize() != laidOutWidth || tab->GetYSize() != laidOutHeight)
		ApplyLayout();

	// MpqLoaded can fire before this tab exists, so build on first draw too.
	if (!uniquesLoaded && Tables::isInitialized()) {
		StatDescriptions::Initialize();
		BuildUniques();
	}

	// The search box is edited by the user, so poll it for changes.
	if (searchBox->GetText() != lastBoxText) {
		lastBoxText = searchBox->GetText();
		query = ToLower(Trim(lastBoxText));
		list->SetScrollTop(0);
		needsRefresh = true;
	}

	if (needsRefresh) {
		ApplyFilter();
		PushRows();
		needsRefresh = false;
	}

	// Enter in the search box picks the first match rather than typing a newline,
	// which is what puts its summary up.
	if (searchBox->TakeSubmitted() && !matches.empty())
		list->SetSelectedRow(0);

	// Both of these follow the mouse and the scroll position, which move on the
	// input thread, so they are caught up here rather than where they change.
	UpdateStatus();
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

			// A page is a screenful less one row, so the row the user was
			// reading stays on screen to anchor where they have got to.
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
