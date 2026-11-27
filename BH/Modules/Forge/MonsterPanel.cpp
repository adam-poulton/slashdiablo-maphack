#include "MonsterPanel.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include "../Maphack/MonsterNames.h"
#include "../../StringUtil.h"
#include "../../Drawing/Basic/Texthook/Texthook.h"
#include "../../Drawing/Advanced/Checkhook/Checkhook.h"
#include "../../Drawing/Advanced/Combohook/Combohook.h"
#include "../../Drawing/Advanced/Inputhook/Inputhook.h"
#include "../../Drawing/Advanced/Listhook/Listhook.h"

using namespace Drawing;
using namespace MonsterMarks;

// The list is held at one width whatever the rules come to be called: a
// name is typed a letter at a time, and a list that widened with it would move
// the editor it is typed in. A longer name is cut short in its row.
#define MP_LIST_WIDTH		170

// The least the list is shown in: the ranks under their caption, the rules'
// caption and two of them. With more it scrolls.
#define MP_LIST_MIN_HEIGHT	(2 * MM_CAPTION_HEIGHT + 6 * MM_ROW_HEIGHT)

// Under the list: what adds a rule, clear of the last row.
#define MP_FOOTER_GAP		6
#define MP_FOOTER_HEIGHT	(MP_FOOTER_GAP + MM_TEXT_HEIGHT + 2)
#define MP_ADD_INSET		2

// Between what a rule matches and how it is drawn, and between that and
// what acts on the rule itself.
#define MP_SECTION_GAP		8
#define MP_ACTION_GAP		14
#define MP_CONDITION_ROWS	6

// The least a box that names things is laid out at, and what sits between the
// ticks of a row of them and either side of the word between the level boxes.
#define MP_INPUT_MIN		140
#define MP_RANK_GAP			10
#define MP_LEVEL_GAP		6
#define MP_LEVEL_DIGITS		3

// Kept between the longest modifier's name and the arrow of the box it is in.
#define MP_ARROW_ROOM		6

// How many monsters the list under their box shows before it scrolls, how far
// under the box it hangs, clear of the panel it draws itself on, and how much
// taller than its text a row is, so one tick box does not sit against the next.
#define MP_PICKER_ROWS		9
#define MP_PICKER_DROP		5
#define MP_PICKER_PADDING	6

#define MP_LABEL_COLOR		Gold
#define MP_LABEL_HOVER		Tan
#define MP_VALUE_COLOR		Silver

#define MP_MARKERS_CAPTION		"By rank"
#define MP_RULES_CAPTION	"Rules"
#define MP_RULES_HELP \
	"Rules apply top to bottom and add up: each changes only what it sets. " \
	"If two set the same thing, the lower one wins."

// The editor writes what a rule does to the line as the place it holds in
// its own list, into the rule's line change, so the two have to be the same
// list.
static_assert((int)MapIconOverride::LineUnchanged == (int)LineUnchanged &&
	(int)MapIconOverride::LineOff == (int)LineOff &&
	(int)MapIconOverride::LineOn == (int)LineOn,
	"the editor's line choices are the rule's line changes");

static Texthook* Label(HookGroup* group, const char* text) {
	Texthook* label = new Texthook(group, 0, 0, text);
	label->SetColor(MP_LABEL_COLOR);
	return label;
}

static Texthook* Action(HookGroup* group, const char* text, OnClick clicked, void* self) {
	Texthook* action = new Texthook(group, 0, 0, text);
	action->SetColor(MP_LABEL_COLOR);
	action->SetHoverColor(MP_LABEL_HOVER);
	action->SetLeftCallback(clicked, self);
	action->SetActive(false);
	return action;
}

static Inputhook* Box(HookGroup* group, unsigned int width, const char* placeholder) {
	Inputhook* box = new Inputhook(group, 0, 0, width, "");
	box->SetCompact(true);
	box->SetPlaceholder(placeholder);
	// Grey is what a box's hint is drawn in, so what is in the box is drawn
	// brighter than that whether or not it is being typed in.
	box->SetColor(MP_VALUE_COLOR);
	box->SetActive(false);
	return box;
}

