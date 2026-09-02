#pragma once
#include <string>
#include <vector>
#include "../../Catalogue/Source.h"
#include "../../Catalogue/StatIndex.h"
#include "../Window/UIPanel.h"

// The base items panel: every item the game can drop before anything is made of
// it, under its item type. Laid out and driven the same way as the sets panel.
//
// A view onto the stat index, scoped to the bases. What the player types becomes
// a query with one text criterion, so the panel holds no bases of its own and
// matches nothing for itself: what it draws is the answer it was given.
class BaseTab : public UIPanel {
	private:
		Drawing::Listhook* list;

		// Sits beside the window rather than inside the tab, which is why it is a
		// bare Tooltiphook rather than one of the tab's hooks.
		Drawing::Tooltiphook* summary;

		std::vector<StatIndex::Result> results;

		// The base each list row stands for, NULL for the type headings. The list
		// interleaves headings with bases, so a row number is not an index into
		// results.
		std::vector<const Catalogue::Source*> rowBases;
		unsigned int shownTypes;	// headings pushed, for the status line
		bool foldOnPush;			// fold the types on the next rows pushed

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
		BaseTab(Drawing::UI* ui);

		std::vector<ChatCommand> GetCommands();
		void OnDraw();
		bool OnKey(bool up, BYTE key);
		void OnClose();
		void Search(const std::string& text);
		void OnSearchSubmitted();
		std::string GetSearchPlaceholder();
		std::string GetStatus();
};
