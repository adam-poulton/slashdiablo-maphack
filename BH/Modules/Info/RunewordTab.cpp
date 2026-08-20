#include "RunewordTab.h"
#include <algorithm>
#include "../../BH.h"
#include "../../Common.h"
#include "../../MPQInit.h"
#include "../../MPQReader.h"
#include "../../TableReader.h"

using namespace Drawing;

// Layout, relative to the tab's content area.
#define RW_SEARCH_X			6
#define RW_SEARCH_Y			3
#define RW_SEARCH_WIDTH		626
#define RW_LIST_Y			25
#define RW_LIST_WIDTH		626
#define RW_LIST_HEIGHT		316
#define RW_DETAIL_Y			(RW_LIST_Y + RW_LIST_HEIGHT + 5)
#define RW_DETAIL_HEIGHT	68
#define RW_DETAIL_TEXT_X	(RW_SEARCH_X + 6)
#define RW_DETAIL_LINE_H	14
#define RW_FOOTER_Y			(RW_DETAIL_Y + RW_DETAIL_HEIGHT + 6)
#define RW_HINT_X			240
#define RW_PREV_X			500
#define RW_NEXT_X			560

// Column layout, relative to the list's left edge. The runes column is sized to
// hold the longest recipe ("Jah + Mal + Jah + Sur + Jah + Ber") without cutting
// it, since the runes are the point of the list.
#define RW_COL_NAME_X		0
#define RW_COL_NAME_W		150
#define RW_COL_RUNES_X		158
#define RW_COL_RUNES_W		290
#define RW_COL_TYPE_X		456
#define RW_COL_TYPE_W		170

// Six recipes shipped under working titles in runes.txt and were renamed before
// release; the files were never updated, so the readable name in "Rune Name" is
// stale. Keyed by the row's internal id, and only applied when the stale name is
// still what the file says, so a modified runes.txt is left alone.
struct RunewordRename {
	const char* id;
	const char* fileName;
	const char* releasedName;
};

static const RunewordRename kRenames[] = {
	{ "Runeword4",  "The Beast",     "Beast" },
	{ "Runeword14", "Bound by Duty", "Chains of Honor" },
	{ "Runeword26", "Doomsayer",     "Doom" },
	{ "Runeword37", "Exile's Path",  "Exile" },
	{ "Runeword47", "Widowmaker",    "Grief" },
	{ "Runeword99", "Winter",        "Voice of Reason" },
};

// Recipes the realm enables server-side without shipping them in runes.txt.
struct ExtraRuneword {
	const char* name;
	const char* runes[6];
	const char* itemType;
};

static const ExtraRuneword kExtraRunewords[] = {
	{ "Plague", { "r32", "r19", "r22" }, "weap" },	// Cham + Fal + Um
};

static std::string ToLower(const std::string& text) {
	std::string result(text);
	std::transform(result.begin(), result.end(), result.begin(), ::tolower);
	return result;
}

static std::string Join(const std::vector<std::string>& parts, const std::string& separator) {
	std::string result;
	for (unsigned int i = 0; i < parts.size(); i++) {
		if (i > 0)
			result += separator;
		result += parts[i];
	}
	return result;
}

// runes.txt stores the string table key in "Name" ("Runeword1") and the readable
// runeword name in "Rune Name" ("Ancient's Pledge"). Some copies of the file
// mark the latter as a comment column, so accept either spelling.
static std::string RunewordName(JSONObject* entry) {
	std::string id = Trim(entry->getString("Name"));
	std::string name;
	const char* fields[] = { "Rune Name", "*Rune Name" };
	for (int i = 0; i < 2 && name.length() == 0; i++)
		name = Trim(entry->getString(fields[i]));
	if (name.length() == 0)
		return id;

	for (unsigned int i = 0; i < (sizeof(kRenames) / sizeof(kRenames[0])); i++) {
		if (id.compare(kRenames[i].id) == 0 && name.compare(kRenames[i].fileName) == 0)
			return kRenames[i].releasedName;
	}
	return name;
}

