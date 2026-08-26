#pragma once
#include <string>
#include <vector>
#include "../../ItemDescription.h"
#include "../Window/UIPanel.h"

// A property entry as it appears in the game's tables.
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
	// Highest level requirement among its runes. A runeword goes in any of a
	// range of bases, so it carries no strength or dexterity of its own.
	ItemDescription::Requirements requirements;
	std::string searchKey;	// lowercased name/runes/types, used for filtering

	std::vector<RunewordProperty> properties;	// the runeword's own bonuses
	std::vector<std::string> extraLines;		// bonuses given as ready made text
	std::vector<std::string> runeCodes;			// "r31", in socket order
	std::vector<std::string> baseSlots;			// "weapon" / "helm" / "shield"
	std::vector<std::string> baseLabels;		// readable name per base slot

	std::vector<std::string> stats;				// built on first view
	bool statsLoaded;
};

class RunewordTab : public UIPanel {
	private:
		Drawing::Listhook* list;

		// Sits beside the window rather than inside the tab, which is why it is a
		// bare Tooltiphook rather than one of the tab's hooks.
		Drawing::Tooltiphook* summary;

		std::vector<RunewordRecipe> recipes;
		std::vector<const RunewordRecipe*> matches;
		std::string query;			// active filter, always lowercase
		int shownSummary;			// row the summary was built for, or -1
		bool recipesLoaded;
		bool needsRefresh;

		// Tab size the contents were last fitted to, so a resize is noticed.
		unsigned int laidOutWidth;
		unsigned int laidOutHeight;

		// The only place anything is sized or positioned.
		void ApplyLayout();

		void BuildRecipes();
		void LoadStats(RunewordRecipe* recipe);
		void ApplyFilter();
		void PushRows();

		std::vector<Drawing::TooltipLine> BuildSummaryLines(RunewordRecipe* recipe);
		void UpdateSummary();

	public:
		RunewordTab(Drawing::UI* ui);

		void MpqLoaded();
		std::vector<ChatCommand> GetCommands();
		void OnDraw();
		bool OnKey(bool up, BYTE key);
		void OnClose();
		void Search(const std::string& text);
		void OnSearchSubmitted();
		std::string GetSearchPlaceholder();
		std::string GetStatus();

		unsigned int GetRecipeCount() { return recipes.size(); };
};