// A level as typed: digits, and no more of them than a level has. Whatever else
// was typed is taken back out of the box.
static unsigned int ReadLevel(Inputhook* box) {
	std::string text = box->GetText(), digits;
	for (char c : text)
		if (std::isdigit((unsigned char)c) && digits.size() < MP_LEVEL_DIGITS)
			digits += c;
	if (digits != text)
		box->SetText("%s", digits.c_str());
	return (unsigned int)std::atoi(digits.c_str());
}

static void WriteLevel(Inputhook* box, unsigned int level) {
	if (level > 0)
		box->SetText("%u", level);
	else
		box->Clear();
}

MonsterPanel::MonsterPanel(UI* ui, std::string name, std::string category,
		RuleList* rules) :
		MarkerPanel(ui, name, category), rules(rules), seenRevision(0), modelStamp(1),
		built(false), editingIndex(-1), modifierIndex(0), labelsStamp(0), labelsNames(0),
		nameLabel(NULL), nameBox(NULL), monstersLabel(NULL), monstersBox(NULL),
		rankLabel(NULL), difficultyLabel(NULL), modifierLabel(NULL), modifierBox(NULL),
		levelLabel(NULL),
		minBox(NULL), toLabel(NULL), maxBox(NULL), editor(NULL), addAction(NULL),
		upAction(NULL), downAction(NULL), removeAction(NULL), picker(NULL),
		pickerOpen(false), summaryHerd(false), summaryNames(0), summaryLimit(0), wantAdd(false),
		wantRemove(false),
		wantMove(0), removeAsked(false), rankWidth(0), difficultyWidth(0),
		modifierWidth(0), levelWidth(0),
		conditionsWidth(0) {
	for (int i = 0; i < RankCount; i++) {
		rank[i] = true;
		rankChecks[i] = NULL;
	}
	for (int i = 0; i < DifficultyCount; i++) {
		difficulty[i] = true;
		difficultyChecks[i] = NULL;
	}
	model = rules->Get();
	seenRevision = rules->Revision();
}

