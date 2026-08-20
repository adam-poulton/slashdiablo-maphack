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
	unsigned int sockets;	// number of runes, so the sockets the base needs
	int requiredLevel;		// highest level requirement of its runes, 0 if unknown
	std::string searchKey;	// lowercased name/runes/types, used for filtering

	std::vector<RunewordProperty> properties;	// the runeword's own bonuses
	std::vector<std::string> runeCodes;			// "r31", in socket order
	std::vector<std::string> baseSlots;			// "weapon" / "helm" / "shield"
	std::vector<std::string> baseLabels;		// readable name per base slot

	std::vector<std::string> stats;				// built on first view
	bool statsLoaded;
};

// Detail view capacity, split over two columns.
#define RW_DETAIL_LINES		48

class RunewordTab : public InfoTab {
	private:
		// List view.
		Drawing::Inputhook* searchBox;
		Drawing::Listhook* list;
		Drawing::Texthook* statusText;
		Drawing::Texthook* listHint;
		Drawing::Texthook* prevLink;
		Drawing::Texthook* nextLink;

		// Detail view, shown in place of the list.
		Drawing::Texthook* detailTitle;
		Drawing::Texthook* detailRunes;
		Drawing::Texthook* detailLevel;
		Drawing::Texthook* detailTypes;
		Drawing::Texthook* detailLines[RW_DETAIL_LINES];
		Drawing::Texthook* backLink;

		std::vector<RunewordRecipe> recipes;
		std::vector<const RunewordRecipe*> matches;
		std::string query;			// active filter, always lowercase
		std::string lastBoxText;	// last text seen in the search box
		std::map<std::string, int> runeLevels;	// rune code -> level requirement
		int shownDetail;			// index into matches, or -1 for the list view
		bool recipesLoaded;
		bool needsRefresh;

		void LoadRuneLevels();
		void BuildRecipes();
		void LoadStats(RunewordRecipe* recipe);
		void ApplyFilter();
		void PushRows();
		void ShowDetail(int match);
		void ShowList();
		void ApplyViewVisibility();

	public:
		RunewordTab(Drawing::UI* ui);

		void MpqLoaded();
		void OnDraw();
		bool OnKey(bool up, BYTE key);

		// Filter the list from outside the window, for the chat command.
		void Search(const std::string& text);

		unsigned int GetRecipeCount() { return recipes.size(); };

		static bool __cdecl OnPrevClick(bool up, Drawing::Hook* hook, void* data);
		static bool __cdecl OnNextClick(bool up, Drawing::Hook* hook, void* data);
		static bool __cdecl OnBackClick(bool up, Drawing::Hook* hook, void* data);
};