// Rune item codes ("r14") come from runes.txt; the readable name comes from the
// item data already parsed out of the MPQ archives.
static std::string RuneName(const std::string& code) {
	std::map<std::string, ItemAttributes*>::iterator it = ItemAttributeMap.find(code);
	if (it != ItemAttributeMap.end() && it->second && it->second->name.length() > 0) {
		std::string name = it->second->name;
		// "El Rune" reads better as just "El" in a recipe list.
		const std::string suffix = " Rune";
		if (name.length() > suffix.length() &&
			name.compare(name.length() - suffix.length(), suffix.length(), suffix) == 0) {
			name.erase(name.length() - suffix.length());
		}
		return name;
	}
	return code;
}

// Item type codes ("armo") map to the descriptive name in ItemTypes.txt.
static std::string ItemTypeName(const std::string& code) {
	JSONObject* entry = Tables::ItemTypes.findEntry("Code", code);
	if (entry) {
		std::string name = Trim(entry->getString("ItemType"));
		if (name.length() > 0)
			return name;
	}
	return code;
}

RunewordTab::RunewordTab(UI* ui) : InfoTab("Runewords", ui),
	shownDetail(-2),
	recipesLoaded(false),
	needsRefresh(true) {

	searchBox = new Inputhook(tab, RW_SEARCH_X, RW_SEARCH_Y, RW_SEARCH_WIDTH, "");
	searchBox->SetPlaceholder("Click here and type to search by runeword name, rune or item type");

	list = new Listhook(tab, RW_SEARCH_X, RW_LIST_Y, RW_LIST_WIDTH, RW_LIST_HEIGHT);
	std::vector<ListColumn> columns;
	columns.push_back(ListColumn("Runeword", RW_COL_NAME_X, RW_COL_NAME_W, White));
	columns.push_back(ListColumn("Runes", RW_COL_RUNES_X, RW_COL_RUNES_W, Orange));
	columns.push_back(ListColumn("Item Types", RW_COL_TYPE_X, RW_COL_TYPE_W, Tan));
	list->SetColumns(columns);

	// The detail pane is always present; with nothing selected it explains how
	// to select something. Hooks belonging to a tab can't be hidden individually.
	Framehook* detailFrame = new Framehook(tab, RW_SEARCH_X, RW_DETAIL_Y, RW_LIST_WIDTH, RW_DETAIL_HEIGHT);
	detailFrame->SetTransparency(BTOneHalf);

	TextColor detailColors[4] = { Gold, Orange, Tan, White };
	for (int i = 0; i < 4; i++) {
		detailLines[i] = new Texthook(tab, RW_DETAIL_TEXT_X,
			RW_DETAIL_Y + 5 + (i * RW_DETAIL_LINE_H), "");
		detailLines[i]->SetColor(detailColors[i]);
	}

	statusText = new Texthook(tab, RW_SEARCH_X, RW_FOOTER_Y, "");
	statusText->SetColor(Grey);

	Texthook* hint = new Texthook(tab, RW_HINT_X, RW_FOOTER_Y, "PgUp / PgDn pages, Esc closes");
	hint->SetColor(Grey);

	Texthook* prev = new Texthook(tab, RW_PREV_X, RW_FOOTER_Y, "< Prev");
	prev->SetColor(Gold);
	prev->SetHoverColor(White);
	prev->SetLeftCallback(RunewordTab::OnPrevClick, this);

	Texthook* next = new Texthook(tab, RW_NEXT_X, RW_FOOTER_Y, "Next >");
	next->SetColor(Gold);
	next->SetHoverColor(White);
	next->SetLeftCallback(RunewordTab::OnNextClick, this);
}

void RunewordTab::MpqLoaded() {
	BuildRecipes();
}

// Rune level requirements come from misc.txt, which is already parsed and kept
// in memory; ItemAttributes only keeps the item's quality level, which is not
// the same number.
void RunewordTab::LoadRuneLevels() {
	if (!runeLevels.empty())
		return;
	std::map<std::string, MPQData*>::iterator data = MpqDataMap.find("misc");
	if (data == MpqDataMap.end() || !data->second)
		return;
	for (auto row = data->second->data.begin(); row != data->second->data.end(); row++) {
		std::string code = (*row)["code"];
		if (code.length() > 0 && (*row)["levelreq"].length() > 0)
			runeLevels[code] = atoi((*row)["levelreq"].c_str());
	}
}

