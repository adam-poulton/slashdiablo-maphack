#include "ForgeWindow.h"
#include "ForgeFile.h"
#include "MarkerPanel.h"
#include "../Settings/SettingsRegistry.h"
#include "../../BH.h"
#include "../../D2Ptrs.h"

// What a window nobody has sized yet opens at. The panels keep it from being
// made smaller than their contents, whatever it is asked to open at.
#define FORGE_WINDOW_WIDTH	400
#define FORGE_WINDOW_HEIGHT	300

void ForgeWindow::OnLoad() {
	LoadConfig();

	RegisterToggleKey(Settings::Category::Input, "Forge window",
		"Opens the Forge, where how chests and containers are marked on the map "
		"is styled. Bound to nothing, it is still reached by typing .forge in chat.");

	CreateUI("Forge", "Forge", FORGE_WINDOW_WIDTH, FORGE_WINDOW_HEIGHT);
	// The tabs are what the Forge styles, so they are shown while there is only
	// one of them, as they will be when there are more.
	GetUI()->SetLoneTabShown(true);

	clickables = new MarkerPanel(GetUI(), "Clickables", Settings::Category::Clickables);
	AddPanel(clickables);

	// Saved as it closes and offered back while open, as the settings window is.
	// A file that cannot be saved is said in the footer while the window is open,
	// and here, as the window goes, where the footer can no longer be read.
	GetUI()->SetOnMinimized([]() -> void {
		Settings::Persist();
		const std::string& problem = BH::forge->GetProblem();
		if (!problem.empty() && D2CLIENT_GetPlayerUnit())
			PrintText(Red, "\377c4Forge:\377c1 %s, so your changes are not saved.",
				problem.c_str());
	});
}

std::vector<ChatCommand> ForgeWindow::GetOwnCommands() {
	std::vector<ChatCommand> commands;
	commands.push_back(ChatCommand{ "forge", {}, "", "Opens the Forge" });
	return commands;
}

void ForgeWindow::OnDraw() {
	bool dirty = Settings::IsDirty();
	if (dirty != showedDirty) {
		showedDirty = dirty;
		GetUI()->SetFooterAction(dirty ? "revert changes" : "",
			[]() -> void { Settings::Revert(); });
	}

	WindowModule::OnDraw();
}
