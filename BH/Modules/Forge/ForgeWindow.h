#pragma once
#include "../Window/WindowModule.h"

class MarkerPanel;
class MonsterPanel;

// The Forge: the window things on the map are styled in, a tab per kind of
// thing. It owns the window and its hotkey; each tab is a MarkerPanel, built from
// the registry, which knows nothing of which module's markers it shows.
//
// What is styled here is kept in the Forge's own file rather than in
// BH_settings.cfg (see ForgeFile).
class ForgeWindow : public WindowModule {
	private:
		MarkerPanel* clickables;
		MonsterPanel* monsters;
		bool showedDirty;		// what the footer last said about unsaved work

	protected:
		std::vector<ChatCommand> GetOwnCommands();

	public:
		ForgeWindow() : WindowModule("Forge", "Show Forge", "None"),
			clickables(NULL), monsters(NULL), showedDirty(false) {};

		void OnLoad();
		void OnDraw();
};