void RunewordTab::BuildRecipes() {
	LoadRuneLevels();
	recipes.clear();
	matches.clear();

	// runes.txt keeps disabled recipes around as placeholders, so only list the
	// ones flagged complete. If nothing is flagged (a modified runes.txt), fall
	// back to every row that actually has runes assigned.
	for (int pass = 0; pass < 2 && recipes.empty(); pass++) {
		bool requireComplete = (pass == 0);
		for (int i = 0; i < Tables::Runewords.size(); i++) {
			JSONObject* entry = Tables::Runewords.entryAt(i);
			if (!entry)
				continue;
			if (requireComplete && entry->getString("complete").compare("1") != 0)
				continue;

			std::vector<std::string> runes;
			int requiredLevel = 0;
			for (int n = 1; n <= 6; n++) {
				std::string code = Trim(entry->getString("Rune" + std::to_string(n)));
				if (code.length() == 0)
					continue;
				runes.push_back(RuneName(code));
				if (runeLevels.count(code) && runeLevels[code] > requiredLevel)
					requiredLevel = runeLevels[code];
			}
			if (runes.empty())
				continue;

			RunewordRecipe recipe;
			recipe.name = RunewordName(entry);
			if (recipe.name.length() == 0)
				continue;
			recipe.requiredLevel = requiredLevel;
			recipe.sockets = runes.size();
			recipe.runes = Join(runes, " + ");

			std::vector<std::string> types;
			for (int n = 1; n <= 6; n++) {
				std::string code = Trim(entry->getString("itype" + std::to_string(n)));
				if (code.length() > 0)
					types.push_back(ItemTypeName(code));
			}
			recipe.itemTypes = Join(types, ", ");

			std::vector<std::string> excluded;
			for (int n = 1; n <= 3; n++) {
				std::string code = Trim(entry->getString("etype" + std::to_string(n)));
				if (code.length() > 0)
					excluded.push_back(ItemTypeName(code));
			}
			if (!excluded.empty())
				recipe.itemTypes += " (not " + Join(excluded, ", ") + ")";

			recipe.searchKey = ToLower(recipe.name + " " + recipe.runes + " " + recipe.itemTypes);
			recipes.push_back(recipe);
		}
	}

	for (unsigned int i = 0; i < (sizeof(kExtraRunewords) / sizeof(kExtraRunewords[0])); i++) {
		const ExtraRuneword& extra = kExtraRunewords[i];

		// Skip it if the realm has since added it to runes.txt.
		bool known = false;
		for (unsigned int n = 0; n < recipes.size() && !known; n++)
			known = (recipes[n].name.compare(extra.name) == 0);
		if (known)
			continue;

		std::vector<std::string> runes;
		int requiredLevel = 0;
		for (int n = 0; n < 6 && extra.runes[n]; n++) {
			runes.push_back(RuneName(extra.runes[n]));
			if (runeLevels.count(extra.runes[n]) && runeLevels[extra.runes[n]] > requiredLevel)
				requiredLevel = runeLevels[extra.runes[n]];
		}

		RunewordRecipe recipe;
		recipe.name = extra.name;
		recipe.requiredLevel = requiredLevel;
		recipe.sockets = runes.size();
		recipe.runes = Join(runes, " + ");
		recipe.itemTypes = ItemTypeName(extra.itemType);
		recipe.searchKey = ToLower(recipe.name + " " + recipe.runes + " " + recipe.itemTypes);
		recipes.push_back(recipe);
	}

	std::sort(recipes.begin(), recipes.end(), [](const RunewordRecipe& a, const RunewordRecipe& b) {
		return ToLower(a.name) < ToLower(b.name);
	});

	recipesLoaded = true;
	needsRefresh = true;
}

void RunewordTab::ApplyFilter() {
	matches.clear();
	for (unsigned int i = 0; i < recipes.size(); i++) {
		if (query.empty() || recipes[i].searchKey.find(query) != std::string::npos)
			matches.push_back(&recipes[i]);
	}
}

