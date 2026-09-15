#pragma once
#include <string>
#include <vector>
#include "../../Catalogue/StatIndex.h"
#include "../../Drawing.h"
#include "StatVocabulary.h"

/*
 * The rows of stat conditions a codex panel is narrowed by, and the controls
 * they are written in.
 *
 * One builder serves every panel that offers conditions, the way one search box
 * serves every panel of a window: the question a player is asking - what grants
 * thirty faster cast rate - is the same question on each of them, and having to
 * write it again on each would be the window asking them to repeat themselves.
 * Which panel is in front decides only what the picker may offer, since a stat
 * is only worth offering where something grants it.
 *
 * The builder answers in criteria and knows nothing about what answers them.
 * It is the window that reserves the room it draws in, and the panels that ask
 * it what they should be showing.
 */
class QueryBuilder {
	public:
		// Most rows there can ever be. The room the window can spare caps it
		// lower; this is what the controls are built for, so that adding a row
		// is switching one on rather than building one over the top of whatever
		// is drawn after it.
		static const unsigned int MaxRows = 8;

		// Built into the window's chrome, so the rows are laid out against the
		// window's content box and are drawn, hidden and destroyed with it.
		QueryBuilder(Drawing::UI* ui);

		// Whether the rows are on screen. The conditions apply either way: a
		// filter that stopped when its rows were put away would be a list that
		// changed length for no reason the player could see.
		bool IsShown() { return shown; };
		void SetShown(bool show);

		// Whether the panel in front offers conditions at all. A panel that does
		// not is not a reason to put the rows away - the one behind it still
		// wants them - but it is a reason not to draw them over its list.
		void SetOffered(bool offers);

		// The kind of source the picker offers stats for, which is whichever
		// panel is in front. Changing it re-reads the rows' labels, since the
		// same stat is worded by the same tables whatever grants it but a stat
		// the new kind never grants has no entry to be worded from.
		void SetKind(const std::string& kind);

		// How much room the rows may take. The window is never resized to fit
		// them, so this is what stops the list being squeezed out of existence:
		// rows past what the budget holds cannot be added.
		void SetHeightBudget(unsigned int pixels);

		// What the rows come to, which is what the window reserves.
		unsigned int GetHeight();

		// The conditions as the index asks for them. A row counts once it names a
		// stat: with an amount beside it the condition is that comparison, and
		// with the box left empty it is the stat at whatever it rolls. A row
		// naming no stat asks nothing, so a row being filled in does not empty
		// the list on the way.
		std::vector<StatIndex::Criterion> GetCriteria();
		unsigned int GetActiveCount() { return activeCount; };

		// What a panel calls the conditions when it says what it is showing -
		// "2 conditions" - and nothing at all where there are none, so a footer
		// composed from it reads the same as it always did.
		std::string DescribeConditions();

		// Bumped whenever the conditions could have changed, so a panel can tell
		// that it has to ask again without comparing criteria itself.
		unsigned int GetRevision() { return revision; };

		// Back to one empty row, with the rows put away. What the window does
		// when it closes, the conditions being the window's rather than any one
		// panel's.
		void Clear();

		// Shuts the stat picker if it is open, and says whether it was. Escape
		// closes what is open before it closes the window.
		bool ClosePicker();

		// Places the rows and keeps them in step with what has been typed into
		// them. Called once a frame, from the window's draw.
		void OnDraw();

	private:
		// One condition: the stat it names, how it compares, and against what.
		// The controls are built once and switched on as rows are added, so that
		// no control is ever created after the picker that has to draw over it.
		struct Row {
			Drawing::Inputhook* stat;
			Drawing::Combohook* comparator;
			Drawing::Inputhook* value;
			Drawing::Buttonhook* remove;

			// Behind the combo box, which holds the caller's index rather than
			// one of its own.
			unsigned int comparatorIndex;

			// The stat the row names, as ItemStatCost names it, and empty until
			// one has been picked. What is typed in the box is a search for one
			// of these and not the thing itself.
			std::string stat_;

			Row() : stat(NULL), comparator(NULL), value(NULL), remove(NULL),
				comparatorIndex(0) {};
		};

		Drawing::UI* ui;
		std::vector<Row*> rows;
		Drawing::Texthook* addLine;
		Drawing::Listhook* picker;

		std::string kind;
		bool shown;
		bool panelOffers;
		unsigned int rowCount;		// rows switched on
		unsigned int maxRows;		// what the budget allows
		unsigned int activeCount;	// rows contributing a criterion
		unsigned int revision;

		// The row whose picker is open, or MaxRows for none.
		unsigned int pickerRow;

		// What the picker is currently offering, parallel to its rows, and the
		// text it was filled from. Refilled only when that text changes: a list
		// rebuilt every frame is one whose scroll position is put back to the
		// top every frame, which is a list that cannot be scrolled.
		std::vector<StatVocabulary::Entry> offered;
		std::string pickerText;

		// What the conditions came to when the revision was last bumped, so a
		// change of any kind is noticed without every control having to report
		// one.
		std::string signature;

		// Geometry the rows are laid out to, so a resize is noticed.
		unsigned int laidOutWidth;
		unsigned int laidOutY;

		unsigned int RowHeight();
		unsigned int RowPitch();
		unsigned int AddLineHeight();

		void ApplyLayout();
		void ShowControls();
		void AddRow();
		void ClearRow(unsigned int index);

		// Takes the row away, or empties it where it is the only one there is.
		void RemoveRow(unsigned int index);

		// Opens the picker under one row's stat box, and fills it from what is
		// typed there. Closing it leaves the box reading whatever stat the row
		// still names.
		void OpenPicker(unsigned int index);
		void FillPicker();
		void CommitPick(unsigned int offeredRow);

		// Whether the picker is in the middle of being used, in which case the
		// stat box losing the caret is the picker being clicked rather than the
		// player going elsewhere.
		bool PickerHasMouse();

		// Puts a row's stat box back to the stat the row names, for a box left
		// with a half typed search in it.
		void RestoreStatText(unsigned int index);

		std::string Signature();

		// Fired by the row buttons and the add line, which carry the builder as
		// their context.
		static bool RemoveClicked(bool up, Drawing::Hook* hook, void* context);
		static bool AddClicked(bool up, Drawing::Hook* hook, void* context);

		// The row a remove button belongs to, since the callbacks are handed the
		// hook rather than the row.
		unsigned int RowOf(Drawing::Hook* hook);
};
