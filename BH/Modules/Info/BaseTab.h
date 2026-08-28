#pragma once
#include <string>
#include <vector>
#include "../../ItemDescription.h"
#include "../Window/UIPanel.h"

// One base item, pre-formatted for display. Everything a row shows is here;
// what the summary panel shows is read back out of the tables by the code.
struct BaseItemRecord {
	std::string code;			// "7gd"
	std::string name;			// "Colossus Blade"
	std::string typeName;		// "Sword", which is also the heading it sits under
	std::string tierName;		// "Normal", "Exceptional" or "Elite"
	std::string searchKey;		// lowercased name/type/tier/code, for filtering

	ItemDescription::Tier tier;
	int level;					// the level from which it starts dropping
	int group;					// where its type falls in the order of headings

	BaseItemRecord() : tier(ItemDescription::TierNormal), level(0), group(0) {};
};

// The base items panel: every item the game can drop before anything is made of
// it, under its item type. Laid out and driven the same way as the sets panel.
class BaseTab : public UIPanel {
	private:
		Drawing::Listhook* list;

		// Sits beside the window rather than inside the tab, which is why it is a
		// bare Tooltiphook rather than one of the tab's hooks.
		Drawing::Tooltiphook* summary;

		std::vector<BaseItemRecord> items;
		std::vector<const BaseItemRecord*> matches;

		// The item each list row stands for, NULL for the type headings. The list
		// interleaves headings with items, so a row number is not an index into
		// matches.
		std::vector<const BaseItemRecord*> rowItems;
		unsigned int shownTypes;	// headings pushed, for the status line
		bool foldOnPush;			// fold the types on the next rows pushed

		std::string query;			// active filter, always lowercase
		int shownSummary;			// row the summary was built for, or -1
		bool basesLoaded;
		bool needsRefresh;

		// Tab size the contents were last fitted to, so a resize is noticed.
		unsigned int laidOutWidth;
		unsigned int laidOutHeight;

		// The only place anything is sized or positioned.
		void ApplyLayout();

		void BuildItems();
		void ApplyFilter();
		void PushRows();

		std::vector<Drawing::TooltipLine> BuildSummaryLines(const BaseItemRecord* item);
		void UpdateSummary();

	public:
		BaseTab(Drawing::UI* ui);

		void MpqLoaded();
		std::vector<ChatCommand> GetCommands();
		void OnDraw();
		bool OnKey(bool up, BYTE key);
		void OnClose();
		void Search(const std::string& text);
		void OnSearchSubmitted();
		std::string GetSearchPlaceholder();
		std::string GetStatus();

		unsigned int GetItemCount() { return items.size(); };
};
