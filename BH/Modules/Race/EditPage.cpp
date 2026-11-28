#include "EditPage.h"
#include <algorithm>
#include <cstdlib>
#include "../../Common.h"
#include "../../StringUtil.h"
#include "../Tracking/RowLists.h"
#include "Race.h"
#include "RouteEdit.h"

using namespace Drawing;

namespace {
	// How often the route is read again. It changes under the page only when
	// its file is changed by another client, which need not show at once.
	const unsigned long long kRefreshEveryMs = 250;

	// Each part's top line is as tall as the button on it, and its list starts
	// level under it, where the run page's lists start.
	const unsigned int kButtonHeight = 18;
	const unsigned int kTitleGap = 4;		// the top line of a part to what is under it
	const unsigned int kFootGap = 5;		// a list to the actions under it
	const unsigned int kBottomPad = 5;		// the last line to the foot of the window
	const unsigned int kActionGap = 10;		// between two actions on a line
	const unsigned int kTitleDoneGap = 10;	// what is being added to, and Done after it
	const unsigned int kMarkGap = 10;		// a split to add, and what marks it

	const unsigned int kPlaceWidth = 14;	// a split's place in its route, to two figures
	const unsigned int kPlaceGap = 4;

	// A list keeps a gutter for its scrollbar on its right, which already
	// stands between its rows and the rule after it.
	const unsigned int kRuleGapBefore = 2;
	const unsigned int kRuleGapAfter = 10;

	// How far left of its rows' text a list draws the band behind the row picked.
	const unsigned int kBandInset = 2;

	// What a box is built at, before there is a window to ask how wide it is.
	const unsigned int kBoxSeed = 96;

	unsigned int Less(unsigned int from, unsigned int by) {
		return from > by ? from - by : 0;
	}

	std::vector<ListColumn> SplitListColumns() {
		std::vector<ListColumn> columns(RaceRows::RouteSplitColumns);
		columns[RaceRows::RouteSplitPlace] = ListColumn("", kPlaceWidth, 0, 0, Grey);
		columns[RaceRows::RouteSplitName] = ListColumn("", 0, 1, kPlaceGap, Gold, White);
		return columns;
	}

	std::vector<ListColumn> ChoiceListColumns() {
		std::vector<ListColumn> columns(RaceRows::ChoiceColumns);
		columns[RaceRows::ChoiceName] = ListColumn("", 110, 3, 0, Gold, White);
		columns[RaceRows::ChoiceMarkedBy] = ListColumn("", 60, 2, kMarkGap, Grey);
		return columns;
	}

	Inputhook* AddBox(HookGroup* group, const char* placeholder) {
		Inputhook* box = new Inputhook(group, 0, 0, kBoxSeed, "");
		box->SetPlaceholder(placeholder);
		box->SetCompact(true);
		return box;
	}
}

EditPage::EditPage(HookGroup* group) : split(-1), done(false), doneClicked(false), hasRoute(false),
		editable(false),
		choicesMade(false), foldOwed(true), asked(ActNone), refreshedAt(0),
		nameWasFocused(false), nameWanted(false), cancelName(false), escapeHeld(false),
		laidOut(false), laidOutWidth(0), laidOutHeight(0), laidOutSplits(0), addWidth(0) {
	for (int action = 0; action < ActCount; action++) {
		actions[action] = NULL;
		asks[action] = { this, ActNone };
	}

	// Nothing is measured here: the columns and the layout wait for the first
	// draw, which is on the thread allowed to ask the game how wide text is.
	nameBox = AddBox(group, "Route name");
	nameBox->SetSelectOnFocus(true);
	// What is in the box is the route's name, with the caret in it or not.
	nameBox->SetColor(White);
	splitList = new Listhook(group, 0, 0, 0, 0);
	AddAction(group, ActUp, "Move up");
	AddAction(group, ActDown, "Move down");
	AddAction(group, ActRemove, "Remove");

	rule = new Boxhook(group, 0, 0, 1, 1);
	rule->SetColor(UI_SURFACE_RULE);
	rule->SetTransparency(BTNormal);

	addTitle = new Texthook(group, 0, 0, "");
	addTitle->SetColor(Silver);
	doneButton = new Buttonhook(group, 0, 0, kButtonHeight, "Done");
	doneButton->SetLeftCallback(OnDone, this);
	findBox = AddBox(group, "Find a split, or type level 24");
	choiceList = new Listhook(group, 0, 0, 0, 0);
}

