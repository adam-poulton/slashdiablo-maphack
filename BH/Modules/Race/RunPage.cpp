#include "RunPage.h"
#include <algorithm>
#include "../../Common.h"
#include "../Tracking/RowLists.h"
#include "Race.h"
#include "RouteEdit.h"

using namespace Drawing;

namespace {
	// How often the routes and the run are read again. The fastest figure is a
	// clock showing seconds.
	const unsigned long long kRefreshEveryMs = 250;

	// Each part's top line is as tall as the buttons, and its list starts
	// level under it.
	const unsigned int kButtonHeight = 18;
	const unsigned int kButtonGap = 6;		// one button to the next
	const unsigned int kNameGap = 10;		// the route's name to the buttons after it
	const unsigned int kTitleGap = 4;		// the top line of a part to what is under it
	const unsigned int kFootGap = 5;		// a list to what is under it
	const unsigned int kBottomPad = 5;		// the last line to the foot of the window
	const unsigned int kActionGap = 10;		// between two actions on a line
	const unsigned int kNoteGap = 6;		// a route's name to what is said after it
	const unsigned int kRowPad = 6;			// what is said after a name to the band's end
	const unsigned int kColumnGap = 10;

	// A list keeps a gutter for its scrollbar on its right, which already
	// stands between its rows and the rule after it.
	const unsigned int kRuleGapBefore = 2;
	const unsigned int kRuleGapAfter = 10;
	const unsigned int kRuleSpan = kRuleGapBefore + 1 + kRuleGapAfter;

	// The routes are as wide as their names, and the route picked has the rest.
	const unsigned int kRoutesMin = 130;
	const unsigned int kRoutesMax = 200;
	const unsigned int kDetailMin = 425;
	const unsigned int kMinHeight = 220;

	unsigned int Less(unsigned int from, unsigned int by) {
		return from > by ? from - by : 0;
	}

	unsigned int Within(unsigned int value, unsigned int least, unsigned int most) {
		return (std::max)(least, (std::min)(value, most));
	}

	unsigned int WidthOf(const std::string& text) {
		return (unsigned int)Texthook::GetTextSize(text, 0).x;
	}

	// Gold is what can be clicked, and white on a band is the one picked, as in
	// the Forge.
	std::vector<ListColumn> RouteListColumns() {
		std::vector<ListColumn> columns(RaceRows::RouteColumns);
		columns[RaceRows::RouteName] = ListColumn("", 0, 1, 0, Gold, White);
		columns[RaceRows::RouteNote] = ListColumn("", 0, 0, kNoteGap, Grey, Disabled, true);
		return columns;
	}

	std::vector<ListColumn> TableColumns() {
		std::vector<ListColumn> columns(RaceRows::SplitColumns);
		columns[RaceRows::SplitName] = ListColumn("", 110, 1, 0, White);
		columns[RaceRows::SplitTime] = ListColumn("Time", 50, 0, kColumnGap, White);
		columns[RaceRows::SplitDelta] = ListColumn("+/-", 46, 0, kColumnGap, Grey);
		columns[RaceRows::SplitBest] = ListColumn("Best run", 56, 0, kColumnGap, Grey);
		columns[RaceRows::SplitSegment] = ListColumn("Best split", 60, 0, kColumnGap, Grey);
		columns[RaceRows::SplitLevel] = ListColumn("Level", 34, 0, kColumnGap, Grey);
		return columns;
	}
}

RunPage::RunPage(HookGroup* group) : shown(true), deleting(false), hasRoute(false),
		editable(false), routeCount(0), presetsMissing(false), pickedRow(-1), autoTicked(false),
		autoClicked(false), asked(ActNone), refreshedAt(0), wantsEdit(false), wantsRename(false),
		laidOut(false), laidOutWidth(0), laidOutHeight(0), laidOutRestore(false),
		routesWidth(kRoutesMin), nameWidth(0) {
	for (int action = 0; action < ActCount; action++) {
		actions[action] = NULL;
		asks[action] = { this, ActNone };
	}

	// Nothing is measured here: the columns and the layout wait for the first
	// draw, which is on the thread allowed to ask the game how wide text is.
	routesTitle = AddText(group, Silver, "Routes");
	routeList = new Listhook(group, 0, 0, 0, 0);
	AddAction(group, ActNew, "New");
	AddAction(group, ActCopy, "Copy");
	AddAction(group, ActEdit, "Edit");
	AddAction(group, ActDelete, "Delete");
	AddAction(group, ActRestore, "Restore presets");

	rule = new Boxhook(group, 0, 0, 1, 1);
	rule->SetColor(UI_SURFACE_RULE);
	rule->SetTransparency(BTNormal);

	routeName = AddText(group, White, "");
	start = new Buttonhook(group, 0, 0, kButtonHeight, "Start");
	start->SetLeftCallback(OnStart, this);
	stop = new Buttonhook(group, 0, 0, kButtonHeight, "Stop");
	stop->SetLeftCallback(OnStop, this);
	table = new Listhook(group, 0, 0, 0, 0);
	autoBox = new Checkhook(group, 0, 0, &autoTicked, "Start a run when a new character joins its first game");
	autoBox->SetLeftCallback(OnAuto, this);
}

