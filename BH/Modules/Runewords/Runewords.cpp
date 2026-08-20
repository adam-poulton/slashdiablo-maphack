#include "Runewords.h"
#include <algorithm>
#include "../../BH.h"
#include "../../Common.h"
#include "../../D2Helpers.h"
#include "../../MPQInit.h"
#include "../../TableReader.h"

using namespace Drawing;

// Lookup window geometry. The list is laid out as three text columns per row,
// positioned relative to the window's tab area.
#define RW_WINDOW_WIDTH		560
#define RW_WINDOW_HEIGHT	420
#define RW_ROWS				28
#define RW_ROW_HEIGHT		12
#define RW_FIRST_ROW_Y		32
#define RW_HEADER_Y			18
#define RW_FOOTER_Y			(RW_FIRST_ROW_Y + (RW_ROWS * RW_ROW_HEIGHT) + 2)
#define RW_COL_NAME_X		6
#define RW_COL_RUNES_X		144
#define RW_COL_TYPE_X		364

// Rough character budgets per column, to keep long entries from spilling into
// the next one. The widest vanilla entries are 19 characters of name, 33 of
// runes ("Jah + Mal + Jah + Sur + Jah + Ber") and 22 of item types.
#define RW_NAME_CHARS		22
#define RW_RUNES_CHARS		34
#define RW_TYPE_CHARS		30

#define RW_TOGGLE_NAME		"Runeword Lookup"

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