// Built as the panel first draws with its markers in, which is on the thread
// that can measure text, and after the markers' own editors, so the list of
// monsters is the last thing here and is drawn over everything else.
void MonsterPanel::BuildControls() {
	built = true;

	nameLabel = Label(tab, "Name");
	nameBox = Box(tab, MP_INPUT_MIN, "");
	nameBox->SetSelectOnFocus(true);

	monstersLabel = Label(tab, "Monsters");
	monstersBox = Box(tab, MP_INPUT_MIN, "Any monster");
	// What it shows at rest is what was picked, not something to search for.
	monstersBox->SetClearOnFocus(true);

	rankLabel = Label(tab, "Rank");
	rankWidth = 0;
	for (int i = 0; i < RankCount; i++) {
		rankChecks[i] = new Checkhook(tab, 0, 0, &rank[i], RankLabel((Rank)i));
		rankChecks[i]->SetTextColor(MP_LABEL_COLOR);
		rankChecks[i]->SetHoverColor(MP_LABEL_HOVER);
		rankWidth += rankChecks[i]->GetXSize() + (i > 0 ? MP_RANK_GAP : 0);
	}

	difficultyLabel = Label(tab, "Difficulty");
	difficultyWidth = 0;
	for (int i = 0; i < DifficultyCount; i++) {
		difficultyChecks[i] = new Checkhook(tab, 0, 0, &difficulty[i],
			DifficultyLabel((Difficulty)i));
		difficultyChecks[i]->SetTextColor(MP_LABEL_COLOR);
		difficultyChecks[i]->SetHoverColor(MP_LABEL_HOVER);
		difficultyWidth += difficultyChecks[i]->GetXSize() + (i > 0 ? MP_RANK_GAP : 0);
	}

	modifierLabel = Label(tab, "Modifier");
	std::vector<std::string> modifiers;
	modifiers.push_back(ModifierLabel(AnyModifier, 0));
	modifierValues.push_back({ AnyModifier, 0 });
	unsigned int widest = 0;
	for (const ModifierName& modifier : Modifiers()) {
		modifiers.push_back(modifier.label);
		modifierValues.push_back({ (unsigned int)modifier.kind, modifier.id });
		widest = (std::max)(widest, (unsigned int)Texthook::GetTextSize(modifier.label, 0).x);
	}
	modifierWidth = 2 * COMBO_PADDING_X + widest + MP_ARROW_ROOM +
		(unsigned int)Texthook::GetTextSize("v", 0).x;
	modifierBox = new Combohook(tab, 0, 0, modifierWidth, &modifierIndex, modifiers);
	modifierBox->SetMaxVisible(MP_PICKER_ROWS);

	levelLabel = Label(tab, "Level");
	unsigned int boxWidth = 2 * INPUT_PADDING_X + 4 + (std::max)(
		(unsigned int)Texthook::GetTextSize("999", 0).x,
		(unsigned int)Texthook::GetTextSize("Any", 0).x);
	minBox = Box(tab, boxWidth, "Any");
	minBox->SetSelectOnFocus(true);
	toLabel = new Texthook(tab, 0, 0, "to");
	toLabel->SetColor(Grey);
	maxBox = Box(tab, boxWidth, "Any");
	maxBox->SetSelectOnFocus(true);
	levelWidth = 2 * boxWidth + 2 * MP_LEVEL_GAP + toLabel->GetXSize();
	conditionsWidth = (std::max)((std::max)((std::max)(rankWidth, difficultyWidth), modifierWidth),
		(std::max)(levelWidth, (unsigned int)MP_INPUT_MIN));

	editor = new MapIconhook(tab, 0, 0, &editing.style, MarkerOptions(0),
		MapIconOverride(&editing.setsIcon, &editing.line, &editingBase));

	addAction = Action(tab, "+ Add rule", OnAddClicked, this);
	addAction->SetActive(true);
	upAction = Action(tab, "Move up", OnUpClicked, this);
	downAction = Action(tab, "Move down", OnDownClicked, this);
	removeAction = Action(tab, "Remove", OnRemoveClicked, this);

	picker = new Listhook(tab, 0, 0, 0, 0);
	std::vector<ListColumn> columns;
	columns.push_back(ListColumn("", CHECK_BOX_SIZE + CHECK_LABEL_GAP, 0, 0, White));
	columns.push_back(ListColumn("", 0, 1, 0, Gold, White));
	picker->SetColumns(columns);
	picker->SetRowPadding(MP_PICKER_PADDING);
	// It hangs over the rest of the editor, so it brings its own panel, and is
	// offered a press ahead of what it covers.
	picker->SetFloating(true);
	picker->SetZOrder(-1);
	picker->SetActive(false);

	Measure();
}

MapIconStyle MonsterPanel::ShownStyle(const Rule& rule) {
	unsigned int base = (unsigned int)ShownRank(rule);
	MapIconStyle style = base < MarkerCount() ? MarkerStyle(base) : MapIconStyle();
	Apply(rule, style);
	return style;
}

// The rules as something else left them: a revert, or a reload.
void MonsterPanel::TakeRules() {
	model = rules->Get();
	seenRevision = rules->Revision();
	modelStamp++;
	int index = editingIndex < (int)model.size() ? editingIndex : -1;
	if (index < 0 && editingIndex >= 0)
		list->SelectFirst();
	LoadEditing(index);
}

void MonsterPanel::Commit() {
	rules->Set(model);
	seenRevision = rules->Revision();
	modelStamp++;
}

void MonsterPanel::LoadEditing(int index) {
	ClosePicker();
	editingIndex = index;
	removeAsked = false;
	nameBox->SetFocused(false);
	monstersBox->SetFocused(false);
	minBox->SetFocused(false);
	maxBox->SetFocused(false);
	if (index < 0)
		return;

	editing = model[index];
	for (int i = 0; i < RankCount; i++)
		rank[i] = (editing.ranks & (1u << i)) != 0;
	for (int i = 0; i < DifficultyCount; i++)
		difficulty[i] = (editing.difficulties & (1u << i)) != 0;

	// The modifiers offered, and after them the rule's own where it is one
	// that has no name.
	std::vector<std::string> modifiers;
	modifierValues.clear();
	modifiers.push_back(ModifierLabel(AnyModifier, 0));
	modifierValues.push_back({ AnyModifier, 0 });
	for (const ModifierName& modifier : Modifiers()) {
		modifiers.push_back(modifier.label);
		modifierValues.push_back({ (unsigned int)modifier.kind, modifier.id });
	}
	std::pair<unsigned int, unsigned int> held(editing.modifierKind, editing.modifierId);
	std::vector<std::pair<unsigned int, unsigned int>>::iterator found =
		std::find(modifierValues.begin(), modifierValues.end(), held);
	if (found == modifierValues.end()) {
		modifiers.push_back(ModifierLabel(held.first, held.second));
		modifierValues.push_back(held);
		found = modifierValues.end() - 1;
	}
	modifierIndex = (unsigned int)(found - modifierValues.begin());
	modifierBox->SetOptions(modifiers);

	nameBox->SetText("%s", editing.name.c_str());
	nameBox->SetTextPos(0);
	WriteLevel(minBox, editing.minLevel);
	WriteLevel(maxBox, editing.maxLevel);
	UpdateSummary(true);
}

