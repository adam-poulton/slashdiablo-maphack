#include "SessionWindow.h"
#include "../../Common.h"
#include "../../D2Helpers.h"
#include "../../StringUtil.h"
#include "../Settings/SettingsRegistry.h"
#include "HudLayout.h"
#include "Session.h"
#include "SessionPanel.h"

// What a window nobody has sized yet opens at.
#define SESSION_WINDOW_WIDTH	660
#define SESSION_WINDOW_HEIGHT	400

void SessionWindow::OnLoad() {
	LoadConfig();

	RegisterToggleKey(Settings::Category::Input, "Session window",
		"Opens the session window: what this sitting has come to, and its games. "
		"Bound to nothing, it is still reached by typing .session in chat.");

	CreateUI("Session", "Session", SESSION_WINDOW_WIDTH, SESSION_WINDOW_HEIGHT);
	// One box for every tab, each saying what it searches as it comes forward.
	// The caret stays out of it on opening: the window is opened to be read,
	// often in the middle of a game.
	GetUI()->EnableSearch("Search");
	SetFocusSearchOnOpen(false);
	SetSearchEveryPanel(false);

	AddPanel(new OverviewPanel(GetUI()));
	AddPanel(new GamesPanel(GetUI()));
	AddPanel(new KillsPanel(GetUI()));
	AddPanel(new DropsPanel(GetUI()));

	GetUI()->SetFooterAction("position HUD", []() -> void {
		Session::RequestHudPlacing(true);
	});
}

std::vector<ChatCommand> SessionWindow::GetOwnCommands() {
	return {
		{ "session", {}, "<new>", "Opens the session window" },
		{ "hud", {}, "<view|move>", "Changes or moves the HUD" },
	};
}

// A HUD left being placed would go on taking the mouse with nothing on screen
// to say why.
void SessionWindow::OnOpenStateChanged(bool open) {
	if (!open)
		Session::RequestHudPlacing(false);
}

void SessionWindow::OnUserInput(const wchar_t* msg, bool fromGame, bool* block) {
	const std::string asked = ToLower(ReadCommandArgument(msg));

	if (GetInvokedCommand() == "hud") {
		*block = true;
		if (!Session::IsEnabled()) {
			Print("\377c4Session:\377c0 the session tracker is off.");
			return;
		}
		if (asked == "move") {
			Session::RequestHudPlacing(true);
			return;
		}
		const HudLayout::View view = HudLayout::ViewNamed(asked);
		if (view == HudLayout::ViewCount) {
			std::string views;
			for (int each = 0; each < HudLayout::ViewCount; each++)
				views += std::string(each ? ", " : "") + ToLower(HudLayout::NameOf((HudLayout::View)each));
			Print("\377c4Session:\377c0 .hud takes a view (%s) or move.", views.c_str());
			return;
		}
		Session::RequestHudView(view);
		return;
	}

	if (asked == "new") {
		*block = true;
		Session::RequestNew();
		Print("\377c4Session:\377c0 started a new session.");
		return;
	}

	WindowModule::OnUserInput(msg, fromGame, block);
}