Texthook* RunPage::AddText(HookGroup* group, TextColor color, const char* text) {
	Texthook* hook = new Texthook(group, 0, 0, "%s", text);
	hook->SetColor(color);
	return hook;
}

void RunPage::AddAction(HookGroup* group, Action action, const char* text) {
	Texthook* hook = AddText(group, Gold, text);
	hook->SetHoverColor(Tan);
	asks[action] = { this, action };
	hook->SetLeftCallback(OnAction, &asks[action]);
	actions[action] = hook;
}

// Noted here and done as the page next draws, which is where the routes are read.
bool RunPage::OnAction(bool up, Hook* hook, void* ask) {
	if (!hook->IsEnabled())
		return false;
	if (up) {
		const Ask* clicked = (const Ask*)ask;
		clicked->page->asked = clicked->action;
	}
	return true;
}

bool RunPage::OnStart(bool up, Hook* hook, void* page) {
	if (up) {
		Race::RequestRun(Race::RunStart);
		((RunPage*)page)->Changed();
	}
	return true;
}

bool RunPage::OnStop(bool up, Hook* hook, void* page) {
	if (up) {
		Race::RequestRun(Race::RunStop);
		((RunPage*)page)->Changed();
	}
	return true;
}

// The box has already turned itself over by the time it says it was clicked.
bool RunPage::OnAuto(bool up, Hook* hook, void* page) {
	if (up)
		((RunPage*)page)->autoClicked = true;
	return true;
}

unsigned int RunPage::MinWidth() {
	return 2 * UI_CONTENT_MARGIN + kRoutesMax + kRuleSpan + kDetailMin;
}

unsigned int RunPage::MinHeight() {
	return kMinHeight;
}

void RunPage::SetShown(bool show) {
	shown = show;
	Hook* const all[] = { routesTitle, routeList, rule, routeName, start, stop, table, autoBox };
	for (Hook* hook : all)
		hook->SetActive(show);
	for (int action = ActNew; action < ActCount; action++)
		actions[action]->SetActive(show && (action != ActRestore || presetsMissing));
	deleting = false;
	notice.clear();
	refreshedAt = 0;
}

bool RunPage::TakeEdit(std::string& id, bool& rename) {
	if (!wantsEdit)
		return false;
	wantsEdit = false;
	id = route.id;
	rename = wantsRename;
	return hasRoute;
}

void RunPage::Changed() {
	deleting = false;
	notice.clear();
	refreshedAt = 0;
}

// A click on a route picks it, and the route picked is the one runs are timed
// on. What is picked is read from the list's own selection.
void RunPage::TakePicks() {
	routeList->TakeClickedRow();
	const int row = routeList->GetSelectedRow();
	std::string id;
	if (row >= 0 && (size_t)row < routeRows.size() &&
			ListRows::KeyRest(routeRows[row].key, RaceRows::RouteKeys::kEdit, id) &&
			id != route.id) {
		Race::PickRoute(id);
		Changed();
	}

	// The splits are read and not picked: nothing stays lit behind a click.
	table->TakeClickedRow();
	if (table->GetSelectedRow() >= 0)
		table->ClearSelection();
}

void RunPage::TakeAuto() {
	if (!autoClicked)
		return;
	autoClicked = false;
	Race::SetAutoStart(autoTicked);
	Changed();
}

