#pragma once
#include <map>
#include <string>
#include <vector>
#include "InfoTab.h"

// A single runeword recipe, pre-formatted for display.
struct RunewordRecipe {
	std::string name;		// "Enigma"
	std::string runes;		// "Jah + Ith + Ber"
	std::string itemTypes;	// "Any Armor"
	unsigned int sockets;	// number of runes in the word
	int requiredLevel;		// highest level requirement of its runes, 0 if unknown
	std::string searchKey;	// lowercased name/runes/types, used for filtering
};

class RunewordTab : public InfoTab {
	private:
		Drawing::Inputhook* searchBox;
		Drawing::Listhook* list;
		Drawing::Texthook* statusText;
		Drawing::Texthook* detailLines[4];

		std::vector<RunewordRecipe> recipes;
		std::vector<const RunewordRecipe*> matches;
		std::string query;			// active filter, always lowercase
		std::string lastBoxText;	// last text seen in the search box
		std::map<std::string, int> runeLevels;	// rune code -> level requirement
		int shownDetail;			// selected row the detail pane is describing
		bool recipesLoaded;
		bool needsRefresh;

		void LoadRuneLevels();
		void BuildRecipes();
		void ApplyFilter();
		void PushRows();
		void PushDetail();

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
};
