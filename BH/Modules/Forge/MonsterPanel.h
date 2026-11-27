#pragma once
#include <string>
#include <utility>
#include <vector>
#include "MarkerPanel.h"
#include "../Maphack/MonsterMarks.h"

namespace Drawing {
	class Checkhook;
	class Combohook;
	class Inputhook;
	class Listhook;
	class Texthook;
}

// The Forge's Monsters tab: the marker each rank of monster starts from, which
// are the registry's and are listed and edited as any tab's markers are, and
// under them the rules, which this panel lists, adds, orders and edits.
//
// A rule is edited as what it matches, then what it changes: a name, the
// monsters it is for, their rank, the difficulty, a modifier and a level, over
// the same editor every marker has, here able to leave the icon or the line as
// it finds it.
//
// The controls edit a copy of the rule in hand, which is put back into the
// list whenever it differs. Nothing holds a pointer into the list itself, so the
// list can be reordered, reverted or reloaded under the panel.
class MonsterPanel : public MarkerPanel {
	private:
		MonsterMarks::RuleList* rules;
		std::vector<MonsterMarks::Rule> model;	// the rules as last taken or given
		unsigned int seenRevision;
		unsigned int modelStamp;				// moves on whenever the model changes
		bool built;

		// The rule in hand, as the controls hold it. Its style keeps what was
		// last chosen for a part that is switched to unchanged, which the list's
		// own copy does not.
		int editingIndex;						// into the model, -1 for none
		MonsterMarks::Rule editing;
		bool rank[MonsterMarks::RankCount];
		bool difficulty[MonsterMarks::DifficultyCount];
		unsigned int modifierIndex;
		std::vector<std::pair<unsigned int, unsigned int>> modifierValues;	// kind and id, by option
		MapIconStyle editingBase;				// what the rule is shown laid over

		// What each rule's row says, worked out when the model or the names
		// it is said with change.
		std::vector<std::string> rowLabels;
		std::vector<std::string> rowHelps;
		unsigned int labelsStamp;
		size_t labelsNames;

		Drawing::Texthook* nameLabel;
		Drawing::Inputhook* nameBox;
		std::string shownPlaceholder;
		Drawing::Texthook* monstersLabel;
		Drawing::Inputhook* monstersBox;
		Drawing::Texthook* rankLabel;
		Drawing::Checkhook* rankChecks[MonsterMarks::RankCount];
		Drawing::Texthook* difficultyLabel;
		Drawing::Checkhook* difficultyChecks[MonsterMarks::DifficultyCount];
		Drawing::Texthook* modifierLabel;
		Drawing::Combohook* modifierBox;
		Drawing::Texthook* levelLabel;
		Drawing::Inputhook* minBox;
		Drawing::Texthook* toLabel;
		Drawing::Inputhook* maxBox;
		Drawing::MapIconhook* editor;
		Drawing::Texthook* addAction;
		Drawing::Texthook* upAction;
		Drawing::Texthook* downAction;
		Drawing::Texthook* removeAction;

		// The monsters to pick from, hanging under their box while it is typed in.
		Drawing::Listhook* picker;
		bool pickerOpen;
		std::string pickerText;				// what the list was last filled for
		std::vector<int> pickerEntries;		// the entry each of its rows stands for, -1 for none
		std::string summary;				// what the box says while the list is shut
		std::vector<unsigned int> summaryMonsters, summaryUniques;
		bool summaryHerd;
		size_t summaryNames;
		unsigned int summaryLimit;			// the characters the box had room for

		// Asked for by a click, which is not on the thread the list is changed
		// on, and done as the panel next draws.
		bool wantAdd, wantRemove;
		int wantMove;
		bool removeAsked;		// asked once, and done when asked again

		unsigned int rankWidth, difficultyWidth, modifierWidth, levelWidth, conditionsWidth;

		void BuildControls();
		void TakeRules();
		void Commit();
		void LoadEditing(int index);
		void CollectEditing();
		void DoRequests();
		void RefreshLabels();
		void UpdateActions();
		void UpdateSummary(bool force);

		bool Chosen(unsigned int entry);
		void OpenPicker();
		void FillPicker();
		void TogglePick(int row);
		void ClosePicker();
		bool PickerHasMouse();
		void UpdatePicker();

		MapIconStyle ShownStyle(const MonsterMarks::Rule& rule);

		static bool OnAddClicked(bool up, Drawing::Hook* hook, void* self);
		static bool OnUpClicked(bool up, Drawing::Hook* hook, void* self);
		static bool OnDownClicked(bool up, Drawing::Hook* hook, void* self);
		static bool OnRemoveClicked(bool up, Drawing::Hook* hook, void* self);

	protected:
		void Sync();
		std::string MarkersCaption();
		void AppendRows(std::vector<MarkerListRow>& rows);
		void MeasureExtra(unsigned int& listWidth, unsigned int& labelWidth,
			unsigned int& detailMinWidth, unsigned int& detailHeight,
			unsigned int& listMinHeight);
		unsigned int ListFooterHeight();
		void LayoutExtra(int selectedId, unsigned int listX, unsigned int listWidth,
			unsigned int footerY, unsigned int detailX, unsigned int detailY,
			unsigned int detailWidth, unsigned int labelWidth);
		bool OverlayOpen() { return pickerOpen; };

	public:
		// The tab is called name, shows the markers registered under category,
		// and edits the rules given.
		MonsterPanel(Drawing::UI* ui, std::string name, std::string category,
			MonsterMarks::RuleList* rules);

		void OnClose();
};