void RunPage::TakeAction() {
	const int action = asked;
	asked = ActNone;
	if (action == ActNone)
		return;

	// Deleting is asked for twice, and anything else done in between is not the
	// second asking.
	const bool confirming = deleting;
	Changed();
	if (action == ActDelete && !confirming) {
		deleting = true;
		return;
	}
	if (action == ActEdit) {
		wantsEdit = hasRoute;
		wantsRename = false;
		return;
	}

	const std::string id = route.id;
	std::string made;
	Race::EditRoutes([&](std::vector<Splits::Route>& routes) {
		const Splits::Route* found = NULL;
		for (const Splits::Route& each : routes) {
			if (each.id == id)
				found = &each;
		}

		if (action == ActNew) {
			made = RouteEdit::Add(routes, NULL);
		} else if (action == ActRestore) {
			RouteEdit::Restore(routes);
		} else if (!found) {
			return;
		} else if (action == ActCopy) {
			const Splits::Route original = *found;
			made = RouteEdit::Add(routes, &original);
		} else if (action == ActDelete && routes.size() > 1) {
			RouteEdit::Remove(routes, id);
		}
	});

	// A route just made is picked, and goes straight to having its name typed.
	if (!made.empty() && Race::PickRoute(made)) {
		Refresh();
		wantsEdit = hasRoute;
		wantsRename = true;
	}
}

void RunPage::Refresh() {
	std::vector<RaceRows::Row> routesNow, tableNow;
	bool found = false, enabled = false, canStart = false, canStop = false, here = false;
	std::string elsewhere;
	RunStore::Clock clock = RunStore::RealTime;

	Race::Read([&](const Race::View& view) {
		const Splits::Route* picked = NULL;
		for (const Splits::Route& each : view.routes) {
			if (each.id == view.route)
				picked = &each;
		}
		if (picked) {
			found = true;
			route = *picked;
		}

		const RunModel::Run* going = view.model.ActiveRun();
		routesNow = RaceRows::RouteList(view.routes, going ? going->route : std::string());
		tableNow = RaceRows::Runs(view.model, { picked, &view.bests, view.clock, view.now,
			view.classId, view.hardcore });

		enabled = view.enabled;
		editable = view.editable;
		routeCount = view.routes.size();
		autoTicked = view.autoStart;
		clock = view.clock;
		canStart = view.inGame && picked && !picked->splits.empty();
		canStop = going != NULL;
		here = going && picked && going->route == picked->id;
		if (going && !here)
			elsewhere = going->name;

		std::string ids;
		for (const Splits::Route& each : view.routes)
			ids += each.id + '\n';
		if (ids != routeIds) {
			routeIds = ids;
			presetsMissing = RouteEdit::PresetsMissing(view.routes);
		}
	});

	hasRoute = found;
	if (!hasRoute) {
		route = Splits::Route();
		editable = false;
	}

	if (RowLists::Push(routeList, routeRows, routesNow))
		MeasureRoutes();
	pickedRow = -1;
	for (size_t i = 0; i < routeRows.size(); i++) {
		if (routeRows[i].key == RaceRows::RouteKeys::kEdit + route.id)
			pickedRow = (int)i;
	}
	RowLists::Push(table, tableRows, tableNow);

	start->SetEnabled(canStart);
	stop->SetEnabled(canStop);

	if (!enabled)
		status = "The race timer is off. It is turned on under Race in the settings";
	else if (!notice.empty())
		status = notice;
	else if (!hasRoute)
		status = "No routes";
	else if (here)
		status = std::string("Run clock: ") + (clock == RunStore::RealTime ? "real time" : "time in game");
	else if (!elsewhere.empty())
		status = "A run on " + elsewhere + " is being timed";
	else if (route.splits.empty())
		status = "This route has no splits yet. Edit adds them";
	else if (!editable)
		status = "BH_routes.json could not be read, so routes cannot be changed here";
	else
		status.clear();
}

void RunPage::MeasureRoutes() {
	// Room for the note is kept beside every route, whichever has it: a run
	// starting would otherwise move everything right of the routes.
	const unsigned int note = kNoteGap + WidthOf(RaceRows::kRunningNote);
	unsigned int widest = 0;
	for (const RaceRows::Row& row : routeRows)
		widest = (std::max)(widest, WidthOf(row.cells[RaceRows::RouteName]) + note);
	const unsigned int wanted = Within(widest + kRowPad + routeList->GetGutterWidth(),
		kRoutesMin, kRoutesMax);
	if (wanted != routesWidth) {
		routesWidth = wanted;
		laidOut = false;
	}
}

