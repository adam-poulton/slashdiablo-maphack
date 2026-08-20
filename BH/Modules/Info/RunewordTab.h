#pragma once
#include <string>
#include <vector>
#include "InfoTab.h"

// A single runeword recipe, pre-formatted for display.
struct RunewordRecipe {
	std::string name;		// "Enigma"
	std::string runes;		// "Jah + Ith + Ber"
	std::string itemTypes;	// "Any Armor"
	unsigned int sockets;	// number of runes in the word
	std::string searchKey;	// lowercased name/runes/types, used for filtering
};

class RunewordTab : public InfoTab {
	private:
		Drawing::Inputhook* searchBox;
		Drawing::Listhook* list;
		Drawing::Texthook* statusText;

		std::vector<RunewordRecipe> recipes;
		std::vector<const RunewordRecipe*> matches;
		std::string query;			// active filter, always lowercase
		std::string lastBoxText;	// last text seen in the search box
		bool recipesLoaded;
		bool needsRefresh;

		void BuildRecipes();
		void ApplyFilter();
		void PushRows();

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