// What the controls hold, put back into the list where it differs from what the
// list has.
void MonsterPanel::CollectEditing() {
	editing.name = Trim(nameBox->GetText());
	editing.ranks = 0;
	for (int i = 0; i < RankCount; i++)
		if (rank[i])
			editing.ranks |= 1u << i;
	editing.difficulties = 0;
	for (int i = 0; i < DifficultyCount; i++)
		if (difficulty[i])
			editing.difficulties |= 1u << i;
	if (modifierIndex < modifierValues.size()) {
		editing.modifierKind = modifierValues[modifierIndex].first;
		editing.modifierId = modifierValues[modifierIndex].second;
	}
	editing.minLevel = ReadLevel(minBox);
	editing.maxLevel = ReadLevel(maxBox);

	Rule kept = editing;
	kept.Normalize();
	if (kept != model[editingIndex]) {
		model[editingIndex] = kept;
		Commit();
	}
}

void MonsterPanel::DoRequests() {
	if (wantAdd) {
		wantAdd = false;
		model.push_back(Rule());
		Commit();
		list->SetSelected(ExtraIdBase + (int)model.size() - 1);
		LoadEditing((int)model.size() - 1);
	}

	int move = wantMove;
	wantMove = 0;
	int target = editingIndex + move;
	if (move != 0 && editingIndex >= 0 && target >= 0 && target < (int)model.size()) {
		std::swap(model[editingIndex], model[target]);
		Commit();
		// The rule in hand is the same one, at another place in the list.
		editingIndex = target;
		list->SetSelected(ExtraIdBase + target);
		removeAsked = false;
	}

	if (wantRemove) {
		wantRemove = false;
		if (editingIndex >= 0 && !removeAsked) {
			removeAsked = true;
		} else if (editingIndex >= 0) {
			model.erase(model.begin() + editingIndex);
			Commit();
			// The one that took its place, or with none after it the one before,
			// or with none left the last of the ranks.
			int next = (std::min)(editingIndex, (int)model.size() - 1);
			if (next >= 0) {
				list->SetSelected(ExtraIdBase + next);
			} else {
				list->SelectLast();
			}
			LoadEditing(next);
		}
	}
}

// What the box says of the monsters picked while it is not being searched in:
// their names, or as many of them as the box has room for and a count of the
// rest, so a long list ends in a number and not part of a name.
void MonsterPanel::UpdateSummary(bool force) {
	size_t names = MonsterNames::All().size();
	unsigned int limit = monstersBox->GetCharacterLimit();
	if (!force && names == summaryNames && limit == summaryLimit &&
			editing.monsters == summaryMonsters && editing.superUniques == summaryUniques &&
			editing.cowKingHerd == summaryHerd)
		return;
	summaryNames = names;
	summaryLimit = limit;
	summaryMonsters = editing.monsters;
	summaryUniques = editing.superUniques;
	summaryHerd = editing.cowKingHerd;

	std::vector<std::string> picked = MonsterNames::NamesOf(MonsterNames::All(),
		editing.monsters, editing.superUniques, editing.cowKingHerd);
	summary.clear();
	for (size_t shown = picked.size(); shown > 0; shown--) {
		summary.clear();
		for (size_t i = 0; i < shown; i++)
			summary += (i > 0 ? ", " : "") + picked[i];
		if (shown < picked.size())
			summary += " +" + std::to_string(picked.size() - shown);
		if (summary.size() <= limit)
			break;
	}
	if (!pickerOpen) {
		monstersBox->SetText("%s", summary.c_str());
		monstersBox->SetTextPos(0);
	}
}

