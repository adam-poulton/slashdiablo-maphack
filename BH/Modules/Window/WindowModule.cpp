#include "WindowModule.h"
#include "../../BH.h"
#include "../../Common.h"

using namespace Drawing;

WindowModule::WindowModule(std::string moduleName, std::string toggleName,
		std::string toggleDefaultKey) : Module(moduleName),
	ui(NULL),
	toggleName(toggleName),
	toggleDefaultKey(toggleDefaultKey),
	wasOpen(false),
	lastFront(NULL),
	searchEveryPanel(false),
	focusSearchOnOpen(true) {
	InitializeCriticalSection(&crit);
}

WindowModule::~WindowModule() {
	for (unsigned int i = 0; i < panels.size(); i++)
		delete panels[i];
	panels.clear();
	// After the panels, since destroying the window destroys the tabs they were
	// built on.
	delete ui;
	ui = NULL;
	DeleteCriticalSection(&crit);
}

void WindowModule::CreateUI(std::string title, std::string configKey,
		unsigned int xSize, unsigned int ySize) {
	if (ui)
		return;
	ui = new UI(title, configKey, xSize, ySize);
	// Safe because the panels measure themselves against the window.
	ui->SetResizable(true);
	ui->EnableFooter();
}

void WindowModule::AddPanel(UIPanel* panel) {
	if (!panel)
		return;
	Lock();
	panels.push_back(panel);
	Unlock();
}

void WindowModule::LoadConfig() {
	BH::config->ReadToggle(toggleName, toggleDefaultKey, true, Toggles[toggleName]);
}

void WindowModule::MpqLoaded() {
	Lock();
	for (unsigned int i = 0; i < panels.size(); i++)
		panels[i]->MpqLoaded();
	Unlock();
}

UIPanel* WindowModule::GetActivePanel() {
	for (unsigned int i = 0; i < panels.size(); i++) {
		if (panels[i]->IsActive())
			return panels[i];
	}
	return NULL;
}

UIPanel* WindowModule::GetFrontPanel() {
	if (!ui || panels.empty())
		return NULL;
	UITab* current = ui->GetActiveTab();
	for (unsigned int i = 0; i < panels.size(); i++) {
		if (panels[i]->GetTab() == current)
			return panels[i];
	}
	return panels[0];
}

UIPanel* WindowModule::GetPanelForCommand(const std::string& command) {
	for (unsigned int i = 0; i < panels.size(); i++) {
		if (panels[i]->HandlesCommand(command))
			return panels[i];
	}
	return NULL;
}

// The window's own commands first, then whatever each panel answers to.
std::vector<ChatCommand> WindowModule::GetCommands() {
	std::vector<ChatCommand> commands = GetOwnCommands();
	for (unsigned int i = 0; i < panels.size(); i++) {
		std::vector<ChatCommand> own = panels[i]->GetCommands();
		commands.insert(commands.end(), own.begin(), own.end());
	}
	return commands;
}

bool WindowModule::IsOpen() {
	return ui && Toggles[toggleName].state && ui->IsVisible() && !ui->IsMinimized();
}

void WindowModule::ShowPanel(UIPanel* panel) {
	if (ui && panel)
		ui->SetCurrentTab(panel->GetTab());
}

// Tab and shift tab walk the row of panels. The search box passes the key up
// rather than typing it, so it is the window's to spend on this.
void WindowModule::CyclePanel(int delta) {
	if (panels.size() < 2)
		return;

	UIPanel* current = GetFrontPanel();
	int count = (int)panels.size();
	int index = 0;
	for (int i = 0; i < count; i++) {
		if (panels[i] == current) {
			index = i;
			break;
		}
	}
	index = ((index + delta) % count + count) % count;

	ShowPanel(panels[index]);
	Lock();
	panels[index]->OnOpen();
	Unlock();
}

void WindowModule::ShowWindow(bool show) {
	if (!ui)
		return;
	if (show)
		Toggles[toggleName].state = true;
	// Collapsing also deactivates the controls, so they can't swallow input.
	ui->SetMinimized(!show);
	ui->SetVisible(Toggles[toggleName].state);
	if (show) {
		ui->SetActive(true);
		UI::Sort(ui);
	}
}

void WindowModule::OnLoop() {
	if (!ui)
		return;
	// Collapse as well as hide, so the controls stop taking input.
	if (!Toggles[toggleName].state)
		ui->SetMinimized(true);
	ui->SetVisible(Toggles[toggleName].state);
	CheckOpenState();
}

// Several of the ways a window opens and closes - ctrl clicking its collapsed
// title bar, right clicking it, escape - never go through ShowWindow(), so the
// panels are told by watching for the window having appeared or gone away.
void WindowModule::CheckOpenState() {
	bool open = IsOpen();
	if (open != wasOpen) {
		Lock();
		for (unsigned int i = 0; i < panels.size(); i++) {
			if (open)
				panels[i]->OnOpen();
			else
				panels[i]->OnClose();
		}
		if (open) {
			if (focusSearchOnOpen && ui->HasSearch()) {
				Inputhook* box = ui->GetSearchBox();
				box->SetCursorPosition(box->GetText().length());
				box->SetFocused(true);
			}
		} else {
			// The box is shared, so it is the window that empties it. A panel
			// clearing its own filter cannot: the text is not its to clear.
			if (ui->HasSearch()) {
				Inputhook* box = ui->GetSearchBox();
				box->SetFocused(false);
				box->Clear();
				lastSearch.clear();
			}
		}
		Unlock();
	}
	wasOpen = open;
}

