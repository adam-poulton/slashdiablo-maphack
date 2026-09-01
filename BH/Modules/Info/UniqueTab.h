#pragma once
#include <string>
#include <vector>
#include "../../Catalogue/UniqueCatalogue.h"
#include "../../ItemDescription.h"
#include "../Window/UIPanel.h"

// A unique in the panel's list: the catalogue's source, and beside it what only
// a panel drawing the source has any use for.
struct UniqueRow {
	const Catalogue::Source* source;
	std::string searchKey;		// lowercased name/base/type, used for filtering

	// What the source's properties do to the numbers its base carries. Worked
	// out on first view, since it is only wanted for the row being read.
	ItemDescription::Modifiers modifiers;
	bool modifiersLoaded;
};

// The unique items panel, laid out and driven the same way as the runewords
// panel.
class UniqueTab : public UIPanel {
	private:
		Drawing::Listhook* list;

		// Sits beside the window rather than inside the tab, which is why it is a
		// bare Tooltiphook rather than one of the tab's hooks.
		Drawing::Tooltiphook* summary;

		std::vector<UniqueRow> uniques;
		std::vector<UniqueRow*> matches;
		std::string query;			// active filter, always lowercase
		int shownSummary;			// row the summary was built for, or -1
		bool uniquesLoaded;
		bool needsRefresh;

		// Tab size the contents were last fitted to, so a resize is noticed.
		unsigned int laidOutWidth;
		unsigned int laidOutHeight;

		// The only place anything is sized or positioned.
		void ApplyLayout();

		void BuildUniques();
		void LoadModifiers(UniqueRow* unique);
		void ApplyFilter();
		void PushRows();

		std::vector<Drawing::TooltipLine> BuildSummaryLines(UniqueRow* unique);
		void UpdateSummary();

	public:
		UniqueTab(Drawing::UI* ui);

		void MpqLoaded();
		std::vector<ChatCommand> GetCommands();
		void OnDraw();
		bool OnKey(bool up, BYTE key);
		void OnClose();
		void Search(const std::string& text);
		void OnSearchSubmitted();
		std::string GetSearchPlaceholder();
		std::string GetStatus();
};
