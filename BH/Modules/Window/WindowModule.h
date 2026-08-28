#pragma once
#include <map>
#include <string>
#include <vector>
#include "../Module.h"
#include "../../Config.h"
#include "../../Drawing.h"
#include "UIPanel.h"

// A module that owns one window: its hotkey, its panels, the chrome they share,
// and the input that belongs to the window rather than to any panel.
//
// All of this was written for the Info window first and then wanted by the
// settings window, which is why it is a base class rather than part of either.
// A window feature added here reaches both; one added to a window reaches only
// that window, and the two drift apart. They already had.
class WindowModule : public Module {
	private:
		CRITICAL_SECTION crit;
		Drawing::UI* ui;
		std::vector<UIPanel*> panels;

		std::string toggleName;
		std::string toggleDefaultKey;

		bool wasOpen;			// so opening and closing can be noticed
		std::string lastSearch;	// last text seen in the shared search box
		UIPanel* lastFront;		// panel the chrome was last brought up to date for

		bool searchEveryPanel;	// every panel filters, not just the one in front
		bool focusSearchOnOpen;

		void CheckOpenState();
		void CyclePanel(int delta);

		// Brings the search box and the footer up to date, and hands the panels
		// whatever is in the box. Assumes the lock is held.
		void UpdateChrome();

		// Hands a search to whichever panels are listening. Assumes the lock is
		// held.
		void DeliverSearch(const std::string& text);

	protected:
		std::map<std::string, Toggle> Toggles;

		// Builds the window. Called from the subclass's OnLoad before any panel
		// is added, since a panel measures itself against the window.
		void CreateUI(std::string title, std::string configKey,
			unsigned int xSize, unsigned int ySize);
		void AddPanel(UIPanel* panel);

		Drawing::UI* GetUI() { return ui; };
		const std::vector<UIPanel*>& GetPanels() { return panels; };

		// Whether the search box filters every panel or only the one in front,
		// and whether it takes the caret when the window opens. A window opened
		// to be read wants the caret in the box; one opened to be browsed with
		// the arrow keys does not.
		void SetSearchEveryPanel(bool every) { searchEveryPanel = every; };
		void SetFocusSearchOnOpen(bool focus) { focusSearchOnOpen = focus; };

		// The commands the window answers itself, as opposed to those its panels
		// answer. Merged with the panels' by GetCommands().
		virtual std::vector<ChatCommand> GetOwnCommands() {
			return std::vector<ChatCommand>();
		};

		// Whatever was typed after the command. A command with no argument leaves
		// the pointer just past the end of it, so this reads defensively and keeps
		// only printable characters.
		static std::string ReadCommandArgument(const wchar_t* msg);

	public:
		WindowModule(std::string moduleName, std::string toggleName,
			std::string toggleDefaultKey);
		virtual ~WindowModule();

		void Lock() { EnterCriticalSection(&crit); };
		void Unlock() { LeaveCriticalSection(&crit); };

		// The panel in front, and only while the window is on screen.
		UIPanel* GetActivePanel();

		// The panel in front, whether or not the window is on screen.
		UIPanel* GetFrontPanel();

		UIPanel* GetPanelForCommand(const std::string& command);

		bool IsOpen();

		// Collapses to the title bar rather than hiding.
		void ShowWindow(bool show);
		void ShowPanel(UIPanel* panel);

		// Puts text in the shared search box and hands it straight to the panels,
		// for a search that arrived from outside the window.
		void SetSearchText(const std::string& text);

		void LoadConfig();
		void MpqLoaded();
		std::vector<ChatCommand> GetCommands();

		void OnLoop();
		void OnGameExit();
		void OnDraw();
		void OnKey(bool up, BYTE key, LPARAM lParam, bool* block);

		// Opens the window on whichever panel answers the command that was typed,
		// with whatever followed it put in the search box. Every window that
		// answers a command wants this, so it is here rather than written out once
		// per window.
		void OnUserInput(const wchar_t* msg, bool fromGame, bool* block);
};
