#include "CodexWindow.h"
#include "../../BH.h"
#include "../../Common.h"
#include "../../D2Helpers.h"
#include "../../TableReader.h"
#include "../Settings/SettingsRegistry.h"

using namespace Drawing;

// What the window asks for the first time it is opened. A window with nothing
// remembered in UI.ini takes a share of the canvas instead, never less than
// this, so on anything past a vanilla resolution it opens larger.
#define CODEX_WINDOW_WIDTH	400
#define CODEX_WINDOW_HEIGHT	420

void CodexWindow::OnLoad() {
	LoadConfig();

	RegisterToggleKey(Settings::Category::Input, "Codex",
		"Opens the codex of runewords, uniques, sets, recipes and bases.");

	CreateUI("Codex", "Codex", CODEX_WINDOW_WIDTH, CODEX_WINDOW_HEIGHT);

	// One search box for every panel. Each panel supplies its own hint as it
	// comes forward, so the box still says what searching it will do.
	GetUI()->EnableSearch("Search");

	// A panel here is opened to be read, so the caret goes straight in the box.
	SetFocusSearchOnOpen(true);

	// Only the panel in front filters. The panels hold unrelated things, so
	// filtering the other four would be work nobody asked for.
	SetSearchEveryPanel(false);

	// Panel order is the order they are added in.
	AddPanel(new RunewordTab(GetUI()));
	AddPanel(new UniqueTab(GetUI()));
	AddPanel(new SetTab(GetUI()));
	AddPanel(new RecipeTab(GetUI()));
	AddPanel(new BaseTab(GetUI()));

	// UI.ini remembers whether it starts collapsed, so leave that alone here.
	// OnLoop() drives visibility, so it stays hidden until we are in a game.
}

// The window's own name, which opens it on whichever panel was last in front.
// The panels add their own.
std::vector<ChatCommand> CodexWindow::GetOwnCommands() {
	std::vector<ChatCommand> commands;
	commands.push_back(ChatCommand{ "codex", {}, "<search>",
		"Opens the codex on the tab last in front" });
	return commands;
}

// Only the part that is particular to this window; opening it on the right panel
// with the right search is every window's behaviour and belongs to the base.
void CodexWindow::OnUserInput(const wchar_t* msg, bool fromGame, bool* block) {
	if (!Tables::isInitialized()) {
		*block = true;
		Print("\377c4Codex:\377c0 still loading game data, try again in a moment.");
		return;
	}
	WindowModule::OnUserInput(msg, fromGame, block);
}
