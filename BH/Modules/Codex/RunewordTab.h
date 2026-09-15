#pragma once
#include <string>
#include <vector>
#include "../../Catalogue/StatIndex.h"
#include "../Window/UIPanel.h"

class QueryBuilder;

// The runewords panel, laid out and driven the same way as the uniques panel.
//
// A view onto the stat index, scoped to the runewords. What the player
// types becomes a text criterion, and the conditions the window shares
// between its panels become criteria beside it, so the panel holds no runewords of its own
// and matches nothing for itself: what it draws is the answer it was given.
class RunewordTab : public UIPanel {
	private:
		Drawing::Listhook* list;

		// Sits beside the window rather than inside the tab, which is why it is a
		// bare Tooltiphook rather than one of the tab's hooks.
		Drawing::Tooltiphook* summary;

		// The stat conditions the window shares between its panels, and the
		// revision the list was last built against, so a condition changed
		// while another panel was in front is noticed on coming back to this
		// one.
		QueryBuilder* conditions;
		unsigned int conditionsRevision;

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
		RunewordTab(Drawing::UI* ui, QueryBuilder* conditions);

		std::vector<ChatCommand> GetCommands();
		void OnDraw();
		bool OnKey(bool up, BYTE key);
		void OnClose();
		void Search(const std::string& text);
		void OnSearchSubmitted();
		std::string GetSearchPlaceholder();
		std::string GetStatus();
};
