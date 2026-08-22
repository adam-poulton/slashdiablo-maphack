#include "RunewordTab.h"
#include <algorithm>
#include "../../BH.h"
#include "../../Common.h"
#include "../../ItemRarity.h"
#include "../../MPQInit.h"
#include "../../MPQReader.h"
#include "../../StatDescriptions.h"
#include "../../TableReader.h"
#include "InfoText.h"

using namespace Drawing;
using namespace InfoText;

// Margins and the gaps between the three bands. Widths and the list height are
// measured from the tab by ApplyLayout().
#define RW_MARGIN			6	// down either side, and below the status line
#define RW_SEARCH_Y			3
#define RW_SEARCH_GAP		7	// between the search box and the list
#define RW_FOOTER_GAP		6	// between the list and the status line
#define RW_FOOTER_HEIGHT	8	// the status line itself

// The name column is fixed at the longest runeword name and the runes take the
// rest, since "Jah + Mal + Jah + Sur + Jah + Ber" is what needs the room.
#define RW_COL_NAME_W		136
#define RW_COL_GAP			4

// Recipes the realm enables without shipping them in runes.txt, as property
// entries in a runes.txt row's shape so they render and add up like the rest. What
// a rune contributes still comes from Gems.txt, so only the runeword's own bonuses
// are listed; anything the property tables cannot express goes in lines[].
struct ExtraRuneword {
	const char* name;
	const char* runes[6];
	const char* itemType;
	RunewordProperty properties[8];
	const char* lines[4];
};

static const ExtraRuneword kExtraRunewords[] = {
	{
		"Plague", { "r32", "r19", "r22" }, "weap",	// Cham + Fal + Um
		{
			{ "hit-skill",    "Poison Nova",   25,  15 },
			{ "gethit-skill", "Lower Resist",  20,  12 },
			{ "aura",         "Cleansing",     13,  17 },
			{ "allskills",    "",               1,   2 },
			{ "dmg-demon",    "",             260, 380 },
			{ "pierce-pois",  "",              23,  23 },
			{ "dmg-fire",     "",               5,  30 },
		},
		{
			// Per level amounts are held in eighths, which cannot express 0.3%.
			"0.3% Deadly Strike (Based on Character Level)",
		}
	},
};

// gems.txt gives every rune three sets of bonuses - weapon, helm or body armour,
// shield - which is why the same runeword rolls differently per base.
static const char* kSlotWeapon = "weapon";
static const char* kSlotHelm = "helm";
static const char* kSlotShield = "shield";

// The string table key is in "Name" ("Runeword1"), the readable name in
// "Rune Name".
static std::string RunewordName(JSONObject* entry) {
	std::string id = Trim(entry->getString("Name"));
	std::string localized = StatDescriptions::GetString(id);
	if (localized.length() > 0)
		return localized;
	
	std::string name;
	const char* fields[] = { "Rune Name", "*Rune Name" };
	for (int i = 0; i < 2 && name.length() == 0; i++)
		name = Trim(entry->getString(fields[i]));
	return (name.length() > 0) ? name : id;
}

// Rune codes ("r14") come from runes.txt, readable names from the parsed item
// data.
static std::string RuneName(const std::string& code) {
	std::string name = ItemName(code);
	// "El Rune" reads better as just "El" in a recipe list.
	const std::string suffix = " Rune";
	if (name.length() > suffix.length() &&
		name.compare(name.length() - suffix.length(), suffix.length(), suffix) == 0) {
		name.erase(name.length() - suffix.length());
	}
	return name;
}

// Walks the Equiv chain in ItemTypes.txt up to the root categories.
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
	// What the game does with the leftover types runewords are allowed in.
	return kSlotWeapon;
}

RunewordTab::RunewordTab(UI* ui) : InfoTab("Runewords", ui),
	shownSummary(-1),
	recipesLoaded(false),
	needsRefresh(true) {

	searchBox = new Inputhook(tab, RW_MARGIN, RW_SEARCH_Y, 0, "");
	searchBox->SetPlaceholder("Search by runeword name, rune or item type");
	searchBox->SetClearOnFocus(true);

	list = new Listhook(tab, RW_MARGIN, 0, 0, 0);
	// The name as the item it makes, the runes in the colour a rune is given.
	std::vector<ListColumn> columns;
	columns.push_back(ListColumn("", RW_COL_NAME_W, 0, 0,
		RarityColor(RarityRuneword), White));
	columns.push_back(ListColumn("", 0, 1, RW_COL_GAP, RarityColor(RarityRune)));
	list->SetColumns(columns);

	statusText = new Texthook(tab, RW_MARGIN, 0, "");
	statusText->SetColor(Grey);

	// Placed and switched on by UpdateSummary().
	summary = new Tooltiphook(InGame, 0, 0);
	summary->SetActive(false);

	ApplyLayout();
}

// The list takes whatever height is left between the search box and the status
// line, so a resize needs nothing but this.
void RunewordTab::ApplyLayout() {
	laidOutWidth = tab->GetXSize();
	laidOutHeight = tab->GetYSize();

	unsigned int contentWidth = (laidOutWidth > 2 * RW_MARGIN) ?
		(laidOutWidth - (2 * RW_MARGIN)) : 0;

	// Measured off the box rather than guessed, since its height follows its font.
	unsigned int listY = RW_SEARCH_Y + searchBox->GetYSize() + RW_SEARCH_GAP;
	unsigned int footerBand = RW_FOOTER_GAP + RW_FOOTER_HEIGHT + RW_MARGIN;
	unsigned int listHeight = (laidOutHeight > listY + footerBand) ?
		(laidOutHeight - listY - footerBand) : 0;

	searchBox->SetXSize(contentWidth);
	list->SetBaseY(listY);
	list->SetSize(contentWidth, listHeight);
	statusText->SetBaseY(listY + listHeight + RW_FOOTER_GAP);
	summary->SetMaxWidth(contentWidth);
}

