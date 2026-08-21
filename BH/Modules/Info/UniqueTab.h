#pragma once
#include <string>
#include <vector>
#include "InfoTab.h"

// A property entry as it appears in the game's tables: what it grants, its
// parameter and the range it rolls.
struct UniqueProperty {
	std::string code;
	std::string param;
	int min;
	int max;
};

// A single unique item, pre-formatted for display.
struct UniqueRecord {
	std::string name;			// "Harlequin Crest"
	std::string baseName;		// "Shako"
	std::string itemType;		// "Cap", from the base's item type
	int requiredLevel;			// 0 if the item has no requirement
	std::string searchKey;		// lowercased name/base/type, used for filtering

	std::vector<UniqueProperty> properties;
	std::vector<std::string> stats;		// built on first view
	bool statsLoaded;
};

// The unique items panel: every unique the realm has enabled, its base item, and
// what it rolls. Laid out and driven the same way as the runewords panel.
class UniqueTab : public InfoTab {
	private:
		Drawing::Inputhook* searchBox;
		Drawing::Listhook* list;
		Drawing::Texthook* statusText;

		// The summary of whichever item is being pointed at, drawn alongside the
		// window rather than over the list. It is a plain tooltip that knows
		// nothing about unique items; BuildSummaryLines() is what makes it one.
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

		// Fits the contents to the tab's current size. Everything that depends
		// on how big the panel is lives here and nowhere else.
		void ApplyLayout();

		void BuildUniques();
		void LoadStats(UniqueRecord* unique);
		void ApplyFilter();
		void PushRows();
		void UpdateStatus();

		// Turns an item into the lines that describe it. The only part of the
		// summary that knows what a unique item is.
		void BuildSummaryLines(UniqueRecord* unique,
			std::vector<Drawing::TooltipLine>& lines);
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