void RunewordTab::PushRows() {
	std::vector<std::vector<std::string>> rows;
	rows.reserve(matches.size());
	for (unsigned int i = 0; i < matches.size(); i++) {
		std::vector<std::string> row;
		row.push_back(matches[i]->name);
		row.push_back(matches[i]->runes);
		row.push_back(matches[i]->itemTypes);
		rows.push_back(row);
	}
	list->SetRows(rows);	// also clears the selection

	if (!recipesLoaded) {
		statusText->SetText("Waiting for game data to finish loading...");
	} else if (matches.empty()) {
		statusText->SetText("No runewords match \"%s\"", query.c_str());
	} else {
		statusText->SetText("%u - %u of %u   (page %u of %u)",
			list->GetFirstVisibleRow() + 1,
			list->GetLastVisibleRow(),
			(unsigned int)matches.size(),
			list->GetPage() + 1,
			list->GetPageCount());
	}
}

void RunewordTab::PushDetail() {
	int selected = list->GetSelectedRow();
	shownDetail = selected;

	if (selected < 0 || selected >= (int)matches.size()) {
		detailLines[0]->SetText("Click a runeword for details, or press enter to open the first match");
		detailLines[0]->SetColor(Grey);
		for (int i = 1; i < 4; i++)
			detailLines[i]->SetText("");
		return;
	}

	const RunewordRecipe* recipe = matches[selected];
	detailLines[0]->SetColor(Gold);
	detailLines[0]->SetText("%s", recipe->name.c_str());
	detailLines[1]->SetText("%s", recipe->runes.c_str());
	detailLines[2]->SetText("Bases: %s", recipe->itemTypes.c_str());
	if (recipe->requiredLevel > 0) {
		detailLines[3]->SetText("%u sockets, required level %d",
			recipe->sockets, recipe->requiredLevel);
	} else {
		detailLines[3]->SetText("%u sockets", recipe->sockets);
	}
}

void RunewordTab::Search(const std::string& text) {
	std::string trimmed = Trim(text);
	query = ToLower(trimmed);
	searchBox->SetText("%s", trimmed.c_str());
	searchBox->SetTextPos(0);
	searchBox->ResetSelection();
	searchBox->SetCursorPosition(searchBox->GetText().length());
	lastBoxText = searchBox->GetText();
	list->SetPage(0);
	needsRefresh = true;
}

void RunewordTab::OnDraw() {
	// MpqLoaded can fire before this tab exists, so build on first draw too.
	if (!recipesLoaded && Tables::isInitialized())
		BuildRecipes();

	// The search box is edited by the user, so poll it for changes.
	if (searchBox->GetText() != lastBoxText) {
		lastBoxText = searchBox->GetText();
		query = ToLower(Trim(lastBoxText));
		list->SetPage(0);
		needsRefresh = true;
	}

	// Enter opens the first match rather than typing a newline into the box.
	if (searchBox->TakeSubmitted()) {
		if (needsRefresh) {
			ApplyFilter();
			PushRows();
			needsRefresh = false;
		}
		list->SetSelectedRow(matches.empty() ? -1 : 0);
	}

	if (needsRefresh) {
		ApplyFilter();
		PushRows();
		needsRefresh = false;
	}

	if (list->GetSelectedRow() != shownDetail)
		PushDetail();
}

bool RunewordTab::OnKey(bool up, BYTE key) {
	switch (key) {
		case VK_ESCAPE:
			// Back out of the detail pane first, so escape doesn't jump straight
			// to closing the window while a runeword is open.
			if (list->GetSelectedRow() < 0)
				return false;
			if (up)
				list->ClearSelection();
			return true;
		case VK_PRIOR:
			if (!up) {
				list->ChangePage(-1);
				needsRefresh = true;
			}
			return true;
		case VK_NEXT:
			if (!up) {
				list->ChangePage(1);
				needsRefresh = true;
			}
			return true;
	}
	return false;
}

bool __cdecl RunewordTab::OnPrevClick(bool up, Hook* hook, void* data) {
	if (up) {
		RunewordTab* self = (RunewordTab*)data;
		self->list->ChangePage(-1);
		self->needsRefresh = true;
	}
	return true;
}

bool __cdecl RunewordTab::OnNextClick(bool up, Hook* hook, void* data) {
	if (up) {
		RunewordTab* self = (RunewordTab*)data;
		self->list->ChangePage(1);
		self->needsRefresh = true;
	}
	return true;
}
