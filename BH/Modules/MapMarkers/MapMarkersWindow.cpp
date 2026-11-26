#include "MapMarkersWindow.h"
#include "MapMarkersPanel.h"
#include "../Settings/SettingsRegistry.h"

#define MAP_MARKERS_WINDOW_WIDTH	478
#define MAP_MARKERS_WINDOW_HEIGHT	250

void MapMarkersWindow::OnLoad() {
	LoadConfig();

	RegisterToggleKey(Settings::Category::Input, "Map markers window",
		"Opens the window chests and containers are styled in. Bound to nothing, "
		"it is still reached by typing .markers in chat.");

	CreateUI("Map markers", "Map Markers", MAP_MARKERS_WINDOW_WIDTH,
		MAP_MARKERS_WINDOW_HEIGHT);

	panel = new MapMarkersPanel(GetUI());
	AddPanel(panel);

	// Saved as it closes and offered back while open, as the settings window is:
	// these are settings like any other, kept in the same file.
	GetUI()->SetOnMinimized([]() -> void { Settings::Persist(); });
}

std::vector<ChatCommand> MapMarkersWindow::GetOwnCommands() {
	std::vector<ChatCommand> commands;
	commands.push_back(ChatCommand{ "markers", {}, "",
		"Opens the map markers window" });
	return commands;
}

void MapMarkersWindow::OnDraw() {
	bool dirty = Settings::IsDirty();
	if (dirty != showedDirty) {
		showedDirty = dirty;
		GetUI()->SetFooterAction(dirty ? "revert changes" : "",
			[]() -> void { Settings::Revert(); });
	}

	WindowModule::OnDraw();
}
