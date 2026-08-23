#pragma once
#include <string>
#include <vector>
#include "../../ItemDescription.h"
#include "InfoTab.h"

// A property entry as it appears in the game's tables. itemCount is how many
// pieces have to be worn for it to apply, and 0 for a bonus that is always on.
//
// Defaults are declared inline rather than in a constructor: windows.h leaves min
// and max defined as macros, and an initializer list naming them does not compile.
struct SetProperty {
	std::string code;
	std::string param;
	int min = 0;
	int max = 0;
	int itemCount = 0;
};

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

	std::vector<SetProperty> own;		// the item's own always-on properties
	std::vector<SetProperty> partial;	// unlocked as pieces are worn

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

	std::vector<SetProperty> partial;	// unlocked as pieces are worn
	std::vector<SetProperty> full;		// the complete set bonus

	std::vector<std::string> partialStats;	// each already carrying its count
	std::vector<std::string> fullStats;
	bool statsLoaded;

	SetRecord() : statsLoaded(false) {};
};

// The set items panel, laid out and driven the same way as the runewords and
// uniques panels.
class SetTab : public InfoTab {
	private:
		Drawing::Inputhook* searchBox;
		Drawing::Listhook* list;
		Drawing::Texthook* statusText;

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
		std::string lastBoxText;	// last text seen in the search box
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
		void UpdateStatus();

		std::vector<Drawing::TooltipLine> BuildSummaryLines(SetItemRecord* item);
		void UpdateSummary();

	public:
		SetTab(Drawing::UI* ui);

		void MpqLoaded();
		std::vector<ChatCommand> GetCommands();
		void OnDraw();
		bool OnKey(bool up, BYTE key);
		void OnOpen();
		void OnClose();
		void Search(const std::string& text);

		unsigned int GetItemCount() { return items.size(); };
		unsigned int GetSetCount() { return sets.size(); };
};
