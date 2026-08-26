#pragma once
#include <string>
#include <vector>
#include <Windows.h>
#include "../../Drawing.h"
#include "../Module.h"

// One panel of a window. Subclasses build their own controls into the panel in
// their constructor and refresh them from OnDraw().
//
// The window owns the frame, the hotkey, the row of tab headings, the search box
// and the footer; a panel only needs to care about its own contents. Anything a
// panel would otherwise have to build for itself - a search box, a status line -
// belongs to the window instead, so that every panel of every window gets the
// same one and an improvement to it reaches all of them.
class UIPanel {
	protected:
		Drawing::UITab* tab;

	public:
		UIPanel(std::string name, Drawing::UI* ui) : tab(new Drawing::UITab(name, ui)) {};
		virtual ~UIPanel() {};

		Drawing::UITab* GetTab() { return tab; };
		bool IsActive() { return tab->IsActive(); };

		// Called once the MPQ data tables are available.
		virtual void MpqLoaded() {};

		// The chat commands that open this panel, lowercased and without their
		// leading dot. Answering one is what makes the window switch to this
		// panel and hand it whatever was typed after the command.
		//
		// Listed rather than answered one at a time, so the window can say what
		// its panels can be asked as well as answer it.
		virtual std::vector<ChatCommand> GetCommands() {
			return std::vector<ChatCommand>();
		};

		// Whether this panel answers a command, which is its own list searched.
		bool HandlesCommand(const std::string& command) {
			std::vector<ChatCommand> commands = GetCommands();
			for (unsigned int i = 0; i < commands.size(); i++) {
				if (commands[i].Answers(command))
					return true;
			}
			return false;
		};

		// The hint the window's search box shows while this panel is in front.
		// Empty leaves whatever the window last set, for a panel that does not
		// care.
		virtual std::string GetSearchPlaceholder() { return std::string(); };

		// Filter the panel's contents. Called with whatever is in the window's
		// search box, whether it was typed there or arrived from a chat command,
		// and again when this panel comes forward, since it was not listening
		// while the text was being typed. An empty search clears the filter.
		virtual void Search(const std::string& text) {};

		// What the panel has to say about what it is showing - a count, or why it
		// is showing nothing. Drawn on the right of the window's footer while
		// this panel is in front, and read every frame, so it should be cheap.
		virtual std::string GetStatus() { return std::string(); };

		// Called when enter is pressed in the window's search box, for the panel
		// in front. The box consumes enter rather than typing it, so this is the
		// only way a panel hears about it. Called after OnDraw(), so whatever the
		// search just changed is already in place to act on.
		virtual void OnSearchSubmitted() {};

		// Called every frame before the window is drawn, whether or not this
		// panel is the active one, so a panel can load data lazily.
		virtual void OnDraw() {};

		// Called for keys the window did not consume itself, and only while this
		// panel is active. Return true to swallow the key.
		virtual bool OnKey(bool up, BYTE key) { return false; };

		// Called for every panel when the window opens, however it was opened.
		// Somewhere to put whatever should be ready the moment it appears. The
		// caret is not a panel's business: the window decides whether its search
		// box takes focus on opening.
		virtual void OnOpen() {};

		// Called for every panel when the window closes, however it was closed.
		// Somewhere to drop whatever shouldn't still be there next time it opens.
		virtual void OnClose() {};
};
