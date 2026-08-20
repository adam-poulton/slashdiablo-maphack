#pragma once
#include <map>
#include <string>
#include <vector>
#include "../Module.h"
#include "../../Config.h"
#include "../../Drawing.h"

// A single runeword recipe, pre-formatted for display.
struct RunewordRecipe {
	std::string name;		// "Enigma"
	std::string runes;		// "Jah + Ith + Ber"
	std::string itemTypes;	// "Any Armor"
	unsigned int sockets;	// number of runes in the word
	std::string searchKey;	// lowercased name/runes/types, used for filtering
};

class Runewords : public Module {
	private:
		CRITICAL_SECTION crit;

		// Our own draggable lookup window.
		Drawing::UI* lookupUI;
		Drawing::UITab* lookupTab;
		Drawing::Inputhook* searchBox;
		Drawing::Texthook* statusText;
		Drawing::Texthook* prevText;
		Drawing::Texthook* nextText;
		std::vector<Drawing::Texthook*> nameCells;
		std::vector<Drawing::Texthook*> runeCells;
		std::vector<Drawing::Texthook*> typeCells;

		std::vector<RunewordRecipe> recipes;
		std::vector<const RunewordRecipe*> matches;
		std::string query;			// active filter, always lowercase
		std::string lastBoxText;	// last text seen in the search box
		unsigned int page;
		bool recipesLoaded;
		bool needsRefresh;

		std::map<std::string, Toggle> Toggles;

		void BuildRecipes();
		void ApplyFilter();
		void UpdateCells();
		void ChangePage(int delta);
		unsigned int PageCount();

	public:
		Runewords() : Module("Runewords"),
			lookupUI(NULL),
			lookupTab(NULL),
			searchBox(NULL),
			statusText(NULL),
			prevText(NULL),
			nextText(NULL),
			page(0),
			recipesLoaded(false),
			needsRefresh(true) {
			InitializeCriticalSection(&crit);
		};

		~Runewords() {
			DeleteCriticalSection(&crit);
		};

		void Lock() { EnterCriticalSection(&crit); };
		void Unlock() { LeaveCriticalSection(&crit); };

		void OnLoad();
		void LoadConfig();
		void MpqLoaded();

		void OnLoop();
		void OnGameExit();
		void OnDraw();
		void OnKey(bool up, BYTE key, LPARAM lParam, bool* block);
		void OnUserInput(const wchar_t* msg, bool fromGame, bool* block);

		// Filter the recipe list; used by the window and by the chat command.
		void Search(const std::string& text);

		// Open (or collapse) the lookup window.
		void ShowWindow(bool show);

		static bool __cdecl OnPrevClick(bool up, Drawing::Hook* hook, void* data);
		static bool __cdecl OnNextClick(bool up, Drawing::Hook* hook, void* data);
};