void EditPage::AddAction(HookGroup* group, Action action, const char* text) {
	Texthook* hook = new Texthook(group, 0, 0, "%s", text);
	hook->SetColor(Gold);
	hook->SetHoverColor(Tan);
	asks[action] = { this, action };
	hook->SetLeftCallback(OnAction, &asks[action]);
	actions[action] = hook;
}

// Noted here and done as the page next draws, which is where the route is read.
bool EditPage::OnAction(bool up, Hook* hook, void* ask) {
	if (!hook->IsEnabled())
		return false;
	if (up) {
		const Ask* clicked = (const Ask*)ask;
		clicked->page->asked = clicked->action;
	}
	return true;
}

bool EditPage::OnDone(bool up, Hook* hook, void* page) {
	if (up)
		((EditPage*)page)->doneClicked = true;
	return true;
}

void EditPage::SetShown(bool show) {
	Hook* const all[] = { nameBox, splitList, rule, addTitle, doneButton, findBox, choiceList };
	for (Hook* hook : all)
		hook->SetActive(show);
	for (int action = ActUp; action < ActCount; action++)
		actions[action]->SetActive(show);
	refreshedAt = 0;
}

void EditPage::Begin(const std::string& id, bool rename) {
	editing = id;
	split = -1;
	done = false;
	doneClicked = false;
	notice.clear();
	asked = ActNone;
	nameWanted = rename;
	nameWasFocused = false;
	cancelName = false;
	if (!find.empty()) {
		findBox->SetText("%s", "");
		find.clear();
	}
	refreshedAt = 0;
}

void EditPage::Leave() {
	if (nameBox->IsFocused() || nameWasFocused)
		CommitName();
	DropName();
	findBox->SetFocused(false);
}

bool EditPage::TakeDone() {
	const bool was = done;
	done = false;
	return was;
}

// Escape in either box lets go of it and leaves the window open, and in the
// name box gives up the name being typed. A box lets the key through, so it is
// held here from its press to its release.
bool EditPage::OnKey(bool up, BYTE key) {
	if (key != VK_ESCAPE)
		return false;
	if (!up && (nameBox->IsFocused() || findBox->IsFocused())) {
		escapeHeld = true;
		if (nameBox->IsFocused())
			cancelName = true;
		findBox->SetFocused(false);
		return true;
	}
	if (up && escapeHeld) {
		escapeHeld = false;
		return true;
	}
	return false;
}

void EditPage::Changed() {
	notice.clear();
	refreshedAt = 0;
}

void EditPage::DropName() {
	nameBox->SetFocused(false);
	nameWasFocused = false;
	nameWanted = false;
	cancelName = false;
	refreshedAt = 0;
}

// The name is kept on enter and on the caret leaving the box, wherever it goes.
void EditPage::TakeName() {
	if (cancelName) {
		DropName();
		return;
	}
	const bool submitted = nameBox->TakeSubmitted();
	const bool focused = nameBox->IsFocused();
	if (submitted || (nameWasFocused && !focused)) {
		CommitName();
		DropName();
		return;
	}
	nameWasFocused = focused;
}

void EditPage::CommitName() {
	const std::string typed = Trim(nameBox->GetText());
	if (!hasRoute || typed.empty() || typed == route.name)
		return;

	bool renamed = false;
	const std::string id = route.id;
	const bool written = Race::EditRoutes([&](std::vector<Splits::Route>& routes) {
		renamed = RouteEdit::Rename(routes, id, typed);
	});
	Changed();
	if (written && !renamed)
		notice = "Another route is called " + typed;
}

// The splits to add are narrowed as the box over them is typed in.
void EditPage::TakeFind() {
	findBox->TakeSubmitted();
	const std::string typed = Trim(findBox->GetText());
	if (typed == find)
		return;
	find = typed;
	Changed();
}

// A click in the route's splits only picks, and what is picked is read from the
// list's own selection: that also says when a click on the split picked, or on
// nothing, let go of it.
void EditPage::TakePicks() {
	splitList->TakeClickedRow();
	const int splitRow = splitList->GetSelectedRow();
	if (splitRow != split && splitRow < (int)splitRows.size()) {
		split = splitRow;
		Changed();
	}

	const int choice = choiceList->TakeClickedRow();
	if (choice >= 0 && (size_t)choice < choiceRows.size()) {
		// A split to add is clicked and not picked: nothing stays lit behind it.
		choiceList->ClearSelection();
		AddChoice(choiceRows[choice]);
	}
}

