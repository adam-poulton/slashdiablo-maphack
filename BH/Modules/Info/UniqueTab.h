#pragma once
#include <string>
#include <vector>
#include "InfoTab.h"

// A property entry as it appears in the game's tables.
struct UniqueProperty {
	std::string code;
	std::string param;
	int min;
	int max;
};

// A single unique item, pre-formatted for display.
struct UniqueRecord {
	std::string name;			// "Harlequin Crest"
	std::string code;			// the base item's code, "uap"
	std::string baseName;		// "Shako"
	std::string itemType;		// "Cap", from the base's item type
	int requiredLevel;			// what the unique itself asks for, 0 if nothing
	std::string searchKey;		// lowercased name/base/type, used for filtering

	std::vector<UniqueProperty> properties;
	std::vector<std::string> stats;		// built on first view
	bool statsLoaded;
};

// The unique items panel, laid out and driven the same way as the runewords
// panel.
class UniqueTab : public InfoTab {
	private:
		Drawing::Inputhook* searchBox;
		Drawing::Listhook* list;
		Drawing::Texthook* statusText;

		// Sits beside the window rather than inside the tab, which is why it is a
		// bare Tooltiphook rather than one of the tab's hooks.
		Drawing::Tooltiphook* summary;

		std::vector<UniqueRecord> uniques;
		std::vector<const UniqueRecord*> matches;
		std::string query;			// active filter, always lowercase
		std::string lastBoxText;	// last text seen in the search box
		int shownSummary;			// row the summary was built for, or -1
		bool uniquesLoaded;
		bool needsRefresh;

		// Tab size the contents were last fitted to, so a resize is noticed.
		unsigned int laidOutWidth;
		unsigned int laidOutHeight;

		// The only place anything is sized or positioned.
		void ApplyLayout();

		void BuildUniques();
		void LoadStats(UniqueRecord* unique);
		void ApplyFilter();
		void PushRows();
		void UpdateStatus();

		std::vector<Drawing::TooltipLine> BuildSummaryLines(UniqueRecord* unique);
		void UpdateSummary();

	public:
		UniqueTab(Drawing::UI* ui);

		void MpqLoaded();
		bool HandlesCommand(const std::string& command);
		void OnDraw();
		bool OnKey(bool up, BYTE key);
		void OnOpen();
		void OnClose();
		void Search(const std::string& text);

		unsigned int GetUniqueCount() { return uniques.size(); };
};
