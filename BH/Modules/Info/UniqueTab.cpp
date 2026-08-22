#include "UniqueTab.h"
#include <algorithm>
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
#define UQ_MARGIN			6	// down either side, and below the status line
#define UQ_SEARCH_Y			3
#define UQ_SEARCH_GAP		7	// between the search box and the list
#define UQ_FOOTER_GAP		6	// between the list and the status line
#define UQ_FOOTER_HEIGHT	8	// the status line itself

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

UniqueTab::UniqueTab(UI* ui) : InfoTab("Uniques", ui),
	shownSummary(-1),
	uniquesLoaded(false),
	needsRefresh(true) {

	searchBox = new Inputhook(tab, UQ_MARGIN, UQ_SEARCH_Y, 0, "");
	searchBox->SetPlaceholder("Search by unique name, base item or item type");
	searchBox->SetClearOnFocus(true);

	list = new Listhook(tab, UQ_MARGIN, 0, 0, 0);
	// Both columns name the same item, so they share its colour.
	TextColor unique = RarityColor(RarityUnique);
	std::vector<ListColumn> columns;
	columns.push_back(ListColumn("", 0, UQ_COL_NAME_WEIGHT, 0, unique, White));
	columns.push_back(ListColumn("", 0, UQ_COL_BASE_WEIGHT, UQ_COL_GAP, unique, White));
	list->SetColumns(columns);

	statusText = new Texthook(tab, UQ_MARGIN, 0, "");
	statusText->SetColor(Grey);

	// Placed and switched on by UpdateSummary().
	summary = new Tooltiphook(InGame, 0, 0);
	summary->SetActive(false);

	ApplyLayout();
}

// The list takes whatever height is left between the search box and the status
// line, so a resize needs nothing but this.
void UniqueTab::ApplyLayout() {
	laidOutWidth = tab->GetXSize();
	laidOutHeight = tab->GetYSize();

	unsigned int contentWidth = (laidOutWidth > 2 * UQ_MARGIN) ?
		(laidOutWidth - (2 * UQ_MARGIN)) : 0;

	// Measured off the box rather than guessed, since its height follows its font.
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
			unique.name = UniqueName(entry);
			if (unique.name.length() == 0)
				continue;

			unique.baseName = ItemName(code);
			unique.requiredLevel = atoi(entry->getString("lvl req").c_str());

			// What makes a search for "amulet" work; the base's name rarely says.
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
	for (unsigned int i = 0; i < unique->properties.size(); i++) {
		const UniqueProperty& property = unique->properties[i];
		StatDescriptions::CollectProperty(property.code, property.param,
			property.min, property.max, stats);
	}
	unique->stats = StatDescriptions::BuildLines(stats);
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
	UpdateStatus();
}

// Follows the scroll position as well as the rows, so it is refreshed per frame.
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

// Ordered the way the game describes an item. The panel wraps and sizes itself
// to whatever it is handed.
void UniqueTab::BuildSummaryLines(UniqueRecord* unique,
		std::vector<TooltipLine>& lines) {
	TextColor color = RarityColor(RarityUnique);
	lines.push_back(TooltipLine(unique->name, color));
	lines.push_back(TooltipLine(unique->baseName, color));
	if (unique->requiredLevel > 0) {
		char required[64];
		sprintf_s(required, "Required level: %d", unique->requiredLevel);
		lines.push_back(TooltipLine(required, White));
	}
	lines.push_back(TooltipLine("", White));

	for (unsigned int i = 0; i < unique->stats.size(); i++)
		lines.push_back(TooltipLine(unique->stats[i], White));
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

		std::vector<TooltipLine> lines;
		BuildSummaryLines(unique, lines);
		summary->SetLines(lines);
		shownSummary = row;
	}

	// Must follow SetLines(): where it fits depends on how big it turned out.
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

// The caret goes straight in the search box. A search that arrived with the
// window, from the chat command, is left alone: the box only clears on a click.
void UniqueTab::OnOpen() {
	searchBox->SetCursorPosition(searchBox->GetText().length());
	searchBox->SetFocused(true);
}

void UniqueTab::OnClose() {
	searchBox->SetFocused(false);
	summary->SetActive(false);
	shownSummary = -1;
	Search("");
}

void UniqueTab::OnDraw() {
	if (tab->GetXSize() != laidOutWidth || tab->GetYSize() != laidOutHeight)
		ApplyLayout();

	// MpqLoaded can fire before this tab exists.
	if (!uniquesLoaded && Tables::isInitialized()) {
		StatDescriptions::Initialize();
		BuildUniques();
	}

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

	// Enter picks the first match rather than typing a newline.
	if (searchBox->TakeSubmitted() && !matches.empty())
		list->SetSelectedRow(0);

	// The mouse and the scroll position move on the input thread, so catch up here.
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
