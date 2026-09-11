#pragma once
#include <string>
#include <vector>
#include "../../Catalogue/Source.h"
#include "../../Catalogue/StatIndex.h"
#include "../Window/UIPanel.h"

// The Horadric Cube panel, laid out and driven the same way as the runewords
// panel.
//
// A view onto the stat index, scoped to the recipes. What the player types
// becomes a query with one text criterion, so the panel holds no recipes of its
// own and matches nothing for itself: what it draws is the answer it was given,
// gathered under the headings the catalogue put its recipes in.
class RecipeTab : public UIPanel {
	private:
		Drawing::Listhook* list;

		// Sits beside the window rather than inside the tab, which is why it is a
		// bare Tooltiphook rather than one of the tab's hooks.
		Drawing::Tooltiphook* summary;

		std::vector<StatIndex::Result> results;

		// The recipe on each list row, and NULL on the heading rows, so a row
		// number means the same thing however the list is folded.
		std::vector<const Catalogue::Source*> rowRecipes;
		unsigned int shownHeadings;	// headings pushed, for the status line
		bool foldOnPush;			// fold the headings on the next rows pushed

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
				const Catalogue::Source& recipe);
		void UpdateSummary();

	public:
		RecipeTab(Drawing::UI* ui);

		std::vector<ChatCommand> GetCommands();
		void OnDraw();
		bool OnKey(bool up, BYTE key);
		void OnClose();
		void Search(const std::string& text);
		void OnSearchSubmitted();
		std::string GetSearchPlaceholder();
		std::string GetStatus();
};