void EditPage::AddChoice(const RaceRows::Row& row) {
	std::string what;
	if (!hasRoute || !editable || !ListRows::KeyRest(row.key, RaceRows::RouteKeys::kAdd, what))
		return;

	Splits::Split added;
	if (what == "level") {
		if (!RouteEdit::LevelSplit(find, added))
			return;
	} else {
		const size_t index = (size_t)atoi(what.c_str());
		if (index >= RouteEdit::Catalogue().size())
			return;
		added = RouteEdit::Catalogue()[index];
	}

	const std::string id = editing;
	int at = split;
	Race::EditRoutes([&](std::vector<Splits::Route>& routes) {
		for (Splits::Route& each : routes) {
			if (each.id == id)
				at = (int)RouteEdit::AddSplit(each, added, split);
		}
	});
	split = at;
	Changed();
}

void EditPage::TakeAction() {
	if (doneClicked) {
		doneClicked = false;
		asked = ActNone;
		Leave();
		done = true;
		return;
	}

	const int action = asked;
	asked = ActNone;
	if (action == ActNone)
		return;
	Changed();
	if (split < 0)
		return;

	const std::string id = editing;
	Race::EditRoutes([&](std::vector<Splits::Route>& routes) {
		for (Splits::Route& each : routes) {
			if (each.id != id)
				continue;
			if (action == ActRemove) {
				RouteEdit::RemoveSplit(each, (unsigned int)split);
				split = -1;
			} else {
				const int by = action == ActUp ? -1 : 1;
				if (RouteEdit::MoveSplit(each, (unsigned int)split, by))
					split += by;
			}
		}
	});
}

void EditPage::Refresh() {
	bool found = false;
	Race::Read([&](const Race::View& view) {
		for (const Splits::Route& each : view.routes) {
			if (each.id == editing) {
				found = true;
				route = each;
			}
		}
		editable = view.editable;
	});

	hasRoute = found;
	if (!hasRoute) {
		// The route has gone from under the page, which has nothing left to change.
		route = Splits::Route();
		split = -1;
		done = true;
	}
	if (split >= (int)route.splits.size())
		split = -1;

	RowLists::Push(splitList, splitRows, RaceRows::RouteSplits(route));
	RefreshChoices();
}

// The splits there are to add are the same until what is typed over them changes.
void EditPage::RefreshChoices() {
	if (choicesMade && find == choicesFor)
		return;
	choicesMade = true;
	choicesFor = find;

	// A search shows what it found, under whichever difficulty that is.
	choiceList->SetFoldingSuspended(!find.empty());
	if (RowLists::Push(choiceList, choiceRows, RaceRows::SplitChoices(find)))
		choiceList->SetScrollTop(0);

	// Once, the first time they are listed whole: three difficulties of the same
	// splits open at once is a long way to scroll for the two played least.
	if (foldOwed && find.empty()) {
		foldOwed = false;
		bool first = true;
		for (size_t i = 0; i < choiceRows.size(); i++) {
			if (!choiceRows[i].heading)
				continue;
			if (!first)
				choiceList->SetGroupFolded((int)i, true);
			first = false;
		}
	}
}

// The one place anything is sized or positioned.
void EditPage::Layout(unsigned int width, unsigned int height, unsigned int splitsWidth) {
	if (!laidOut) {
		splitList->SetColumns(SplitListColumns());
		choiceList->SetColumns(ChoiceListColumns());
	}
	laidOut = true;
	laidOutWidth = width;
	laidOutHeight = height;
	laidOutSplits = splitsWidth;

	const unsigned int textHeight = addTitle->GetYSize();
	const unsigned int boxHeight = nameBox->GetYSize();
	const unsigned int gutter = splitList->GetGutterWidth();
	const unsigned int titleY = Less(kButtonHeight, textHeight) / 2;
	const unsigned int listY = kButtonHeight + kTitleGap;
	const unsigned int footY = Less(height, kBottomPad + textHeight);

	// The splits stand as wide as the routes did, or as what is done to them
	// needs where that is more.
	unsigned int actionsWidth = 2 * kActionGap;
	for (Action action : { ActUp, ActDown, ActRemove })
		actionsWidth += actions[action]->GetXSize();
	splitsWidth = (std::max)(splitsWidth, actionsWidth + gutter);

	const unsigned int splitsX = UI_CONTENT_MARGIN;
	const unsigned int ruleX = splitsX + splitsWidth + kRuleGapBefore;
	const unsigned int addX = ruleX + 1 + kRuleGapAfter;
	addWidth = Less(width, addX + UI_CONTENT_MARGIN);

	// The route: its name in a box as wide as the band behind a split picked,
	// over its splits and what is done to the one picked.
	nameBox->SetBaseX(Less(splitsX, kBandInset));
	nameBox->SetBaseY(Less(kButtonHeight, boxHeight) / 2);
	nameBox->SetXSize((std::max)(Less(splitsWidth, gutter), kBoxSeed));
	splitList->SetBaseX(splitsX);
	splitList->SetBaseY(listY);
	splitList->SetSize(splitsWidth, Less(footY, listY + kFootGap));
	unsigned int x = splitsX;
	for (Action action : { ActUp, ActDown, ActRemove }) {
		actions[action]->SetBaseX(x);
		actions[action]->SetBaseY(footY);
		x += actions[action]->GetXSize() + kActionGap;
	}

	rule->SetBaseX(ruleX);
	rule->SetBaseY(0);
	rule->SetYSize(Less(height, FOOTER_RULE_CLEARANCE));

	// The splits there are to add, under the box that narrows them, with the
	// way back against the far edge, where the run page has its buttons.
	addTitle->SetBaseX(addX);
	addTitle->SetBaseY(titleY);
	doneButton->SetBaseX(addX + Less(addWidth, doneButton->GetXSize()));
	doneButton->SetBaseY(0);
	findBox->SetBaseX(Less(addX, kBandInset));
	findBox->SetBaseY(listY);
	findBox->SetXSize((std::max)(Less(addWidth, gutter), kBoxSeed));
	const unsigned int choicesY = listY + boxHeight + kTitleGap;
	choiceList->SetBaseX(addX);
	choiceList->SetBaseY(choicesY);
	choiceList->SetSize(addWidth, Less(height, choicesY));
}

