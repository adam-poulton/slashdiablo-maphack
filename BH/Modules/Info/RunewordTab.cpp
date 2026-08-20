#include "RunewordTab.h"
#include <algorithm>
#include "../../BH.h"
#include "../../Common.h"
#include "../../MPQInit.h"
#include "../../MPQReader.h"
#include "../../StatDescriptions.h"
#include "../../TableReader.h"

using namespace Drawing;

// Layout, relative to the tab's content area.
#define RW_SEARCH_X			6
#define RW_SEARCH_Y			3
#define RW_SEARCH_WIDTH		626
#define RW_LIST_Y			25
#define RW_LIST_WIDTH		626
#define RW_LIST_HEIGHT		399
#define RW_FOOTER_Y			(RW_LIST_Y + RW_LIST_HEIGHT + 6)
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

// The detail view replaces the list, so it gets the same area. Stats run long
// once the runes are included, so they are listed in two columns.
#define RW_DETAIL_TITLE_Y	RW_LIST_Y
#define RW_DETAIL_SUM_Y		(RW_LIST_Y + 15)
#define RW_DETAIL_BODY_Y	(RW_LIST_Y + 36)
#define RW_DETAIL_LINE_H	12
#define RW_DETAIL_COL_W		316
#define RW_DETAIL_ROWS		(RW_DETAIL_LINES / 2)

// Six recipes shipped under working titles in runes.txt and were renamed before
// release; the files were never updated, so the readable name in "Rune Name" is
// stale. Keyed by the row's internal id, and only applied when the stale name is
// still what the file says, so a modified runes.txt is left alone. The string
// table is preferred over both when it has an entry.
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

// Which set of rune bonuses a base takes. gems.txt gives every rune three sets,
// one for weapons, one for helms and body armour and one for shields, which is
// why the same runeword rolls differently depending on what it is made in.
static const char* kSlotWeapon = "weapon";
static const char* kSlotHelm = "helm";
static const char* kSlotShield = "shield";

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
	std::string localized = StatDescriptions::GetString(id);
	if (localized.length() > 0)
		return localized;

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

// Walks the Equiv chain in ItemTypes.txt up to the root categories to work out
// which of the three rune bonus sets a base takes.
static const char* BaseSlot(const std::string& code) {
	std::string current = code;
	for (int depth = 0; depth < 12 && current.length() > 0; depth++) {
		if (current.compare("shld") == 0)
			return kSlotShield;
		if (current.compare("weap") == 0)
			return kSlotWeapon;
		if (current.compare("armo") == 0 || current.compare("tors") == 0 ||
			current.compare("helm") == 0)
			return kSlotHelm;
		JSONObject* entry = Tables::ItemTypes.findEntry("Code", current);
		if (!entry)
			break;
		current = Trim(entry->getString("Equiv1"));
	}
	// Anything that isn't clearly armour takes the weapon bonuses, which is what
	// the game does with the leftover types runewords are allowed in.
	return kSlotWeapon;
}

RunewordTab::RunewordTab(UI* ui) : InfoTab("Runewords", ui),
	shownDetail(-1),
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

	statusText = new Texthook(tab, RW_SEARCH_X, RW_FOOTER_Y, "");
	statusText->SetColor(Grey);

	listHint = new Texthook(tab, RW_HINT_X, RW_FOOTER_Y,
		"Click a runeword or press enter for its stats");
	listHint->SetColor(Grey);

	prevLink = new Texthook(tab, RW_PREV_X, RW_FOOTER_Y, "< Prev");
	prevLink->SetColor(Gold);
	prevLink->SetHoverColor(White);
	prevLink->SetLeftCallback(RunewordTab::OnPrevClick, this);

	nextLink = new Texthook(tab, RW_NEXT_X, RW_FOOTER_Y, "Next >");
	nextLink->SetColor(Gold);
	nextLink->SetHoverColor(White);
	nextLink->SetLeftCallback(RunewordTab::OnNextClick, this);

	detailTitle = new Texthook(tab, RW_SEARCH_X, RW_DETAIL_TITLE_Y, "");
	detailTitle->SetColor(Gold);
	detailSummary = new Texthook(tab, RW_SEARCH_X, RW_DETAIL_SUM_Y, "");
	detailSummary->SetColor(Orange);
	for (int i = 0; i < RW_DETAIL_LINES; i++) {
		unsigned int column = (i < RW_DETAIL_ROWS) ? 0 : 1;
		unsigned int row = i % RW_DETAIL_ROWS;
		detailLines[i] = new Texthook(tab,
			RW_SEARCH_X + (column * RW_DETAIL_COL_W),
			RW_DETAIL_BODY_Y + (row * RW_DETAIL_LINE_H), "");
		detailLines[i]->SetColor(White);
	}

	backLink = new Texthook(tab, RW_SEARCH_X, RW_FOOTER_Y, "< Back to the list");
	backLink->SetColor(Gold);
	backLink->SetHoverColor(White);
	backLink->SetLeftCallback(RunewordTab::OnBackClick, this);

	ApplyViewVisibility();
}

