#pragma once
#include <string>
#include <vector>
#include "../Window/WindowModule.h"
#include "SettingsPanel.h"

// The settings window. It owns the window, its hotkey, the reload hotkey and one
// panel per category; the panels are built from what the modules have registered,
// so nothing here knows what any particular setting is.
//
// Everything about being a window - the shared search box and footer, cycling
// between panels, what escape does, how it is sized and remembered - belongs to
// WindowModule, which the Info window shares.
class SettingsWindow : public WindowModule {
	private:
		// Which categories there are and the order their tabs appear in. The
		// window's to decide rather than the registry's: order of registration is
		// order of module loading.
		//
		// The names come from Settings::Category so that a module and this list
		// cannot disagree about what a tab is called.
		static const char* Categories[];
		static unsigned int CategoryCount();

		// The reload hotkey, which used to belong to Maphack for no better reason
		// than that Maphack happened to own the settings window's visibility.
		unsigned int reloadConfig;
		bool legacyReloadConfigHotkey;

		unsigned int builtVersion;	// registry version the panels were built for
		bool showedDirty;			// what the footer last said about unsaved work

		// Builds a panel for every category that has anything registered in it.
		// Done as it draws rather than on loading, because modules register as they
		// load and the load order is alphabetical: a window built at load time
		// would see only the modules that sort before it.
		void EnsurePanels();

	protected:
		std::vector<ChatCommand> GetOwnCommands();

	public:
		SettingsWindow() : WindowModule("Settings", "Show Settings", "VK_NUMPAD8"),
			reloadConfig(0),
			// Ctrl-R has always worked unless the config says otherwise, so that is
			// the default a missing key falls back to.
			legacyReloadConfigHotkey(true),
			builtVersion(0),
			showedDirty(false) {};

		void OnLoad();
		void LoadConfig();
		void OnLoop();
		void OnDraw();
		void OnKey(bool up, BYTE key, LPARAM lParam, bool* block);
};