void WindowModule::OnGameExit() {
	// Nothing is drawn outside a game, so stop taking input on the menus too.
	if (!ui)
		return;
	ui->SetVisible(false);
	CheckOpenState();
}

void WindowModule::DeliverSearch(const std::string& text) {
	if (searchEveryPanel) {
		for (unsigned int i = 0; i < panels.size(); i++)
			panels[i]->Search(text);
		return;
	}
	UIPanel* front = GetFrontPanel();
	if (front)
		front->Search(text);
}

void WindowModule::UpdateChrome() {
	UIPanel* front = GetFrontPanel();
	bool frontChanged = (front != lastFront);
	if (frontChanged) {
		lastFront = front;
		if (front) {
			std::string hint = front->GetSearchPlaceholder();
			if (hint.length() > 0)
				ui->SetSearchPlaceholder(hint);
		}
	}

	if (ui->HasSearch()) {
		std::string text = ui->GetSearchBox()->GetText();
		// A panel coming forward is told as well as one whose search changed: it
		// was not listening while the text was being typed.
		if (text != lastSearch || frontChanged) {
			lastSearch = text;
			DeliverSearch(text);
		}
	}
}

void WindowModule::SetSearchText(const std::string& text) {
	if (!ui)
		return;
	Lock();
	if (ui->HasSearch()) {
		Inputhook* box = ui->GetSearchBox();
		box->SetText("%s", text.c_str());
		box->SetTextPos(0);
		box->ResetSelection();
		box->SetCursorPosition(text.length());
	}
	// Delivered here rather than left to the next draw, so a caller that puts a
	// search in and then looks at the panel sees the result of it.
	lastSearch = text;
	DeliverSearch(text);
	Unlock();
}

void WindowModule::OnDraw() {
	if (!ui)
		return;
	if (!Toggles[toggleName].state || ui->IsMinimized())
		return;

	Lock();
	// Before the panels draw, so a search typed this frame is applied this frame.
	UpdateChrome();
	for (unsigned int i = 0; i < panels.size(); i++)
		panels[i]->OnDraw();

	UIPanel* front = GetFrontPanel();
	// After the panels have drawn, so a panel acting on enter is acting on rows
	// the search has already brought up to date.
	if (front && ui->HasSearch() && ui->GetSearchBox()->TakeSubmitted())
		front->OnSearchSubmitted();

	// Likewise last, so the footer reports what the panels just worked out.
	ui->SetFooterRight(front ? front->GetStatus() : std::string());
	Unlock();
}

std::string WindowModule::ReadCommandArgument(const wchar_t* msg) {
	std::string text;
	for (int i = 0; msg && i < 64 && msg[i] >= L' ' && msg[i] <= L'~'; i++)
		text += (char)msg[i];
	return Trim(text);
}

void WindowModule::OnUserInput(const wchar_t* msg, bool fromGame, bool* block) {
	*block = true;

	std::string text = ReadCommandArgument(msg);

	// A command naming no panel leaves whichever was last in front.
	UIPanel* target = GetPanelForCommand(GetInvokedCommand());
	if (!target)
		target = GetFrontPanel();

	if (target) {
		// Brought forward before the search is handed over, since the search box is
		// shared and delivers to whichever panel is in front.
		ShowPanel(target);
		SetSearchText(text);
		// Unconditionally, since an already open window is not a change of state
		// for the window to notice.
		target->OnOpen();
	}
	ShowWindow(true);
}

void WindowModule::OnKey(bool up, BYTE key, LPARAM lParam, bool* block) {
	Toggle& toggle = Toggles[toggleName];
	if (toggle.toggle != 0 && key == toggle.toggle) {
		*block = true;
		if (up)
			ShowWindow(!ui || ui->IsMinimized());
		return;
	}

	if (!toggle.state || !ui || ui->IsMinimized())
		return;

	// First refusal, so a panel can spend a key the window would otherwise take.
	UIPanel* active = GetActivePanel();
	if (active && active->OnKey(up, key)) {
		*block = true;
		return;
	}

	// Ctrl-F reaches the search box wherever the caret happens to be. Only ever
	// while the box is unfocused: a focused box swallows every ctrl combination
	// itself, so this is never in the way of typing.
	bool ctrl = ((GetKeyState(VK_LCONTROL) & 0x80) || (GetKeyState(VK_RCONTROL) & 0x80));
	if (ctrl && key == 0x46 && ui->HasSearch()) {
		*block = true;
		if (!up) {
			Inputhook* box = ui->GetSearchBox();
			box->SetFocused(true);
			// Selected rather than cleared, so the previous query is still there
			// to go back to if this was a slip.
			box->SelectAll();
		}
		return;
	}

	if (key == VK_TAB) {
		*block = true;
		if (!up) {
			bool shift = (GetKeyState(VK_LSHIFT) & 0x80) || (GetKeyState(VK_RSHIFT) & 0x80);
			CyclePanel(shift ? -1 : 1);
		}
		return;
	}

	// Closes the window rather than opening the game menu. A focused search box
	// drops focus on this same press without swallowing it, so one press does
	// both rather than the first press appearing to do nothing.
	if (key == VK_ESCAPE) {
		*block = true;
		if (up)
			ShowWindow(false);
	}
}