static std::string Truncate(const std::string& text, size_t limit) {
	if (text.length() <= limit)
		return text;
	if (limit <= 2)
		return text.substr(0, limit);
	return text.substr(0, limit - 2) + "..";
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

void Runewords::OnLoad() {
	LoadConfig();

	lookupUI = new UI("Runewords", RW_WINDOW_WIDTH, RW_WINDOW_HEIGHT);

	lookupTab = new UITab("Runewords", lookupUI);
	new Texthook(lookupTab, RW_COL_NAME_X, 4, "Search:");
	searchBox = new Inputhook(lookupTab, 52, 2, 180, "");
	new Texthook(lookupTab, 240, 4, "(name, rune or item type)");
	Texthook* hint = new Texthook(lookupTab, 420, 4, "Esc closes");
	hint->SetColor(Grey);

	Texthook* header = new Texthook(lookupTab, RW_COL_NAME_X, RW_HEADER_Y, "Runeword");
	header->SetColor(Gold);
	header = new Texthook(lookupTab, RW_COL_RUNES_X, RW_HEADER_Y, "Runes");
	header->SetColor(Gold);
	header = new Texthook(lookupTab, RW_COL_TYPE_X, RW_HEADER_Y, "Item Types");
	header->SetColor(Gold);

	for (int row = 0; row < RW_ROWS; row++) {
		unsigned int rowY = RW_FIRST_ROW_Y + (row * RW_ROW_HEIGHT);
		Texthook* name = new Texthook(lookupTab, RW_COL_NAME_X, rowY, "");
		name->SetColor(White);
		nameCells.push_back(name);
		Texthook* runes = new Texthook(lookupTab, RW_COL_RUNES_X, rowY, "");
		runes->SetColor(Orange);
		runeCells.push_back(runes);
		Texthook* types = new Texthook(lookupTab, RW_COL_TYPE_X, rowY, "");
		types->SetColor(Tan);
		typeCells.push_back(types);
	}

	statusText = new Texthook(lookupTab, RW_COL_NAME_X, RW_FOOTER_Y, "");
	statusText->SetColor(Grey);
	prevText = new Texthook(lookupTab, 420, RW_FOOTER_Y, "< Prev");
	prevText->SetColor(Gold);
	prevText->SetHoverColor(White);
	prevText->SetLeftCallback(Runewords::OnPrevClick, this);
	nextText = new Texthook(lookupTab, 480, RW_FOOTER_Y, "Next >");
	nextText->SetColor(Gold);
	nextText->SetHoverColor(White);
	nextText->SetLeftCallback(Runewords::OnNextClick, this);

	// Whether the window starts collapsed to its title bar is remembered in
	// UI.ini, so leave that alone here. Visibility is driven by OnLoop() so the
	// window stays hidden until we are actually in a game.
}

void Runewords::LoadConfig() {
	BH::config->ReadToggle(RW_TOGGLE_NAME, "VK_NUMPAD9", true, Toggles[RW_TOGGLE_NAME]);
}

void Runewords::MpqLoaded() {
	Lock();
	BuildRecipes();
	Unlock();
}

void Runewords::BuildRecipes() {
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
			for (int n = 1; n <= 6; n++) {
				std::string code = Trim(entry->getString("Rune" + std::to_string(n)));
				if (code.length() > 0)
					runes.push_back(RuneName(code));
			}
			if (runes.empty())
				continue;

			RunewordRecipe recipe;
			recipe.name = RunewordName(entry);
			if (recipe.name.length() == 0)
				continue;
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
		for (int n = 0; n < 6 && extra.runes[n]; n++)
			runes.push_back(RuneName(extra.runes[n]));

		RunewordRecipe recipe;
		recipe.name = extra.name;
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

void Runewords::ApplyFilter() {
	matches.clear();
	for (unsigned int i = 0; i < recipes.size(); i++) {
		if (query.empty() || recipes[i].searchKey.find(query) != std::string::npos)
			matches.push_back(&recipes[i]);
	}
	if (page >= PageCount())
		page = PageCount() - 1;
}

unsigned int Runewords::PageCount() {
	if (matches.size() <= RW_ROWS)
		return 1;
	return (unsigned int)((matches.size() - 1) / RW_ROWS) + 1;
}

void Runewords::UpdateCells() {
	unsigned int first = page * RW_ROWS;
	for (int row = 0; row < RW_ROWS; row++) {
		unsigned int index = first + row;
		if (index < matches.size()) {
			const RunewordRecipe* recipe = matches[index];
			nameCells[row]->SetText("%s", Truncate(recipe->name, RW_NAME_CHARS).c_str());
			runeCells[row]->SetText("%s", Truncate(recipe->runes, RW_RUNES_CHARS).c_str());
			typeCells[row]->SetText("%s", Truncate(recipe->itemTypes, RW_TYPE_CHARS).c_str());
		} else {
			nameCells[row]->SetText("");
			runeCells[row]->SetText("");
			typeCells[row]->SetText("");
		}
	}

	unsigned int total = (unsigned int)matches.size();
	if (!recipesLoaded) {
		statusText->SetText("Waiting for game data to finish loading...");
	} else if (total == 0) {
		statusText->SetText("No runewords match \"%s\"", query.c_str());
	} else {
		unsigned int last = first + RW_ROWS;
		if (last > total)
			last = total;
		statusText->SetText("%u - %u of %u   (page %u of %u)",
			first + 1, last, total, page + 1, PageCount());
	}
}

void Runewords::ChangePage(int delta) {
	unsigned int pages = PageCount();
	if (delta < 0 && page == 0)
		page = pages - 1;
	else if (delta > 0 && page + 1 >= pages)
		page = 0;
	else
		page += delta;
	needsRefresh = true;
}

void Runewords::Search(const std::string& text) {
	std::string trimmed = Trim(text);
	query = ToLower(trimmed);
	page = 0;
	if (searchBox) {
		searchBox->SetText("%s", trimmed.c_str());
		searchBox->SetTextPos(0);
		searchBox->ResetSelection();
		searchBox->SetCursorPosition(searchBox->GetText().length());
		lastBoxText = searchBox->GetText();
	}
	Lock();
	ApplyFilter();
	Unlock();
	needsRefresh = true;
}

void Runewords::ShowWindow(bool show) {
	if (!lookupUI)
		return;
	if (show)
		Toggles[RW_TOGGLE_NAME].state = true;
	// Collapsing the window to its title bar also deactivates its search box
	// and paging buttons, so they can't swallow input while it is out of the way.
	lookupUI->SetMinimized(!show);
	lookupUI->SetVisible(Toggles[RW_TOGGLE_NAME].state);
	if (show) {
		lookupUI->SetActive(true);
		UI::Sort(lookupUI);
	}
}

void Runewords::OnLoop() {
	if (!lookupUI)
		return;
	// Disabling the feature entirely hides the window; collapse it as well so
	// its hooks stop responding to clicks and keys.
	if (!Toggles[RW_TOGGLE_NAME].state)
		lookupUI->SetMinimized(true);
	lookupUI->SetVisible(Toggles[RW_TOGGLE_NAME].state);
}

void Runewords::OnGameExit() {
	// Nothing is drawn outside a game, so make sure the window agrees and stops
	// taking input while we are back on the menus.
	if (lookupUI)
		lookupUI->SetVisible(false);
}

void Runewords::OnDraw() {
	if (!lookupUI || !lookupTab)
		return;

	// MpqLoaded can fire before this module exists, so build on first draw too.
	if (!recipesLoaded && Tables::isInitialized()) {
		Lock();
		BuildRecipes();
		Unlock();
	}

	if (!Toggles[RW_TOGGLE_NAME].state || lookupUI->IsMinimized())
		return;

	// The search box is edited by the user, so poll it for changes.
	if (searchBox && searchBox->GetText() != lastBoxText) {
		lastBoxText = searchBox->GetText();
		query = ToLower(Trim(lastBoxText));
		page = 0;
		needsRefresh = true;
	}

	if (needsRefresh) {
		Lock();
		ApplyFilter();
		UpdateCells();
		needsRefresh = false;
		Unlock();
	}
}

void Runewords::OnKey(bool up, BYTE key, LPARAM lParam, bool* block) {
	Toggle& toggle = Toggles[RW_TOGGLE_NAME];
	if (toggle.toggle != 0 && key == toggle.toggle) {
		*block = true;
		if (up)
			ShowWindow(!lookupUI || lookupUI->IsMinimized());
		return;
	}

	if (!toggle.state || !lookupUI || lookupUI->IsMinimized())
		return;

	switch (key) {
		// Escape closes the window rather than opening the game menu, but only
		// while it is open. When the search box has focus it consumes escape
		// first to drop focus, so the second press closes the window.
		case VK_ESCAPE:
			*block = true;
			if (up)
				ShowWindow(false);
			break;
		case VK_PRIOR:
			*block = true;
			if (!up)
				ChangePage(-1);
			break;
		case VK_NEXT:
			*block = true;
			if (!up)
				ChangePage(1);
			break;
	}
}

void Runewords::OnUserInput(const wchar_t* msg, bool fromGame, bool* block) {
	*block = true;

	// A command with no argument leaves the parameter pointer just past the end
	// of the command, so read defensively and keep only printable characters.
	std::string text;
	for (int i = 0; msg && i < 64 && msg[i] >= L' ' && msg[i] <= L'~'; i++)
		text += (char)msg[i];
	text = Trim(text);

	if (!recipesLoaded) {
		Print("\377c4Runewords:\377c0 still loading game data, try again in a moment.");
		return;
	}

	// ".rw" on its own just opens the window; ".rw <search>" also lists matches.
	Search(text);
	ShowWindow(true);

	if (text.empty()) {
		Print("\377c4Runewords:\377c0 %d recipes. Use .rw <name, rune or item type> to search.",
			(int)recipes.size());
		return;
	}

	if (matches.empty()) {
		Print("\377c4Runewords:\377c0 nothing matches \"%s\".", text.c_str());
		return;
	}

	const int maxPrinted = 10;
	for (int i = 0; i < (int)matches.size() && i < maxPrinted; i++) {
		Print("\377c4%s\377c0: %s \377c5[%u sockets - %s]\377c0",
			matches[i]->name.c_str(),
			matches[i]->runes.c_str(),
			matches[i]->sockets,
			matches[i]->itemTypes.c_str());
	}
	if ((int)matches.size() > maxPrinted) {
		Print("\377c4Runewords:\377c0 %d more matches, see the lookup window.",
			(int)matches.size() - maxPrinted);
	}
}

bool __cdecl Runewords::OnPrevClick(bool up, Hook* hook, void* data) {
	if (up)
		((Runewords*)data)->ChangePage(-1);
	return true;
}

bool __cdecl Runewords::OnNextClick(bool up, Hook* hook, void* data) {
	if (up)
		((Runewords*)data)->ChangePage(1);
	return true;
}
