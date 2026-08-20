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
#define RW_SEARCH_WIDTH		388
#define RW_LIST_Y			25
#define RW_LIST_WIDTH		388
#define RW_LIST_HEIGHT		399
#define RW_FOOTER_Y			(RW_LIST_Y + RW_LIST_HEIGHT + 6)
#define RW_PREV_X			250
#define RW_NEXT_X			310

// Column layout, relative to the list's left edge. The runes column is sized to
// hold the longest recipe ("Jah + Mal + Jah + Sur + Jah + Ber") without cutting
// it, since the runes are the point of the list.
#define RW_COL_NAME_X		0
#define RW_COL_NAME_W		140
#define RW_COL_RUNES_X		148
#define RW_COL_RUNES_W		240

// The detail view replaces the list. Its text is centred inside a border sized
// to hold it, so it reads like the description on the item itself.
#define RW_DETAIL_PAD		7
#define RW_DETAIL_LINE_H	12
#define RW_DETAIL_MAX_W		(RW_LIST_WIDTH - (2 * RW_DETAIL_PAD))

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

// Splits text over as many lines as it takes to fit the given width. Measuring
// uses the game's font routines, so this belongs on the drawing thread.
static void WrapText(const std::string& text, unsigned int font,
		unsigned int maxWidth, std::vector<std::string>& lines) {
	if (text.length() == 0 || (unsigned int)Texthook::GetTextSize(text, font).x <= maxWidth) {
		lines.push_back(text);
		return;
	}

	std::string line;
	size_t pos = 0;
	while (pos < text.length()) {
		size_t space = text.find(' ', pos);
		std::string word = (space == std::string::npos) ?
			text.substr(pos) : text.substr(pos, space - pos);
		pos = (space == std::string::npos) ? text.length() : space + 1;
		if (word.length() == 0)
			continue;	// runs of spaces

		std::string candidate = line.length() ? (line + " " + word) : word;
		if (line.length() > 0 &&
			(unsigned int)Texthook::GetTextSize(candidate, font).x > maxWidth) {
			lines.push_back(line);
			line = word;
		} else {
			line = candidate;
		}
	}
	if (line.length() > 0)
		lines.push_back(line);
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
	searchBox->SetPlaceholder("Search by runeword name, rune or item type");

	list = new Listhook(tab, RW_SEARCH_X, RW_LIST_Y, RW_LIST_WIDTH, RW_LIST_HEIGHT);
	std::vector<ListColumn> columns;
	columns.push_back(ListColumn("Runeword", RW_COL_NAME_X, RW_COL_NAME_W, White));
	columns.push_back(ListColumn("Runes", RW_COL_RUNES_X, RW_COL_RUNES_W, Orange));
	list->SetColumns(columns);

	statusText = new Texthook(tab, RW_SEARCH_X, RW_FOOTER_Y, "");
	statusText->SetColor(Grey);

	prevLink = new Texthook(tab, RW_PREV_X, RW_FOOTER_Y, "< Prev");
	prevLink->SetColor(Gold);
	prevLink->SetHoverColor(White);
	prevLink->SetLeftCallback(RunewordTab::OnPrevClick, this);

	nextLink = new Texthook(tab, RW_NEXT_X, RW_FOOTER_Y, "Next >");
	nextLink->SetColor(Gold);
	nextLink->SetHoverColor(White);
	nextLink->SetLeftCallback(RunewordTab::OnNextClick, this);

	// Created before the text so the border draws behind it. Both are positioned
	// and sized when a runeword is opened.
	detailFrame = new Framehook(tab, RW_SEARCH_X, RW_LIST_Y, RW_LIST_WIDTH, 0);
	detailFrame->SetTransparency(BTOneHalf);
	for (int i = 0; i < RW_DETAIL_LINES; i++) {
		detailLines[i] = new Texthook(tab, RW_SEARCH_X, RW_LIST_Y, "");
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
	prevLink->SetActive(!detail);
	nextLink->SetActive(!detail);

	detailFrame->SetActive(detail);
	// The lines themselves are switched on individually by ShowDetail(), so that
	// only the ones holding text are drawn.
	if (!detail) {
		for (int i = 0; i < RW_DETAIL_LINES; i++)
			detailLines[i]->SetActive(false);
	}
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

// Renders a recipe's stats the first time it is looked at.
//
// The finished item's stats are the runeword's own bonuses plus what each rune
// adds, and the game adds equal stats together rather than listing them twice,
// so the same is done here before rendering. Runes give different bonuses in a
// weapon, a helm or body armour, and a shield, so this is worked out once per
// kind of base the runeword allows: lines that come out the same whatever it is
// made in are listed plainly, and only the ones that differ say which base they
// belong to.
void RunewordTab::LoadStats(RunewordRecipe* recipe) {
	if (recipe->statsLoaded)
		return;
	recipe->statsLoaded = true;
	StatDescriptions::Initialize();

	std::vector<StatDescriptions::Stat> own;
	for (unsigned int i = 0; i < recipe->properties.size(); i++) {
		const RunewordProperty& property = recipe->properties[i];
		StatDescriptions::CollectProperty(property.code, property.param,
			property.min, property.max, own);
	}

	// One rendered list per kind of base, each already added up.
	std::vector<std::vector<std::string>> perBase;
	for (unsigned int s = 0; s < recipe->baseSlots.size(); s++) {
		std::vector<StatDescriptions::Stat> stats = own;
		for (unsigned int r = 0; r < recipe->runeCodes.size(); r++) {
			JSONObject* gem = Tables::Gems.findEntry("code", recipe->runeCodes[r]);
			if (!gem)
				continue;
			for (int n = 1; n <= 3; n++) {
				std::string prefix = recipe->baseSlots[s] + "Mod" + std::to_string(n);
				std::string code = Trim(gem->getString(prefix + "Code"));
				if (code.length() == 0)
					continue;
				StatDescriptions::CollectProperty(code,
					Trim(gem->getString(prefix + "Param")),
					atoi(gem->getString(prefix + "Min").c_str()),
					atoi(gem->getString(prefix + "Max").c_str()),
					stats);
			}
		}
		StatDescriptions::MergeStats(stats);

		std::vector<std::string> lines;
		for (unsigned int i = 0; i < stats.size(); i++) {
			std::string line = StatDescriptions::Render(stats[i]);
			if (line.length() > 0)
				lines.push_back(line);
		}
		perBase.push_back(lines);
	}

	if (perBase.empty()) {
		StatDescriptions::MergeStats(own);
		for (unsigned int i = 0; i < own.size(); i++) {
			std::string line = StatDescriptions::Render(own[i]);
			if (line.length() > 0)
				recipe->stats.push_back(line);
		}
		return;
	}

	// Lines every base has in common need no explanation; the rest are tagged
	// with the base they apply to.
	for (unsigned int i = 0; i < perBase[0].size(); i++) {
		bool everywhere = true;
		for (unsigned int b = 1; b < perBase.size() && everywhere; b++) {
			everywhere = std::find(perBase[b].begin(), perBase[b].end(),
				perBase[0][i]) != perBase[b].end();
		}
		if (everywhere)
			recipe->stats.push_back(perBase[0][i]);
	}
	for (unsigned int b = 0; b < perBase.size(); b++) {
		for (unsigned int i = 0; i < perBase[b].size(); i++) {
			bool common = std::find(recipe->stats.begin(), recipe->stats.end(),
				perBase[b][i]) != recipe->stats.end();
			if (common)
				continue;
			recipe->stats.push_back(perBase[b][i] + "  (" + recipe->baseLabels[b] + ")");
		}
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

	// Built the way the game describes an item: what it is, then what it needs,
	// then what it does.
	unsigned int font = detailLines[0]->GetFont();
	std::vector<std::string> lines;
	std::vector<TextColor> colors;

	lines.push_back(recipe->name);
	colors.push_back(Gold);
	lines.push_back(recipe->runes);
	colors.push_back(Orange);
	if (recipe->requiredLevel > 0) {
		char required[64];
		sprintf_s(required, "Required level: %d", recipe->requiredLevel);
		lines.push_back(required);
		colors.push_back(White);
	}
	lines.push_back(recipe->itemTypes);
	colors.push_back(White);
	lines.push_back("");
	colors.push_back(White);

	for (unsigned int i = 0; i < recipe->stats.size(); i++) {
		std::vector<std::string> wrapped;
		WrapText(recipe->stats[i], font, RW_DETAIL_MAX_W, wrapped);
		for (unsigned int w = 0; w < wrapped.size() && lines.size() < RW_DETAIL_LINES; w++) {
			lines.push_back(wrapped[w]);
			colors.push_back(White);
		}
	}

	// The border is sized to the text it holds and everything is centred inside
	// it, so the block stays in the middle of the panel however long it runs.
	unsigned int widest = 0;
	for (unsigned int i = 0; i < lines.size(); i++) {
		unsigned int width = (unsigned int)Texthook::GetTextSize(lines[i], font).x;
		if (width > widest)
			widest = width;
	}
	unsigned int boxWidth = widest + (2 * RW_DETAIL_PAD);
	unsigned int boxX = RW_SEARCH_X + ((RW_LIST_WIDTH - boxWidth) / 2);
	detailFrame->SetBaseX(boxX);
	detailFrame->SetXSize(boxWidth);
	detailFrame->SetYSize((unsigned int)(lines.size() * RW_DETAIL_LINE_H) + (2 * RW_DETAIL_PAD));

	int line = 0;
	for (; line < (int)lines.size(); line++) {
		unsigned int width = (unsigned int)Texthook::GetTextSize(lines[line], font).x;
		detailLines[line]->SetBaseX(boxX + ((boxWidth - width) / 2));
		detailLines[line]->SetBaseY(RW_LIST_Y + RW_DETAIL_PAD + (line * RW_DETAIL_LINE_H));
		detailLines[line]->SetColor(colors[line]);
		detailLines[line]->SetText("%s", lines[line].c_str());
		detailLines[line]->SetActive(true);
	}
	for (; line < RW_DETAIL_LINES; line++) {
		detailLines[line]->SetText("");
		detailLines[line]->SetActive(false);
	}

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