void RunewordTab::MpqLoaded() {
	StatDescriptions::Initialize();
	BuildRecipes();
}

bool RunewordTab::HandlesCommand(const std::string& command) {
	return command.compare("rw") == 0 || command.compare("runewords") == 0;
}

// From misc.txt: ItemAttributes keeps only the item's quality level, which is a
// different number.
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

	// Only the rows flagged complete; the file keeps placeholders too. A modified
	// runes.txt with nothing flagged falls back to every row that has runes.
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

		for (int n = 0; n < 8 && extra.properties[n].code.length() > 0; n++)
			recipe.properties.push_back(extra.properties[n]);
		for (int n = 0; n < 4 && extra.lines[n]; n++)
			recipe.extraLines.push_back(extra.lines[n]);
		recipe.searchKey = ToLower(recipe.name + " " + recipe.runes + " " + recipe.itemTypes);
		recipes.push_back(recipe);
	}

	std::sort(recipes.begin(), recipes.end(), [](const RunewordRecipe& a, const RunewordRecipe& b) {
		return ToLower(a.name) < ToLower(b.name);
	});

	recipesLoaded = true;
	needsRefresh = true;
}

// Stats are the runeword's own bonuses plus each rune's, added together the way
// the game does. Runes differ by base, so this runs once per kind of base the
// runeword allows and only the lines that differ are tagged with their base.
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

	// One rendered list per kind of base.
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
		std::vector<std::string> lines = StatDescriptions::BuildLines(stats);
		// Base independent, so adding them to every list leaves them in common.
		for (unsigned int i = 0; i < recipe->extraLines.size(); i++)
			lines.push_back(recipe->extraLines[i]);
		perBase.push_back(lines);
	}

	if (perBase.empty()) {
		recipe->stats = StatDescriptions::BuildLines(own);
		for (unsigned int i = 0; i < recipe->extraLines.size(); i++)
			recipe->stats.push_back(recipe->extraLines[i]);
		return;
	}

	// Only the lines the bases disagree on are tagged.
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
	shownSummary = -1;
	UpdateStatus();
}

// Follows the scroll position as well as the rows, so it is refreshed per frame.
void RunewordTab::UpdateStatus() {
	if (!recipesLoaded) {
		statusText->SetText("Waiting for game data to finish loading...");
	} else if (matches.empty()) {
		statusText->SetText("No runewords match \"%s\"", query.c_str());
	} else if (list->GetMaxScrollTop() > 0) {
		statusText->SetText("%u - %u of %u runewords",
			list->GetFirstVisibleRow() + 1,
			list->GetLastVisibleRow(),
			(unsigned int)matches.size());
	} else {
		statusText->SetText("%u runewords", (unsigned int)matches.size());
	}
}

// Ordered the way the game describes an item. The panel wraps and sizes itself
// to whatever it is handed.
void RunewordTab::BuildSummaryLines(RunewordRecipe* recipe,
		std::vector<TooltipLine>& lines) {
	lines.push_back(TooltipLine(recipe->itemTypes, Grey));
	lines.push_back(TooltipLine(recipe->name, RarityColor(RarityRuneword)));
	lines.push_back(TooltipLine(recipe->runes, RarityColor(RarityRune)));
	if (recipe->requiredLevel > 0) {
		char required[64];
		sprintf_s(required, "Required level: %d", recipe->requiredLevel);
		lines.push_back(TooltipLine(required, White));
	}
	lines.push_back(TooltipLine("", White));

	for (unsigned int i = 0; i < recipe->stats.size(); i++)
		lines.push_back(TooltipLine(recipe->stats[i], White));
}

// Follows the mouse, falling back to the selection. Rebuilt only when the row
// changes, since the mouse sits on one row for many frames.
void RunewordTab::UpdateSummary() {
	int row = list->GetHoveredRow();
	if (row < 0)
		row = list->GetSelectedRow();

	if (!IsActive() || row < 0 || row >= (int)matches.size()) {
		summary->SetActive(false);
		shownSummary = -1;
		return;
	}

	if (row != shownSummary) {
		// recipes owns the records; matches only points into it.
		RunewordRecipe* recipe = const_cast<RunewordRecipe*>(matches[row]);
		LoadStats(recipe);

		std::vector<TooltipLine> lines;
		BuildSummaryLines(recipe, lines);
		summary->SetLines(lines);
		shownSummary = row;
	}

	// Must follow SetLines(): where it fits depends on how big it turned out.
	summary->PlaceBeside(tab->GetX(), tab->GetY(), tab->GetXSize(), tab->GetYSize());
	summary->SetActive(true);
}

void RunewordTab::Search(const std::string& text) {
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
void RunewordTab::OnOpen() {
	searchBox->SetCursorPosition(searchBox->GetText().length());
	searchBox->SetFocused(true);
}

void RunewordTab::OnClose() {
	searchBox->SetFocused(false);
	summary->SetActive(false);
	shownSummary = -1;
	Search("");
}

void RunewordTab::OnDraw() {
	if (tab->GetXSize() != laidOutWidth || tab->GetYSize() != laidOutHeight)
		ApplyLayout();

	// MpqLoaded can fire before this tab exists.
	if (!recipesLoaded && Tables::isInitialized()) {
		StatDescriptions::Initialize();
		BuildRecipes();
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

bool RunewordTab::OnKey(bool up, BYTE key) {
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