void MonsterPanel::UpdateActions() {
	upAction->SetEnabled(editingIndex > 0);
	downAction->SetEnabled(editingIndex >= 0 && editingIndex + 1 < (int)model.size());
	const char* remove = removeAsked ? "Remove?" : "Remove";
	if (removeAction->GetText().compare(remove) != 0) {
		removeAction->SetText("%s", remove);
		removeAction->SetColor(removeAsked ? Red : MP_LABEL_COLOR);
	}

	// With no name of its own the rule goes by what it matches, which is
	// what the box says in the place of one.
	if (editingIndex >= 0 && editingIndex < (int)rowLabels.size() && editing.name.empty()) {
		const std::string& said = rowLabels[editingIndex];
		if (said != shownPlaceholder) {
			shownPlaceholder = said;
			nameBox->SetPlaceholder(said);
		}
	}
}

bool MonsterPanel::Chosen(unsigned int entry) {
	const MonsterNames::Entry& named = MonsterNames::All()[entry];
	if (named.kind == MonsterNames::Herd)
		return editing.cowKingHerd;
	const std::vector<unsigned int>& held = named.kind == MonsterNames::SuperUnique ?
		editing.superUniques : editing.monsters;
	for (unsigned int id : named.ids)
		if (std::find(held.begin(), held.end(), id) != held.end())
			return true;
	return false;
}

void MonsterPanel::OpenPicker() {
	pickerOpen = true;
	FillPicker();
}

// Filled from what is typed in the box, so the list narrows as the box does:
// what is already picked, of whatever kind, then the rest by kind, the few ubers
// and the super uniques ahead of the long run of monsters. The rows keep their
// places as they are ticked, and are put in order again only when the list is
// next filled.
void MonsterPanel::FillPicker() {
	const std::vector<MonsterNames::Entry>& entries = MonsterNames::All();
	pickerText = monstersBox->GetText();
	std::string needle = ToLower(Trim(pickerText));

	// One more group than there are kinds, for the picked.
	static const char* const Headings[] = { "Picked", "Ubers", "Super uniques", "Monsters" };
	static const MonsterNames::Kind Kinds[] = { MonsterNames::KindCount, MonsterNames::Uber,
		MonsterNames::SuperUnique, MonsterNames::Monster };
	// The herd is listed where the Cow King is.
	auto kindOf = [](const MonsterNames::Entry& entry) {
		return entry.kind == MonsterNames::Herd ? MonsterNames::SuperUnique : entry.kind;
	};

	std::vector<ListRow> rows;
	pickerEntries.clear();
	for (int group = 0; group < 4; group++) {
		std::vector<unsigned int> listed;
		for (unsigned int i = 0; i < entries.size(); i++) {
			if (!needle.empty() && entries[i].search.find(needle) == std::string::npos)
				continue;
			bool picked = Chosen(i);
			if (group == 0 ? picked : (!picked && kindOf(entries[i]) == Kinds[group]))
				listed.push_back(i);
		}
		if (listed.empty())
			continue;
		rows.push_back(ListRow({ Headings[group] }, true));
		pickerEntries.push_back(-1);
		for (unsigned int entry : listed) {
			ListRow row({ "", entries[entry].name });
			row.check = group == 0 ? 1 : 0;
			rows.push_back(row);
			pickerEntries.push_back((int)entry);
		}
	}
	if (rows.empty()) {
		ListRow none({ "", entries.empty() ? "Reading the game's tables" :
			"No monster by that name" });
		none.colors = { Grey, Grey };
		rows.push_back(none);
		pickerEntries.push_back(-1);
	}
	picker->SetRows(rows);
	picker->SetScrollTop(0);
}