// What each part says of the route as it was last read, and which of the
// actions can be taken.
void EditPage::UpdateTexts() {
	const bool picked = hasRoute && split >= 0 && (size_t)split < route.splits.size();
	const size_t splits = route.splits.size();

	// The name is the route's until the caret is in it.
	if (!nameBox->IsFocused() && nameBox->GetText() != route.name) {
		nameBox->SetText("%s", route.name.c_str());
		nameBox->SetTextPos(0);
		nameBox->ResetSelection();
		nameBox->SetCursorPosition((unsigned int)route.name.length());
	}
	if (nameWanted && hasRoute) {
		nameWanted = false;
		nameBox->SetFocused(true);
		nameBox->SelectAll();
		nameWasFocused = true;
	}

	// Always there, and greyed until a split is picked, so that what can be
	// done to a split is seen before one is clicked.
	actions[ActUp]->SetEnabled(editable && picked && split > 0);
	actions[ActDown]->SetEnabled(editable && picked && (size_t)split + 1 < splits);
	actions[ActRemove]->SetEnabled(editable && picked);

	const unsigned int titleWidth = Less(addWidth, doneButton->GetXSize() + kTitleDoneGap);
	std::string adding = "Add a split";
	if (picked)
		adding += " after " + route.splits[split].name;
	else if (splits > 0)
		adding += " at the end";
	addTitle->SetText("%s", Texthook::Fit(adding, titleWidth, 0).c_str());

	if (!editable) {
		status = "BH_routes.json could not be read, so routes cannot be changed here";
	} else if (!notice.empty()) {
		status = notice;
	} else if (!find.empty()) {
		size_t matching = 0;
		for (const RaceRows::Row& row : choiceRows) {
			if (!row.heading)
				matching++;
		}
		status = (matching == 0 ? "No split matches \"" :
			std::to_string(matching) + (matching == 1 ? " split matches \"" : " splits match \"")) +
			find + "\"";
	} else if (picked) {
		const Splits::Split& one = route.splits[split];
		status = one.name + ": " + RouteEdit::DifficultyOf(one) + ", marked by " + RouteEdit::MarkedBy(one);
	} else if (splits == 0) {
		status = "Click a split on the right to add it";
	} else {
		status = "Click a split to move or remove it, or one on the right to add it";
	}
}

// Giving a list rows lets go of what was selected in it.
void EditPage::KeepPicks() {
	if (splitList->GetSelectedRow() != split)
		splitList->SetSelectedRow(split);
}

void EditPage::Draw(unsigned int width, unsigned int height, unsigned int splitsWidth) {
	TakeName();
	TakeFind();
	TakePicks();
	TakeAction();

	const unsigned long long tick = BHGetTickCount();
	const bool due = refreshedAt == 0 || tick - refreshedAt >= kRefreshEveryMs;
	if (due) {
		refreshedAt = tick;
		Refresh();
	}
	// The texts are cut to the widths the layout comes to.
	const bool moved = !laidOut || width != laidOutWidth || height != laidOutHeight ||
		splitsWidth != laidOutSplits;
	if (moved)
		Layout(width, height, splitsWidth);
	if (due || moved)
		UpdateTexts();
	KeepPicks();
}
