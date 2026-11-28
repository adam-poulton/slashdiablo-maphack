#pragma once
#include <string>
#include "../Window/UIPanel.h"

class EditPage;
class RunPage;

// The race window's one screen. It opens on the run page: the routes there are,
// and the one picked with Start and Stop over its splits. Editing a route swaps
// that for the edit page, in the same place, until Done.
class RacePanel : public UIPanel {
	private:
		RunPage* run;
		EditPage* edit;
		bool editing;

		void ShowRun();

	public:
		explicit RacePanel(Drawing::UI* ui);
		~RacePanel();

		void OnDraw();
		void OnClose();
		bool OnKey(bool up, BYTE key);
		std::string GetStatus();

		unsigned int GetMinWidth();
		unsigned int GetMinHeight();
};