void MonsterPanel::TogglePick(int row) {
	if (row < 0 || row >= (int)pickerEntries.size() || pickerEntries[row] < 0)
		return;
	const MonsterNames::Entry& named = MonsterNames::All()[pickerEntries[row]];
	std::vector<unsigned int>& held = named.kind == MonsterNames::SuperUnique ?
		editing.superUniques : editing.monsters;
	bool chosen = Chosen((unsigned int)pickerEntries[row]);
	if (named.kind == MonsterNames::Herd) {
		editing.cowKingHerd = !chosen;
	} else if (chosen) {
		for (unsigned int id : named.ids)
			held.erase(std::remove(held.begin(), held.end(), id), held.end());
	} else {
		held.insert(held.end(), named.ids.begin(), named.ids.end());
		std::sort(held.begin(), held.end());
		held.erase(std::unique(held.begin(), held.end()), held.end());
	}
	picker->SetRowCheck(row, !chosen);
}

void MonsterPanel::ClosePicker() {
	if (!pickerOpen)
		return;
	pickerOpen = false;
	picker->SetActive(false);
	// Left holding a search, the box would read as though that were what the
	// rule is for.
	monstersBox->SetText("%s", summary.c_str());
	monstersBox->SetTextPos(0);
}

// A press inside the list takes the caret out of the box, so an unfocused box is
// not by itself a reason to shut the list: the press may be the one ticking a
// monster in it. Held while the mouse is on it, and while its scrollbar is being
// dragged, which carries the mouse off it.
bool MonsterPanel::PickerHasMouse() {
	if (!picker->IsActive())
		return false;
	if (picker->IsScrolling())
		return true;
	return picker->InRange((unsigned int)Hook::GetMouseX(), (unsigned int)Hook::GetMouseY());
}

void MonsterPanel::UpdatePicker() {
	// A tick arrives on the release, by which time the press has taken the caret
	// out of the box. It is given back, so the next monster can be searched for
	// without clicking into the box again.
	int picked = picker->TakeClickedRow();
	if (picked >= 0 && pickerOpen) {
		picker->ClearSelection();
		TogglePick(picked);
		monstersBox->SetFocused(true);
	} else if (monstersBox->IsFocused()) {
		if (!pickerOpen)
			OpenPicker();
		else if (pickerText != monstersBox->GetText())
			FillPicker();
	} else if (pickerOpen && !PickerHasMouse()) {
		ClosePicker();
	}
}

void MonsterPanel::Sync() {
	if (MarkerCount() == 0)
		return;
	if (!built)
		BuildControls();

	if (rules->Revision() != seenRevision)
		TakeRules();
	DoRequests();

	int selected = list->GetSelected();
	int index = selected >= ExtraIdBase ? selected - ExtraIdBase : -1;
	if (index >= (int)model.size()) {
		index = -1;
		list->SelectFirst();
	}
	if (index != editingIndex)
		LoadEditing(index);

	if (editingIndex >= 0) {
		UpdatePicker();
		CollectEditing();
		UpdateSummary(false);
		unsigned int base = (unsigned int)ShownRank(editing);
		editingBase = base < MarkerCount() ? MarkerStyle(base) : MapIconStyle();
	}
	RefreshLabels();
	UpdateActions();
}

std::string MonsterPanel::MarkersCaption() {
	return MP_MARKERS_CAPTION;
}

// What each rule's row says: its name, or with none what it matches in a few
// words. The whole of what it matches is said over the row. Worked out again
// only when the rules or the names they are said with have changed.
void MonsterPanel::RefreshLabels() {
	const std::vector<MonsterNames::Entry>& entries = MonsterNames::All();
	if (labelsStamp == modelStamp && labelsNames == entries.size())
		return;
	labelsStamp = modelStamp;
	labelsNames = entries.size();
	rowLabels.clear();
	rowHelps.clear();
	for (const Rule& rule : model) {
		std::vector<std::string> names =
			MonsterNames::NamesOf(entries, rule.monsters, rule.superUniques, rule.cowKingHerd);
		rowLabels.push_back(rule.name.empty() ? Summary(rule, names) : rule.name);
		rowHelps.push_back(Summary(rule, names, 0));
	}
}