// Only one of the two views is shown at a time; the hooks belonging to the other
// are switched off so they neither draw nor take clicks.
void RunewordTab::ApplyViewVisibility() {
	bool detail = (shownDetail >= 0);

	list->SetActive(!detail);
	statusText->SetActive(!detail);
	listHint->SetActive(!detail);
	prevLink->SetActive(!detail);
	nextLink->SetActive(!detail);

	detailTitle->SetActive(detail);
	detailSummary->SetActive(detail);
	for (int i = 0; i < RW_DETAIL_LINES; i++)
		detailLines[i]->SetActive(detail);
	backLink->SetActive(detail);
}

void RunewordTab::MpqLoaded() {
	StatDescriptions::Initialize();
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

			RunewordRecipe recipe;
			recipe.statsLoaded = false;
			recipe.requiredLevel = 0;

			std::vector<std::string> runeNames;
			for (int n = 1; n <= 6; n++) {
				std::string code = Trim(entry->getString("Rune" + std::to_string(n)));
				if (code.length() == 0)
					continue;
				recipe.runeCodes.push_back(code);
				runeNames.push_back(RuneName(code));
				if (runeLevels.count(code) && runeLevels[code] > recipe.requiredLevel)
					recipe.requiredLevel = runeLevels[code];
			}
			if (runeNames.empty())
				continue;

			recipe.name = RunewordName(entry);
			if (recipe.name.length() == 0)
				continue;
			recipe.sockets = runeNames.size();
			recipe.runes = Join(runeNames, " + ");

			std::vector<std::string> types;
			for (int n = 1; n <= 6; n++) {
				std::string code = Trim(entry->getString("itype" + std::to_string(n)));
				if (code.length() == 0)
					continue;
				types.push_back(ItemTypeName(code));

				// One block of rune bonuses per distinct kind of base.
				const char* slot = BaseSlot(code);
				bool seen = false;
				for (unsigned int s = 0; s < recipe.baseSlots.size() && !seen; s++)
					seen = (recipe.baseSlots[s].compare(slot) == 0);
				if (!seen) {
					recipe.baseSlots.push_back(slot);
					recipe.baseLabels.push_back(types.back());
				}
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

			for (int n = 1; n <= 7; n++) {
				std::string index = std::to_string(n);
				RunewordProperty property;
				property.code = Trim(entry->getString("T1Code" + index));
				if (property.code.length() == 0)
					continue;
				property.param = Trim(entry->getString("T1Param" + index));
				property.min = atoi(entry->getString("T1Min" + index).c_str());
				property.max = atoi(entry->getString("T1Max" + index).c_str());
				recipe.properties.push_back(property);
			}

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

		RunewordRecipe recipe;
		recipe.statsLoaded = false;
		recipe.requiredLevel = 0;
		recipe.name = extra.name;

		std::vector<std::string> runeNames;
		for (int n = 0; n < 6 && extra.runes[n]; n++) {
			recipe.runeCodes.push_back(extra.runes[n]);
			runeNames.push_back(RuneName(extra.runes[n]));
			if (runeLevels.count(extra.runes[n]) && runeLevels[extra.runes[n]] > recipe.requiredLevel)
				recipe.requiredLevel = runeLevels[extra.runes[n]];
		}
		recipe.sockets = runeNames.size();
		recipe.runes = Join(runeNames, " + ");
		recipe.itemTypes = ItemTypeName(extra.itemType);
		recipe.baseSlots.push_back(BaseSlot(extra.itemType));
		recipe.baseLabels.push_back(recipe.itemTypes);
		recipe.searchKey = ToLower(recipe.name + " " + recipe.runes + " " + recipe.itemTypes);
		recipes.push_back(recipe);
	}

	std::sort(recipes.begin(), recipes.end(), [](const RunewordRecipe& a, const RunewordRecipe& b) {
		return ToLower(a.name) < ToLower(b.name);
	});

	recipesLoaded = true;
	needsRefresh = true;
}

