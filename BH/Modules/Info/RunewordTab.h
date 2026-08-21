#pragma once
#include <map>
#include <string>
#include <vector>
#include "InfoTab.h"

// A property entry as it appears in the game's tables: what it grants, its
// parameter and the range it rolls.
struct RunewordProperty {
	std::string code;
	std::string param;
	int min;
	int max;
};

// A single runeword recipe, pre-formatted for display.
struct RunewordRecipe {
	std::string name;		// "Enigma"
	std::string runes;		// "Jah + Ith + Ber"
	std::string itemTypes;	// "Any Armor"
	int requiredLevel;		// highest level requirement of its runes, 0 if unknown
	std::string searchKey;	// lowercased name/runes/types, used for filtering

	std::vector<RunewordProperty> properties;	// the runeword's own bonuses
	std::vector<std::string> extraLines;		// bonuses given as ready made text
	std::vector<std::string> runeCodes;			// "r31", in socket order
	std::vector<std::string> baseSlots;			// "weapon" / "helm" / "shield"
	std::vector<std::string> baseLabels;		// readable name per base slot

	std::vector<std::string> stats;				// built on first view
	bool statsLoaded;
};

class RunewordTab : public InfoTab {
	private:
		Drawing::Inputhook* searchBox;
		Drawing::Listhook* list;
		Drawing::Texthook* statusText;

		// The summary of whichever recipe is being pointed at, drawn alongside
		// the window rather than over the list. It is a plain tooltip that knows
		// nothing about runewords; BuildSummaryLines() is what makes it one.
		Drawing::Tooltiphook* summary;

		std::vector<RunewordRecipe> recipes;
		std::vector<const RunewordRecipe*> matches;
		std::string query;			// active filter, always lowercase
		std::string lastBoxText;	// last text seen in the search box
		std::map<std::string, int> runeLevels;	// rune code -> level requirement
		int shownSummary;			// row the summary was built for, or -1
		bool recipesLoaded;
		bool needsRefresh;

		// Tab size the contents were last fitted to, so a resize is noticed.
		unsigned int laidOutWidth;
		unsigned int laidOutHeight;

		// Fits the contents to the tab's current size. Everything that depends
		// on how big the panel is lives here and nowhere else.
		void ApplyLayout();

		void LoadRuneLevels();
		void BuildRecipes();
		void LoadStats(RunewordRecipe* recipe);
		void ApplyFilter();
		void PushRows();
		void UpdateStatus();

		// Turns a recipe into the lines that describe it. The only part of the
		// summary that knows what a runeword is, so another kind of thing can be
		// described in the same panel by writing its own version of this.
		void BuildSummaryLines(RunewordRecipe* recipe,
			std::vector<Drawing::TooltipLine>& lines);
		void UpdateSummary();

	public:
		RunewordTab(Drawing::UI* ui);

		void MpqLoaded();
		bool HandlesCommand(const std::string& command);
		void OnDraw();
		bool OnKey(bool up, BYTE key);
		void OnOpen();
		void OnClose();
		void Search(const std::string& text);

		unsigned int GetRecipeCount() { return recipes.size(); };
};