void MonsterPanel::AppendRows(std::vector<MarkerListRow>& rows) {
	RefreshLabels();
	rows.push_back(MarkerListRow::Caption(MP_RULES_CAPTION, MP_RULES_HELP));
	for (unsigned int i = 0; i < model.size(); i++)
		rows.push_back(MarkerListRow::Marker(ExtraIdBase + (int)i, rowLabels[i],
			ShownStyle(model[i]), rowHelps[i]));
}

void MonsterPanel::MeasureExtra(unsigned int& listWidth, unsigned int& labelWidth,
		unsigned int& detailMinWidth, unsigned int& detailHeight,
		unsigned int& listMinHeight) {
	listWidth = MP_LIST_WIDTH;
	listMinHeight = (std::min)(listMinHeight, (unsigned int)MP_LIST_MIN_HEIGHT);
	if (!built)
		return;

	Texthook* labels[] = { nameLabel, monstersLabel, rankLabel, difficultyLabel,
		modifierLabel, levelLabel };
	for (Texthook* label : labels)
		labelWidth = (std::max)(labelWidth, label->GetXSize() + MAP_ICON_LABEL_GAP);
	labelWidth = (std::max)(labelWidth, editor->GetNaturalLabelWidth());
	editor->SetLabelWidth(labelWidth);

	detailMinWidth = (std::max)(detailMinWidth,
		(std::max)(editor->GetMinXSize(), labelWidth + conditionsWidth));
	detailHeight = (std::max)(detailHeight, MP_CONDITION_ROWS * MAP_ICON_ROW_HEIGHT +
		MP_SECTION_GAP + editor->GetYSize() + MP_SECTION_GAP + MM_TEXT_HEIGHT);
}

unsigned int MonsterPanel::ListFooterHeight() {
	return MP_FOOTER_HEIGHT;
}

