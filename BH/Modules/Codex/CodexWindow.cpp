#include "CodexWindow.h"
#include "../../BH.h"
#include "../../Catalogue/RunewordCatalogue.h"
#include "../../Catalogue/SetCatalogue.h"
#include "../../Catalogue/UniqueCatalogue.h"
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

// The filter button's mark: grey while nothing is being filtered on, gold once
// something is, so that a window filtering with its rows put away still says so.
#define CODEX_FILTER_IDLE		0xD0
#define CODEX_FILTER_IDLE_LIT	0x20
#define CODEX_FILTER_ON			0x0D
#define CODEX_FILTER_ON_LIT		0x0C

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

	// Built before the panels, which are handed it as they are added.
	conditions = new QueryBuilder(GetUI());
	GetUI()->EnableSearchButton(IconFilter, [this]() {
		conditions->SetShown(!conditions->IsShown());
	});

	// Panel order is the order they are added in.
	AddConditionPanel(new RunewordTab(GetUI(), conditions), RunewordCatalogue::Kind);
	AddConditionPanel(new UniqueTab(GetUI(), conditions), UniqueCatalogue::Kind);
	AddConditionPanel(new SetTab(GetUI(), conditions), SetCatalogue::Kind);
	AddPanel(new RecipeTab(GetUI()));
	AddPanel(new BaseTab(GetUI()));

	// UI.ini remembers whether it starts collapsed, so leave that alone here.
	// OnLoop() drives visibility, so it stays hidden until we are in a game.
}

void CodexWindow::AddConditionPanel(UIPanel* panel, const std::string& kind) {
	AddPanel(panel);
	conditionKinds[panel] = kind;
}

// The conditions are the window's rather than any one panel's, so it is the
// window that puts them away - as it is the window that empties the search box
// they sit under. The codex opens on a clean list, and a filter left behind
// would be one more thing it had not put back.
void CodexWindow::OnOpenStateChanged(bool open) {
	if (!open && conditions)
		conditions->Clear();
}

void CodexWindow::UpdateConditions() {
	UIPanel* front = GetFrontPanel();
	std::map<UIPanel*, std::string>::iterator kind =
		conditionKinds.find(front);
	bool offered = (kind != conditionKinds.end());

	GetUI()->SetSearchButtonShown(offered);
	conditions->SetOffered(offered);
	if (!offered) {
		// Undrawn rather than cleared: the panel in front has no use for them,
		// but the one behind it does and is coming back to them.
		GetUI()->SetSearchExtraHeight(0);
		return;
	}

	conditions->SetKind(kind->second);

	// Everything the window is not already spending, less what a list needs to
	// be a list at all. The window is never resized to make room, so this is
	// what stops the rows taking the panel's last pixel.
	unsigned int height = GetUI()->GetYSize();
	unsigned int spent = GetUI()->GetChromeAboveHeight() +
		GetUI()->GetChromeBelowHeight() - GetUI()->GetSearchExtraHeight();
	unsigned int spare = (height > spent + UI_MIN_CONTENT_HEIGHT) ?
		(height - spent - UI_MIN_CONTENT_HEIGHT) : 0;
	conditions->SetHeightBudget(spare);

	conditions->OnDraw();
	GetUI()->SetSearchExtraHeight(conditions->GetHeight());

	Buttonhook* button = GetUI()->GetSearchButton();
	button->SetPressed(conditions->IsShown());
	bool filtering = conditions->GetActiveCount() > 0;
	button->SetColor(filtering ? CODEX_FILTER_ON : CODEX_FILTER_IDLE);
	button->SetHoverColor(filtering ? CODEX_FILTER_ON_LIT : CODEX_FILTER_IDLE_LIT);
}

// Before the panels are drawn, so a condition changed this frame is one the
// panels answer this frame, and so the room the rows take is out of the tab's
// height before the tab measures itself against it.
void CodexWindow::OnDraw() {
	if (conditions && IsOpen()) {
		Lock();
		UpdateConditions();
		Unlock();
	}
	WindowModule::OnDraw();
}

// The stat picker hangs over the panel, so escape shuts that before it shuts the
// window: one press undoes one thing, which is what a player pressing it twice
// in a row expects.
//
// Decided on the press and remembered until the release, because the window is
// closed on the release and by then the picker this press shut is no longer
// open to have been the reason.
void CodexWindow::OnKey(bool up, BYTE key, LPARAM lParam, bool* block) {
	if (key == VK_ESCAPE && conditions && IsOpen()) {
		if (!up)
			escapeShutPicker = conditions->ClosePicker();
		if (escapeShutPicker) {
			*block = true;
			if (up)
				escapeShutPicker = false;
			return;
		}
	}
	WindowModule::OnKey(up, key, lParam, block);
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
