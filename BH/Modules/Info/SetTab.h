#pragma once
#include <string>
#include <vector>
#include "../../ItemDescription.h"
#include "../../PropertyStats.h"
#include "../Window/UIPanel.h"

// One piece of a set, pre-formatted for display.
struct SetItemRecord {
	std::string name;			// "Tal Rasha's Fine-Spun Cloth"
	std::string code;			// the base item's code, "lbl"
	std::string baseName;		// "Mesh Belt"
	std::string itemType;		// "Belt", from the base's item type
	std::string setName;		// "Tal Rasha's Wrappings"
	int setIndex;				// into the set list, or -1 for an orphan
	int requiredLevel;			// what the piece itself asks for, 0 if nothing
	std::string searchKey;		// lowercased name/base/type/set, for filtering

	// The piece's own always-on properties, and the ones more pieces unlock.
	std::vector<PropertyStats::Property> own;
	std::vector<PropertyStats::Property> partial;

	std::vector<std::string> ownStats;		// built on first view
	std::vector<std::string> partialStats;	// each already carrying its count

	// What the piece's own always-on properties do to the numbers its base
	// carries. Its set's bonuses are not in here: they only apply once enough
	// of the set is worn, and the piece on its own is what this describes.
	ItemDescription::Modifiers modifiers;
	bool statsLoaded;

	SetItemRecord() : setIndex(-1), requiredLevel(0), statsLoaded(false) {};
};

// A set's own bonuses. Which pieces belong to it is held on the pieces.
struct SetRecord {
	std::string name;			// "Tal Rasha's Wrappings"

	// Unlocked as pieces are worn, and what wearing all of them gives.
	std::vector<PropertyStats::Property> partial;
	std::vector<PropertyStats::Property> full;

	std::vector<std::string> partialStats;	// each already carrying its count
	std::vector<std::string> fullStats;
	bool statsLoaded;

	SetRecord() : statsLoaded(false) {};
};

// The set items panel, laid out and driven the same way as the runewords and
// uniques panels.
class SetTab : public UIPanel {
	private:
		Drawing::Listhook* list;

		// Sits beside the window rather than inside the tab, which is why it is a
		// bare Tooltiphook rather than one of the tab's hooks.
		Drawing::Tooltiphook* summary;

		std::vector<SetRecord> sets;
		std::vector<SetItemRecord> items;
		std::vector<const SetItemRecord*> matches;

		// The piece each list row stands for, NULL for the set headings. The list
		// interleaves headings with pieces, so a row number is not an index into
		// matches.
		std::vector<const SetItemRecord*> rowItems;
		unsigned int shownSets;		// headings pushed, for the status line
		bool foldOnPush;			// fold the sets on the next rows pushed

		std::string query;			// active filter, always lowercase
		int shownSummary;			// row the summary was built for, or -1
		bool setsLoaded;
		bool needsRefresh;

		// Tab size the contents were last fitted to, so a resize is noticed.
		unsigned int laidOutWidth;
		unsigned int laidOutHeight;

		// The only place anything is sized or positioned.
		void ApplyLayout();

		void BuildSets();
		void BuildItems();
		void LoadItemStats(SetItemRecord* item);
		void LoadSetStats(SetRecord* set);
		void ApplyFilter();
		void PushRows();

		std::vector<Drawing::TooltipLine> BuildSummaryLines(SetItemRecord* item);
		void UpdateSummary();

	public:
		SetTab(Drawing::UI* ui);

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
		unsigned int GetSetCount() { return sets.size(); };
};
