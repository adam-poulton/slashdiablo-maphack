#pragma once
#include <string>
#include <vector>
#include "../../Catalogue/StatIndex.h"
#include "../Window/UIPanel.h"

// The set items panel, laid out and driven the same way as the runewords and
// uniques panels.
//
// A view onto the stat index, scoped to the pieces of a set. What the player
// types becomes a query with one text criterion, so the panel holds no pieces
// of its own and matches nothing for itself: what it draws is the answer it was
// given. A set's own bonuses are a source of their own, which the panel asks
// the catalogue for when it describes one of that set's pieces.
class SetTab : public UIPanel {
	private:
		Drawing::Listhook* list;

		// Sits beside the window rather than inside the tab, which is why it is a
		// bare Tooltiphook rather than one of the tab's hooks.
		Drawing::Tooltiphook* summary;

		std::vector<StatIndex::Result> results;

		// The piece each list row stands for, NULL for the set headings. The list
		// interleaves headings with pieces, so a row number is not an index into
		// the results.
		std::vector<const Catalogue::Source*> rowPieces;
		unsigned int shownSets;		// headings pushed, for the status line
		bool foldOnPush;			// fold the sets on the next rows pushed

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
				const Catalogue::Source& piece);
		void UpdateSummary();

	public:
		SetTab(Drawing::UI* ui);

		std::vector<ChatCommand> GetCommands();
		void OnDraw();
		bool OnKey(bool up, BYTE key);
		void OnClose();
		void Search(const std::string& text);
		void OnSearchSubmitted();
		std::string GetSearchPlaceholder();
		std::string GetStatus();
};
