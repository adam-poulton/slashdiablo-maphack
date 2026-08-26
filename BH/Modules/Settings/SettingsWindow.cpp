#include "SettingsWindow.h"
#include "../../About.h"
#include "../../BH.h"

using namespace Drawing;

// What the window asks for the first time it is opened. With nothing remembered
// in UI.ini it takes a share of the canvas instead, never less than this, so on
// anything past a vanilla resolution it opens larger.
#define SETTINGS_WINDOW_WIDTH	460
#define SETTINGS_WINDOW_HEIGHT	400

const char* SettingsWindow::Categories[] = {
	Settings::Category::Map,
	Settings::Category::Items,
	Settings::Category::Filter,
	Settings::Category::Input,
	Settings::Category::Lobby,
};

unsigned int SettingsWindow::CategoryCount() {
	return sizeof(Categories) / sizeof(const char*);
}

void SettingsWindow::OnLoad() {
	LoadConfig();

	// "Settings" rather than the version string. The title bar says which window
	// this is, which matters now there is more than one of them; the version goes
	// in the footer, where there is room for it and it does not change what
	// section of UI.ini the geometry is remembered under.
	CreateUI("Settings", "Settings", SETTINGS_WINDOW_WIDTH, SETTINGS_WINDOW_HEIGHT);

	GetUI()->EnableSearch("Search all settings by name");

	// A search here is for a setting whose tab you do not know - that being the
	// reason to search rather than browse - so every panel answers it with the
	// matches from every tab. Told to every panel rather than to the one in front,
	// so that changing tabs part way through a search does not change what is on
	// screen: the results are the same wherever they are read from.
	SetSearchEveryPanel(true);

	// And the caret stays out of the box, so the arrow keys reach the settings.
	// The Info window wants the opposite, which is why this is a switch and not
	// two implementations.
	SetFocusSearchOnOpen(false);

	// Closing the window is what writes the settings back out. This used to happen
	// inside UI itself, which meant collapsing any window at all wrote them.
	GetUI()->SetOnMinimized([]() -> void { Settings::Persist(); });

	GetUI()->SetFooterLeft(About::Version());
}

void SettingsWindow::LoadConfig() {
	WindowModule::LoadConfig();
	BH::config->ReadKey("Reload Config", "VK_NUMPAD0", reloadConfig);
	BH::config->ReadBoolean("Ctrl+R Reload Config", legacyReloadConfigHotkey);
}

std::vector<ChatCommand> SettingsWindow::GetOwnCommands() {
	std::vector<ChatCommand> commands;
	commands.push_back(ChatCommand{ "settings", { "options" }, "<search>",
		"Opens the settings window" });
	return commands;
}

void SettingsWindow::EnsurePanels() {
	if (builtVersion == Settings::Version())
		return;
	builtVersion = Settings::Version();

	// The tabs there turned out to be. Nothing registered in a category means no
	// tab for it: a category is a heading over settings, and an empty one is a
	// heading over nothing.
	std::vector<std::string> categories;
	for (unsigned int c = 0; c < CategoryCount(); c++) {
		if (!Settings::InCategory(Categories[c]).empty())
			categories.push_back(Categories[c]);
	}

	for (unsigned int c = 0; c < categories.size(); c++) {
		bool built = false;
		const std::vector<UIPanel*>& existing = GetPanels();
		for (unsigned int p = 0; p < existing.size() && !built; p++)
			built = (existing[p]->GetTab()->GetName().compare(categories[c]) == 0);
		if (!built)
			AddPanel(new SettingsPanel(categories[c], categories, GetUI()));
	}

	// Every panel is told what all the tabs are, not only the ones just built: a
	// search reaches settings on tabs other than the one in front, so a panel has
	// to know what the others are and what order they come in. Safe to walk as
	// settings panels, since they are the only kind this window has.
	const std::vector<UIPanel*>& panels = GetPanels();
	for (unsigned int p = 0; p < panels.size(); p++)
		((SettingsPanel*)panels[p])->SetCategories(categories);
}

// Where the settings are noticed changing, whoever changed them. On the loop
// rather than as it draws, because what modules do about it installs patches.
void SettingsWindow::OnLoop() {
	Settings::Poll();
	WindowModule::OnLoop();
}

void SettingsWindow::OnDraw() {
	EnsurePanels();

	// Offered rather than merely announced. The window writes the settings out as
	// it closes, so by the time someone reads the footer the useful thing is not
	// being told there is unsaved work but being given the way back from it.
	bool dirty = Settings::IsDirty();
	if (dirty != showedDirty) {
		showedDirty = dirty;
		GetUI()->SetFooterAction(dirty ? "revert changes" : "",
			[]() -> void { Settings::Revert(); });
	}

	WindowModule::OnDraw();
}

void SettingsWindow::OnKey(bool up, BYTE key, LPARAM lParam, bool* block) {
	// Before the window's own keys, since reloading is not something the settings
	// window is doing to itself - it works whether or not the window is open.
	bool ctrl = ((GetKeyState(VK_LCONTROL) & 0x80) || (GetKeyState(VK_RCONTROL) & 0x80));
	if ((legacyReloadConfigHotkey && key == 0x52 && ctrl) ||
			(reloadConfig != 0 && key == reloadConfig)) {
		*block = true;
		if (up)
			BH::ReloadConfig();
		return;
	}

	WindowModule::OnKey(up, key, lParam, block);
}