// Renders a recipe's stats the first time it is looked at: the runeword's own
// bonuses, then what its runes add for each kind of base it allows.
void RunewordTab::LoadStats(RunewordRecipe* recipe) {
	if (recipe->statsLoaded)
		return;
	recipe->statsLoaded = true;
	StatDescriptions::Initialize();

	RunewordStatBlock own;
	for (unsigned int i = 0; i < recipe->properties.size(); i++) {
		const RunewordProperty& property = recipe->properties[i];
		StatDescriptions::DescribeProperty(property.code, property.param,
			property.min, property.max, own.lines);
	}
	if (!own.lines.empty())
		recipe->stats.push_back(own);

	for (unsigned int s = 0; s < recipe->baseSlots.size(); s++) {
		RunewordStatBlock block;
		block.heading = "From the runes, in " + recipe->baseLabels[s] + ":";
		for (unsigned int r = 0; r < recipe->runeCodes.size(); r++) {
			JSONObject* gem = Tables::Gems.findEntry("code", recipe->runeCodes[r]);
			if (!gem)
				continue;
			for (int n = 1; n <= 3; n++) {
				std::string prefix = recipe->baseSlots[s] + "Mod" + std::to_string(n);
				std::string code = Trim(gem->getString(prefix + "Code"));
				if (code.length() == 0)
					continue;
				StatDescriptions::DescribeProperty(code,
					Trim(gem->getString(prefix + "Param")),
					atoi(gem->getString(prefix + "Min").c_str()),
					atoi(gem->getString(prefix + "Max").c_str()),
					block.lines);
			}
		}
		if (!block.lines.empty())
			recipe->stats.push_back(block);
	}
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

void RunewordTab::ShowDetail(int match) {
	if (match < 0 || match >= (int)matches.size())
		return;

	// The recipe list owns the recipes; matches only points into it.
	RunewordRecipe* recipe = const_cast<RunewordRecipe*>(matches[match]);
	LoadStats(recipe);
	shownDetail = match;

	detailTitle->SetText("%s", recipe->name.c_str());

	char summary[256];
	if (recipe->requiredLevel > 0) {
		sprintf_s(summary, "%s     %u sockets, level %d     in %s",
			recipe->runes.c_str(), recipe->sockets, recipe->requiredLevel,
			recipe->itemTypes.c_str());
	} else {
		sprintf_s(summary, "%s     %u sockets     in %s",
			recipe->runes.c_str(), recipe->sockets, recipe->itemTypes.c_str());
	}
	detailSummary->SetText("%s", summary);

	int line = 0;
	for (unsigned int b = 0; b < recipe->stats.size() && line < RW_DETAIL_LINES; b++) {
		const RunewordStatBlock& block = recipe->stats[b];
		if (block.heading.length() > 0) {
			// Don't leave a heading stranded at the foot of a column.
			if (line > 0 && (line % RW_DETAIL_ROWS) == RW_DETAIL_ROWS - 1)
				line++;
			if (line >= RW_DETAIL_LINES)
				break;
			detailLines[line]->SetColor(Gold);
			detailLines[line]->SetText("%s", block.heading.c_str());
			line++;
		}
		for (unsigned int i = 0; i < block.lines.size() && line < RW_DETAIL_LINES; i++, line++) {
			detailLines[line]->SetColor(block.heading.length() > 0 ? Tan : White);
			detailLines[line]->SetText("%s", block.lines[i].c_str());
		}
	}
	for (; line < RW_DETAIL_LINES; line++)
		detailLines[line]->SetText("");

	ApplyViewVisibility();
}

void RunewordTab::ShowList() {
	shownDetail = -1;
	list->ClearSelection();
	ApplyViewVisibility();
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
	ShowList();
}

void RunewordTab::OnDraw() {
	// MpqLoaded can fire before this tab exists, so build on first draw too.
	if (!recipesLoaded && Tables::isInitialized()) {
		StatDescriptions::Initialize();
		BuildRecipes();
	}

	// The search box is edited by the user, so poll it for changes. Typing goes
	// back to the list, since the results have changed under it.
	if (searchBox->GetText() != lastBoxText) {
		lastBoxText = searchBox->GetText();
		query = ToLower(Trim(lastBoxText));
		list->SetPage(0);
		needsRefresh = true;
		ShowList();
	}

	if (needsRefresh) {
		ApplyFilter();
		PushRows();
		needsRefresh = false;
	}

	// Enter opens the first match rather than typing a newline into the box.
	if (searchBox->TakeSubmitted() && !matches.empty())
		ShowDetail(0);

	int selected = list->GetSelectedRow();
	if (selected >= 0 && selected != shownDetail)
		ShowDetail(selected);
}

bool RunewordTab::OnKey(bool up, BYTE key) {
	switch (key) {
		case VK_ESCAPE:
			// Back out of the detail view first, so escape doesn't jump straight
			// to closing the window while a runeword is open.
			if (shownDetail < 0)
				return false;
			if (up)
				ShowList();
			return true;
		case VK_PRIOR:
			if (shownDetail >= 0)
				return false;
			if (!up) {
				list->ChangePage(-1);
				needsRefresh = true;
			}
			return true;
		case VK_NEXT:
			if (shownDetail >= 0)
				return false;
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

bool __cdecl RunewordTab::OnBackClick(bool up, Hook* hook, void* data) {
	if (up)
		((RunewordTab*)data)->ShowList();
	return true;
}
