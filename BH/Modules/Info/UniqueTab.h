#pragma once
#include <string>
#include <vector>
#include "../../Catalogue/StatIndex.h"
#include "../Window/UIPanel.h"

// The unique items panel, laid out and driven the same way as the runewords
// panel.
//
// A view onto the stat index, scoped to the uniques. What the player types
// becomes a query with one text criterion, so the panel holds no uniques of its
// own and matches nothing for itself: what it draws is the answer it was given.
class UniqueTab : public UIPanel {
	private:
		Drawing::Listhook* list;

		// Sits beside the window rather than inside the tab, which is why it is a
		// bare Tooltiphook rather than one of the tab's hooks.
		Drawing::Tooltiphook* summary;

		std::vector<StatIndex::Result> results;
		std::string search;			// what the player typed, always lowercase
		int shownSummary;			// row the summary was built for, or -1
		bool catalogueLoaded;
		bool needsRefresh;

		// Tab size the contents were last fitted to, so a resize is noticed.
		unsigned int laidOutWidth;
		unsigned int laidOutHeight;

		// The only place anything is sized or positioned.
		void ApplyLayout();

		void RunQuery();
		void PushRows();

		std::vector<Drawing::TooltipLine> BuildSummaryLines(
				const Catalogue::Source& source);
		void UpdateSummary();

	public:
		UniqueTab(Drawing::UI* ui);

		std::vector<ChatCommand> GetCommands();
		void OnDraw();
		bool OnKey(bool up, BYTE key);
		void OnClose();
		void Search(const std::string& text);
		void OnSearchSubmitted();
		std::string GetSearchPlaceholder();
		std::string GetStatus();
};
