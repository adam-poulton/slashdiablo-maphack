#pragma once
#include <string>
#include <vector>
#include "../../ItemDescription.h"
#include "InfoTab.h"

// A property entry as it appears in the game's tables.
struct CubeProperty {
	std::string code;
	std::string param;
	int min;
	int max;
};

// A single Horadric Cube recipe, pre-formatted for display.
struct CubeRecipe {
	std::string group;			// the heading it sits under, "Gem"
	std::string result;			// "Perfect Ruby", as the game names it

	// What the result is drawn in, both in the list and at the top of the
	// summary panel, so a recipe reads as the item it makes wherever it is
	// shown. Gold wherever the game gives the name no colour of its own, which
	// is what a plain item and every quality the game draws plain come to.
	TextColor resultColor;
	std::string ingredients;	// "3 Flawless Ruby"
	std::string searchKey;		// lowercased group/result/ingredients/notes

	// What the recipe does beyond making the result: the sockets it adds, the
	// levels it costs, and the conditions it is only allowed under.
	std::vector<std::string> notes;

	std::vector<CubeProperty> properties;	// the bonuses the result is given
	std::vector<std::string> stats;			// built on first view
	bool statsLoaded;

	CubeRecipe() : resultColor(Gold), statsLoaded(false) {};
};

// The Horadric Cube panel, laid out and driven the same way as the runewords
// panel.
class RecipeTab : public InfoTab {
	private:
		Drawing::Inputhook* searchBox;
		Drawing::Listhook* list;
		Drawing::Texthook* statusText;

		// Sits beside the window rather than inside the tab, which is why it is a
		// bare Tooltiphook rather than one of the tab's hooks.
		Drawing::Tooltiphook* summary;

		std::vector<CubeRecipe> recipes;
		std::vector<const CubeRecipe*> matches;

		// The recipe on each list row, and NULL on the heading rows, so a row
		// number means the same thing however the list is folded.
		std::vector<const CubeRecipe*> rowRecipes;
		unsigned int shownGroups;	// headings pushed, for the status line
		bool foldOnPush;			// fold the groups on the next rows pushed

		std::string query;			// active filter, always lowercase
		std::string lastBoxText;	// last text seen in the search box
		int shownSummary;			// row the summary was built for, or -1
		bool recipesLoaded;
		bool needsRefresh;

		// Tab size the contents were last fitted to, so a resize is noticed.
		unsigned int laidOutWidth;
		unsigned int laidOutHeight;

		// The only place anything is sized or positioned.
		void ApplyLayout();

		void BuildRecipes();
		void LoadStats(CubeRecipe* recipe);
		void ApplyFilter();
		void PushRows();
		void UpdateStatus();

		std::vector<Drawing::TooltipLine> BuildSummaryLines(CubeRecipe* recipe);
		void UpdateSummary();

	public:
		RecipeTab(Drawing::UI* ui);

		void MpqLoaded();
		bool HandlesCommand(const std::string& command);
		void OnDraw();
		bool OnKey(bool up, BYTE key);
		void OnOpen();
		void OnClose();
		void Search(const std::string& text);

		unsigned int GetRecipeCount() { return recipes.size(); };
};
