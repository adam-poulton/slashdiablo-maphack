#pragma once
#include "../Window/WindowModule.h"

class MapMarkersPanel;

// The window the map markers are styled in. It owns the window and its hotkey;
// what is in it is MapMarkersPanel's, which is built from the registry and so
// knows nothing of which module's markers it is showing.
class MapMarkersWindow : public WindowModule {
	private:
		MapMarkersPanel* panel;
		bool showedDirty;		// what the footer last said about unsaved work

	protected:
		std::vector<ChatCommand> GetOwnCommands();

	public:
		MapMarkersWindow() : WindowModule("MapMarkers", "Show Map Markers", "None"),
			panel(NULL), showedDirty(false) {};

		void OnLoad();
		void OnDraw();
};
