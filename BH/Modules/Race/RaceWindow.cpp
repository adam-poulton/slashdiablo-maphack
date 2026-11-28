#include "RaceWindow.h"
#include "../../Common.h"
#include "../../D2Helpers.h"
#include "../../StringUtil.h"
#include "../Settings/SettingsRegistry.h"
#include "Race.h"
#include "RaceHudLayout.h"
#include "RacePanel.h"

// What a window nobody has sized yet opens at.
#define RACE_WINDOW_WIDTH	660
#define RACE_WINDOW_HEIGHT	400

void RaceWindow::OnLoad() {
	LoadConfig();

	RegisterToggleKey(Settings::Category::Input, "Race window",
		"Opens the race window: the routes there are to time a run on, and the "
		"run being timed. "
		"Bound to nothing, it is still reached by typing .race in chat.");

	CreateUI("Race", "Race", RACE_WINDOW_WIDTH, RACE_WINDOW_HEIGHT);
	AddPanel(new RacePanel(GetUI()));

	GetUI()->SetFooterAction("position HUD", []() -> void {
		Race::RequestHudPlacing(true);
	});
}

std::vector<ChatCommand> RaceWindow::GetOwnCommands() {
	return {
		{ "race", {}, "<hud view|hud move>", "Opens the race window" },
		{ "run", {}, "<start|stop|split|undo|skip>", "Times a run" },
		{ "route", {}, "<name>", "Picks the route runs are timed on" },
	};
}

// A HUD left being placed would go on taking the mouse with nothing on screen
// to say why. And the route picked here is a setting, written as the window
// goes so that it is not left as unsaved work for the settings window to offer
// to revert.
void RaceWindow::OnOpenStateChanged(bool open) {
	if (open)
		return;
	Race::RequestHudPlacing(false);
	Settings::Persist();
}

void RaceWindow::OnUserInput(const wchar_t* msg, bool fromGame, bool* block) {
	const std::string asked = ToLower(ReadCommandArgument(msg));

	if (GetInvokedCommand() == "run") {
		*block = true;
		const std::pair<const char*, Race::RunAction> actions[] = {
			{ "start", Race::RunStart }, { "stop", Race::RunStop },
			{ "reset", Race::RunStop }, { "split", Race::RunSplit },
			{ "undo", Race::RunUndo }, { "skip", Race::RunSkip },
		};
		for (const auto& action : actions) {
			if (asked == action.first) {
				Race::RequestRun(action.second);
				return;
			}
		}
		Print("\377c4Race:\377c0 .run takes start, stop, split, undo or skip.");
		return;
	}

	if (GetInvokedCommand() == "route") {
		*block = true;
		if (!asked.empty()) {
			Race::RequestRoute(asked);
			return;
		}
		Print("\377c4Race:\377c0 .route takes one of:");
		for (const std::string& name : Race::RouteNames())
			Print("  %s", name.c_str());
		return;
	}

	// ".race hud" and what to do with it.
	const std::string hud = "hud";
	if (asked.compare(0, hud.size(), hud) == 0) {
		*block = true;
		if (!Race::IsEnabled()) {
			Print("\377c4Race:\377c0 the race timer is off.");
			return;
		}
		const std::string what = Trim(asked.substr(hud.size()));
		if (what == "move") {
			Race::RequestHudPlacing(true);
			return;
		}
		const RaceHudLayout::View view = RaceHudLayout::ViewNamed(what);
		if (view == RaceHudLayout::ViewCount) {
			std::string views;
			for (int each = 0; each < RaceHudLayout::ViewCount; each++)
				views += std::string(each ? ", " : "") +
					ToLower(RaceHudLayout::NameOf((RaceHudLayout::View)each));
			Print("\377c4Race:\377c0 .race hud takes a view (%s) or move.", views.c_str());
			return;
		}
		Race::RequestHudView(view);
		return;
	}

	WindowModule::OnUserInput(msg, fromGame, block);
}