// The one place anything is sized or positioned.
void RunPage::Layout(unsigned int width, unsigned int height) {
	if (!laidOut) {
		routeList->SetColumns(RouteListColumns());
		table->SetColumns(TableColumns());
	}
	laidOut = true;
	laidOutWidth = width;
	laidOutHeight = height;
	laidOutRestore = presetsMissing;

	const unsigned int textHeight = routesTitle->GetYSize();
	const unsigned int pitch = routeList->GetRowHeight();
	const unsigned int titleY = Less(kButtonHeight, textHeight) / 2;
	const unsigned int listY = kButtonHeight + kTitleGap;
	const unsigned int footY = Less(height, kBottomPad + textHeight);

	const unsigned int routesX = UI_CONTENT_MARGIN;
	const unsigned int ruleX = routesX + routesWidth + kRuleGapBefore;
	const unsigned int detailX = ruleX + 1 + kRuleGapAfter;
	const unsigned int detailWidth = Less(width, detailX + UI_CONTENT_MARGIN);

	// The routes, over what is done to the list of them. Putting the presets
	// back is offered on a line of its own, only while one is missing.
	const unsigned int actionsY = presetsMissing ? Less(footY, pitch) : footY;
	routesTitle->SetBaseX(routesX);
	routesTitle->SetBaseY(titleY);
	routeList->SetBaseX(routesX);
	routeList->SetBaseY(listY);
	routeList->SetSize(routesWidth, Less(actionsY, listY + kFootGap));
	unsigned int x = routesX;
	for (Action action : { ActNew, ActCopy, ActEdit, ActDelete }) {
		actions[action]->SetBaseX(x);
		actions[action]->SetBaseY(actionsY);
		x += actions[action]->GetXSize() + kActionGap;
	}
	actions[ActRestore]->SetBaseX(routesX);
	actions[ActRestore]->SetBaseY(footY);

	rule->SetBaseX(ruleX);
	rule->SetBaseY(0);
	rule->SetYSize(Less(height, FOOTER_RULE_CLEARANCE));

	// The route picked: its name, with the buttons against the far edge, over
	// its splits. The box's label stands on the line the actions are written on.
	const unsigned int stopX = detailX + Less(detailWidth, stop->GetXSize());
	const unsigned int startX = Less(stopX, kButtonGap + start->GetXSize());
	routeName->SetBaseX(detailX);
	routeName->SetBaseY(titleY);
	nameWidth = Less(startX, detailX + kNameGap);
	start->SetBaseX(startX);
	start->SetBaseY(0);
	stop->SetBaseX(stopX);
	stop->SetBaseY(0);

	const unsigned int boxY = Less(footY, CHECK_LABEL_TOP);
	table->SetBaseX(detailX);
	table->SetBaseY(listY);
	table->SetSize(detailWidth, Less(boxY, listY + kFootGap));
	autoBox->SetBaseX(detailX);
	autoBox->SetBaseY(boxY);
}

void RunPage::UpdateTexts() {
	actions[ActNew]->SetEnabled(editable);
	actions[ActCopy]->SetEnabled(editable && hasRoute);
	actions[ActEdit]->SetEnabled(editable && hasRoute);
	actions[ActDelete]->SetEnabled(editable && hasRoute && routeCount > 1);
	actions[ActDelete]->SetText(deleting ? "Delete?" : "Delete");
	actions[ActDelete]->SetColor(deleting ? Red : Gold);
	actions[ActRestore]->SetActive(shown && presetsMissing);
	actions[ActRestore]->SetEnabled(editable);

	routeName->SetText("%s", Texthook::Fit(route.name, nameWidth, 0).c_str());
}

// Giving a list rows lets go of what was selected in it, and so does a click
// on the route picked, which is not something to let go of.
void RunPage::KeepPicks() {
	if (routeList->GetSelectedRow() != pickedRow)
		routeList->SetSelectedRow(pickedRow);
}

void RunPage::Draw(unsigned int width, unsigned int height) {
	TakePicks();
	TakeAction();
	TakeAuto();

	const unsigned long long tick = BHGetTickCount();
	const bool due = refreshedAt == 0 || tick - refreshedAt >= kRefreshEveryMs;
	if (due) {
		refreshedAt = tick;
		Refresh();
	}
	// The texts are cut to the widths the layout comes to.
	const bool moved = !laidOut || width != laidOutWidth || height != laidOutHeight ||
		presetsMissing != laidOutRestore;
	if (moved)
		Layout(width, height);
	if (due || moved)
		UpdateTexts();
	KeepPicks();
}