void MonsterPanel::LayoutExtra(int selectedId, unsigned int listX, unsigned int listWidth,
		unsigned int footerY, unsigned int detailX, unsigned int detailY,
		unsigned int detailWidth, unsigned int labelWidth) {
	if (!built)
		return;

	addAction->SetBaseX(listX + MP_ADD_INSET);
	addAction->SetBaseY(footerY + MP_FOOTER_GAP);

	bool on = editingIndex >= 0 && selectedId == ExtraIdBase + editingIndex;
	Hook* controls[] = {
		nameLabel, nameBox, monstersLabel, monstersBox, rankLabel, rankChecks[0],
		rankChecks[1], rankChecks[2], rankChecks[3], difficultyLabel,
		difficultyChecks[0], difficultyChecks[1], difficultyChecks[2], modifierLabel,
		modifierBox,
		levelLabel, minBox, toLabel, maxBox, editor, upAction, downAction, removeAction,
	};
	for (Hook* control : controls)
		control->SetActive(on);
	picker->SetActive(on && pickerOpen);
	if (!on)
		return;

	unsigned int controlX = detailX + labelWidth;
	unsigned int controlWidth = detailWidth > labelWidth ? detailWidth - labelWidth : 0;
	unsigned int y = detailY;

	nameLabel->SetBaseX(detailX);
	nameLabel->SetBaseY(MapIconhook::RowY(y, nameLabel));
	nameBox->SetBaseX(controlX);
	nameBox->SetBaseY(MapIconhook::RowY(y, nameBox));
	nameBox->SetXSize(controlWidth);
	y += MAP_ICON_ROW_HEIGHT;

	monstersLabel->SetBaseX(detailX);
	monstersLabel->SetBaseY(MapIconhook::RowY(y, monstersLabel));
	monstersBox->SetBaseX(controlX);
	monstersBox->SetBaseY(MapIconhook::RowY(y, monstersBox));
	monstersBox->SetXSize(controlWidth);
	y += MAP_ICON_ROW_HEIGHT;

	rankLabel->SetBaseX(detailX);
	rankLabel->SetBaseY(MapIconhook::RowY(y, rankLabel));
	unsigned int x = controlX;
	for (int i = 0; i < RankCount; i++) {
		rankChecks[i]->SetBaseX(x);
		rankChecks[i]->SetBaseY(MapIconhook::RowY(y, rankChecks[i]));
		x += rankChecks[i]->GetXSize() + MP_RANK_GAP;
	}
	y += MAP_ICON_ROW_HEIGHT;

	difficultyLabel->SetBaseX(detailX);
	difficultyLabel->SetBaseY(MapIconhook::RowY(y, difficultyLabel));
	x = controlX;
	for (int i = 0; i < DifficultyCount; i++) {
		difficultyChecks[i]->SetBaseX(x);
		difficultyChecks[i]->SetBaseY(MapIconhook::RowY(y, difficultyChecks[i]));
		x += difficultyChecks[i]->GetXSize() + MP_RANK_GAP;
	}
	y += MAP_ICON_ROW_HEIGHT;

	modifierLabel->SetBaseX(detailX);
	modifierLabel->SetBaseY(MapIconhook::RowY(y, modifierLabel));
	modifierBox->SetBaseX(controlX);
	modifierBox->SetBaseY(MapIconhook::RowY(y, modifierBox));
	y += MAP_ICON_ROW_HEIGHT;

	levelLabel->SetBaseX(detailX);
	levelLabel->SetBaseY(MapIconhook::RowY(y, levelLabel));
	minBox->SetBaseX(controlX);
	minBox->SetBaseY(MapIconhook::RowY(y, minBox));
	toLabel->SetBaseX(controlX + minBox->GetXSize() + MP_LEVEL_GAP);
	toLabel->SetBaseY(MapIconhook::RowY(y, toLabel));
	maxBox->SetBaseX(toLabel->GetBaseX() + toLabel->GetXSize() + MP_LEVEL_GAP);
	maxBox->SetBaseY(MapIconhook::RowY(y, maxBox));
	y += MAP_ICON_ROW_HEIGHT + MP_SECTION_GAP;

	editor->SetBaseX(detailX);
	editor->SetBaseY(y);
	editor->SetLabelWidth(labelWidth);
	editor->SetXSize(detailWidth);

	// At the foot of the column, on the line of what adds a rule under the
	// list, so what acts on the list reads as one row across the tab.
	x = detailX;
	Texthook* actions[] = { upAction, downAction, removeAction };
	for (Texthook* action : actions) {
		action->SetBaseX(x);
		action->SetBaseY(footerY + MP_FOOTER_GAP);
		x += action->GetXSize() + MP_ACTION_GAP;
	}

	// Under the box it is filled from, or over it where the screen has no room
	// under.
	if (pickerOpen) {
		unsigned int rowCount = (std::min)((unsigned int)pickerEntries.size(),
			(unsigned int)MP_PICKER_ROWS);
		unsigned int height = (std::max)(rowCount, 1u) * picker->GetRowHeight();
		unsigned int below = monstersBox->GetBaseY() + monstersBox->GetYSize() + MP_PICKER_DROP;
		unsigned int screenBelow = monstersBox->GetY() + monstersBox->GetYSize() + MP_PICKER_DROP;
		picker->SetBaseX(monstersBox->GetBaseX());
		if (screenBelow + height + MP_PICKER_DROP > Hook::GetScreenHeight() &&
				monstersBox->GetBaseY() >= height + MP_PICKER_DROP)
			picker->SetBaseY(monstersBox->GetBaseY() - MP_PICKER_DROP - height);
		else
			picker->SetBaseY(below);
		picker->SetSize(monstersBox->GetXSize(), height);
	}
}

void MonsterPanel::OnClose() {
	MarkerPanel::OnClose();
	if (!built)
		return;
	ClosePicker();
	removeAsked = false;
	nameBox->SetFocused(false);
	monstersBox->SetFocused(false);
	minBox->SetFocused(false);
	maxBox->SetFocused(false);
}

bool MonsterPanel::OnAddClicked(bool up, Hook* hook, void* self) {
	if (up)
		((MonsterPanel*)self)->wantAdd = true;
	return true;
}

bool MonsterPanel::OnUpClicked(bool up, Hook* hook, void* self) {
	if (up)
		((MonsterPanel*)self)->wantMove = -1;
	return true;
}

bool MonsterPanel::OnDownClicked(bool up, Hook* hook, void* self) {
	if (up)
		((MonsterPanel*)self)->wantMove = 1;
	return true;
}

bool MonsterPanel::OnRemoveClicked(bool up, Hook* hook, void* self) {
	if (up)
		((MonsterPanel*)self)->wantRemove = true;
	return true;
}
