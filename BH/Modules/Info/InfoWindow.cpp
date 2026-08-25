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
	// Safe because the tabs measure themselves against the window.
	infoUI->SetResizable(true);

	// Tab order is the order they are added in.
	tabs.push_back(new RunewordTab(infoUI));
	tabs.push_back(new UniqueTab(infoUI));
	tabs.push_back(new SetTab(infoUI));
	tabs.push_back(new RecipeTab(infoUI));
	tabs.push_back(new BaseTab(infoUI));

	// UI.ini remembers whether it starts collapsed, so leave that alone here.
	// OnLoop() drives visibility, so it stays hidden until we are in a game.
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

// The tab in front, whether or not the window is on screen.
InfoTab* InfoWindow::GetCurrentTab() {
	if (!infoUI || tabs.empty())
		return NULL;
	UITab* current = infoUI->GetActiveTab();
	for (unsigned int i = 0; i < tabs.size(); i++) {
		if (tabs[i]->GetTab() == current)
			return tabs[i];
	}
	return tabs[0];
}

InfoTab* InfoWindow::GetTabForCommand(const std::string& command) {
	for (unsigned int i = 0; i < tabs.size(); i++) {
		if (tabs[i]->HandlesCommand(command))
			return tabs[i];
	}
	return NULL;
}

// The window's own name first, which opens it on whichever tab was last in
// front, and then whatever each tab answers to.
std::vector<ChatCommand> InfoWindow::GetCommands() {
	std::vector<ChatCommand> commands;
	commands.push_back(ChatCommand{ "info", {}, "<search>",
		"Opens the window on the tab last in front" });
	for (unsigned int i = 0; i < tabs.size(); i++) {
		std::vector<ChatCommand> own = tabs[i]->GetCommands();
		commands.insert(commands.end(), own.begin(), own.end());
	}
	return commands;
}

void InfoWindow::ShowTab(InfoTab* tab) {
	if (infoUI && tab)
		infoUI->SetCurrentTab(tab->GetTab());
}

// Tab and shift tab walk the row of tabs. The search boxes pass the key up
// rather than typing it, so it is the window's to spend on this.
void InfoWindow::CycleTab(int delta) {
	if (tabs.size() < 2)
		return;

	InfoTab* current = GetCurrentTab();
	int count = (int)tabs.size();
	int index = 0;
	for (int i = 0; i < count; i++) {
		if (tabs[i] == current) {
			index = i;
			break;
		}
	}
	index = ((index + delta) % count + count) % count;

	ShowTab(tabs[index]);
	// So the caret lands in the tab that just came forward.
	Lock();
	tabs[index]->OnOpen();
	Unlock();
}

void InfoWindow::ShowWindow(bool show) {
	if (!infoUI)
		return;
	if (show)
		Toggles[INFO_TOGGLE_NAME].state = true;
	// Collapsing also deactivates the controls, so they can't swallow input.
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
	// Collapse as well as hide, so the controls stop taking input.
	if (!Toggles[INFO_TOGGLE_NAME].state)
		infoUI->SetMinimized(true);
	infoUI->SetVisible(Toggles[INFO_TOGGLE_NAME].state);
	CheckOpenState();
}

// Several of the ways the window opens and closes - ctrl clicking its collapsed
// title bar, right clicking it, escape - never go through ShowWindow(), so the
// tabs are told by watching for the window having appeared or gone away.
void InfoWindow::CheckOpenState() {
	bool open = Toggles[INFO_TOGGLE_NAME].state && infoUI->IsVisible() &&
		!infoUI->IsMinimized();
	if (open != wasOpen) {
		Lock();
		for (unsigned int i = 0; i < tabs.size(); i++) {
			if (open)
				tabs[i]->OnOpen();
			else
				tabs[i]->OnClose();
		}
		Unlock();
	}
	wasOpen = open;
}

void InfoWindow::OnGameExit() {
	// Nothing is drawn outside a game, so stop taking input on the menus too.
	if (!infoUI)
		return;
	infoUI->SetVisible(false);
	CheckOpenState();
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

	// First refusal, so a tab can use escape before the window closes.
	InfoTab* active = GetActiveTab();
	if (active && active->OnKey(up, key)) {
		*block = true;
		return;
	}

	if (key == VK_TAB) {
		*block = true;
		if (!up) {
			bool shift = (GetKeyState(VK_LSHIFT) & 0x80) || (GetKeyState(VK_RSHIFT) & 0x80);
			CycleTab(shift ? -1 : 1);
		}
		return;
	}

	// Closes the window rather than opening the game menu. A focused text box
	// takes the first press to drop focus, so the second one lands here.
	if (key == VK_ESCAPE) {
		*block = true;
		if (up)
			ShowWindow(false);
	}
}

void InfoWindow::OnUserInput(const wchar_t* msg, bool fromGame, bool* block) {
	*block = true;

	// A command with no argument leaves the pointer just past the end of it, so
	// read defensively and keep only printable characters.
	std::string text;
	for (int i = 0; msg && i < 64 && msg[i] >= L' ' && msg[i] <= L'~'; i++)
		text += (char)msg[i];
	text = Trim(text);

	if (!Tables::isInitialized()) {
		Print("\377c4Info:\377c0 still loading game data, try again in a moment.");
		return;
	}

	// ".info" names no tab, so it leaves whichever was last in front.
	InfoTab* target = GetTabForCommand(GetInvokedCommand());
	if (!target)
		target = GetCurrentTab();

	if (target) {
		target->Search(text);
		ShowTab(target);
		// Unconditionally, since an already open window is not a change of state
		// for CheckOpenState() to notice.
		target->OnOpen();
	}
	ShowWindow(true);
}
