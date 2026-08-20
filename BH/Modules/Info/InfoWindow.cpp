#include "InfoWindow.h"
#include "../../BH.h"
#include "../../Common.h"
#include "../../D2Helpers.h"
#include "../../TableReader.h"

using namespace Drawing;

#define INFO_WINDOW_WIDTH	400
#define INFO_WINDOW_HEIGHT	420
#define INFO_TOGGLE_NAME	"Info Window"

void InfoWindow::OnLoad() {
	LoadConfig();

	infoUI = new UI("Info", INFO_WINDOW_WIDTH, INFO_WINDOW_HEIGHT);

	runewordTab = new RunewordTab(infoUI);
	tabs.push_back(runewordTab);

	// Whether the window starts collapsed to its title bar is remembered in
	// UI.ini, so leave that alone here. Visibility is driven by OnLoop() so the
	// window stays hidden until we are actually in a game.
}

void InfoWindow::LoadConfig() {
	BH::config->ReadToggle(INFO_TOGGLE_NAME, "VK_NUMPAD9", true, Toggles[INFO_TOGGLE_NAME]);
}

void InfoWindow::MpqLoaded() {
	Lock();
	for (unsigned int i = 0; i < tabs.size(); i++)
		tabs[i]->MpqLoaded();
	Unlock();
}

InfoTab* InfoWindow::GetActiveTab() {
	for (unsigned int i = 0; i < tabs.size(); i++) {
		if (tabs[i]->IsActive())
			return tabs[i];
	}
	return NULL;
}

void InfoWindow::ShowTab(InfoTab* tab) {
	if (infoUI && tab)
		infoUI->SetCurrentTab(tab->GetTab());
}

void InfoWindow::ShowWindow(bool show) {
	if (!infoUI)
		return;
	if (show)
		Toggles[INFO_TOGGLE_NAME].state = true;
	// Collapsing the window to its title bar also deactivates its controls, so
	// they can't swallow input while it is out of the way.
	infoUI->SetMinimized(!show);
	infoUI->SetVisible(Toggles[INFO_TOGGLE_NAME].state);
	if (show) {
		infoUI->SetActive(true);
		UI::Sort(infoUI);
	}
}

void InfoWindow::OnLoop() {
	if (!infoUI)
		return;
	// Disabling the feature entirely hides the window; collapse it as well so
	// its controls stop responding to clicks and keys.
	if (!Toggles[INFO_TOGGLE_NAME].state)
		infoUI->SetMinimized(true);
	infoUI->SetVisible(Toggles[INFO_TOGGLE_NAME].state);
}

void InfoWindow::OnGameExit() {
	// Nothing is drawn outside a game, so make sure the window agrees and stops
	// taking input while we are back on the menus.
	if (infoUI)
		infoUI->SetVisible(false);
}

void InfoWindow::OnDraw() {
	if (!infoUI)
		return;
	if (!Toggles[INFO_TOGGLE_NAME].state || infoUI->IsMinimized())
		return;

	Lock();
	for (unsigned int i = 0; i < tabs.size(); i++)
		tabs[i]->OnDraw();
	Unlock();
}

void InfoWindow::OnKey(bool up, BYTE key, LPARAM lParam, bool* block) {
	Toggle& toggle = Toggles[INFO_TOGGLE_NAME];
	if (toggle.toggle != 0 && key == toggle.toggle) {
		*block = true;
		if (up)
			ShowWindow(!infoUI || infoUI->IsMinimized());
		return;
	}

	if (!toggle.state || !infoUI || infoUI->IsMinimized())
		return;

	// The active tab gets first refusal, so it can use escape to back out of its
	// own state before the window closes.
	InfoTab* active = GetActiveTab();
	if (active && active->OnKey(up, key)) {
		*block = true;
		return;
	}

	// Escape closes the window rather than opening the game menu. When a text
	// box has focus it consumes escape first to drop that focus, so the second
	// press closes the window.
	if (key == VK_ESCAPE) {
		*block = true;
		if (up)
			ShowWindow(false);
	}
}

void InfoWindow::OnUserInput(const wchar_t* msg, bool fromGame, bool* block) {
	*block = true;

	// A command with no argument leaves the parameter pointer just past the end
	// of the command, so read defensively and keep only printable characters.
	std::string text;
	for (int i = 0; msg && i < 64 && msg[i] >= L' ' && msg[i] <= L'~'; i++)
		text += (char)msg[i];
	text = Trim(text);

	if (!Tables::isInitialized()) {
		Print("\377c4Info:\377c0 still loading game data, try again in a moment.");
		return;
	}

	// Results are shown in the window rather than repeated into the chat log.
	if (runewordTab) {
		runewordTab->Search(text);
		ShowTab(runewordTab);
	}
	ShowWindow(true);
}
