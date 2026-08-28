#pragma once
#include <map>
#include <set>
#include <string>
#include <vector>
#include "../Window/UIPanel.h"
#include "SettingsRegistry.h"

class SettingsPanel;

// The bar across a heading, and the whole of what a click on the heading lands
// on. Invisible - the heading's own text is all that is drawn - but the full
// width of the panel, so the target is one predictable line rather than however
// wide the label happened to be.
class SettingsHeadingBar : public Drawing::Boxhook {
	private:
		SettingsPanel* panel;
		unsigned int row;
	public:
		SettingsHeadingBar(Drawing::HookGroup* group, SettingsPanel* panel,
			unsigned int row);

		void OnDraw() {};
		bool OnLeftClick(bool up, unsigned int x, unsigned int y);
};

// One tab of the settings window, laid out from the registry.
//
// The only place a setting is positioned or sized. A module says what its
// settings are; how they are drawn is decided here, once, for all of them - one
// column, label and control on a line, the control against the right edge so it
// tracks the window, a setting that depends on another indented under it and
// greyed out while it is off, help on hover, and what happens when the window is
// resized or the settings are filtered.
//
// Rows are built once and laid out again as needed. Filtering and resizing both
// only lay out again, so neither destroys a control the user might be part way
// through using.
//
// A heading folds the settings under it away, and starts folded: a heading is
// therefore a decision to hide things, and a setting registered without one is
// always on screen.
//
// The panel holds a row for every setting in the window rather than only for its
// own category, and shows its own until a search widens it to all of them. That
// is what makes a search cross the tabs: somebody searching does not know which
// tab the setting is on - that being the reason to search rather than browse -
// and a result therefore has to be able to come from anywhere and to say where it
// came from.
class SettingsPanel : public UIPanel {
	private:
		// One row, as built. Which hooks exist depends on the kind: a checkbox
		// draws its own label, so those rows have no separate label, and a note is
		// nothing but lines of text.
		struct Row {
			// What the row stands for, or NULL for the heading a category's
			// unheaded settings are gathered under, which nothing registered.
			const Settings::Descriptor* setting;
			Drawing::Texthook* label;	// NULL where the control labels itself
			Drawing::Hook* control;		// what you change, or NULL
			Drawing::Keyhook* hotkey;	// the binding beside it, or NULL

			// A heading's own three parts: what takes the click, the fold marker,
			// and how many settings are hidden. All NULL on every other kind.
			SettingsHeadingBar* bar;
			Drawing::Texthook* marker;
			Drawing::Texthook* count;

			// A note is wrapped to the width of the window, and a Texthook is one
			// line, so a note is drawn as several of them.
			std::vector<Drawing::Texthook*> noteLines;

			// The tab the row belongs to. Held on the row rather than read off the
			// descriptor because the heading over an unheaded run has no descriptor
			// to read it off, and because it is what a search is grouped by.
			std::string category;

			bool heading;				// registered as one, or standing in for one

			unsigned int indent;
			unsigned int height;		// as laid out; a note grows with its text
			bool enabled;				// its parent is on, or it has no parent

			// Lowercased label and config key. Not the category: every row in a tab
			// carries the same one, so searching for it used to match all of them
			// and filter nothing. Where a result lives is drawn above it instead.
			std::string searchKey;

			// The heading this row is under. Positional - the last heading built
			// before it - which is also what makes it independent of the
			// parent/child indent: a child is hidden because its heading is folded,
			// whatever its parent is doing.
			//
			// Never -1 on a row that is not itself a heading: a category that
			// registered settings without one is given a heading to gather them
			// under, so that a search has something to name them by.
			int headingRow;

			Row() : setting(NULL), label(NULL), control(NULL), hotkey(NULL),
				bar(NULL), marker(NULL), count(NULL), heading(false), indent(0),
				height(0), enabled(true), headingRow(-1) {};
		};

		std::string category;

		// Every tab in the window, in the order the window shows them, which is the
		// order a search puts its results in.
		std::vector<std::string> categories;

		Drawing::Scrollhook* box;
		std::vector<Row> rows;
		std::map<std::string, unsigned int> byKey;	// so a row can find its parent
		std::vector<unsigned int> shown;			// indices into rows, filtered

		// Sits outside the panel and over the top of it, which is why it is a bare
		// Tooltiphook rather than one of the panel's hooks: a hook inside the
		// scrolling box would be clipped to it and drawn under its neighbours.
		Drawing::Tooltiphook* helpTip;
		int shownHelp;				// row the tip was built for, or -1

		std::string query;			// active filter, always lowercase
		int focusRow;				// index into rows, or -1

		// Folds, by heading label rather than by row, so they outlive the rows
		// they were made against: any module registering a setting rebuilds every
		// row in the panel, and a fold held on a row would be lost with it.
		//
		// seenHeadings is what makes folded-by-default work without a flag: a
		// heading nobody has seen before is folded as it is built, and one the user
		// has since opened is known and left alone.
		std::set<std::string> folded;
		std::set<std::string> seenHeadings;

		// Row to bring into view after the next layout, or -1. A fold is laid out
		// again from the top, so without this the heading that was clicked would
		// walk off the screen as the rows under it appeared.
		int scrollToRow;

		// Both fold markers as measured at the current font, and the column they
		// share, which is as wide as the wider of them: a label that moved when its
		// marker changed would jump every time it was folded.
		unsigned int markerWidth;

		unsigned int builtVersion;	// registry version the rows were built for
		unsigned int laidOutWidth, laidOutHeight;
		bool needsLayout;

		// Builds a control per setting, once. Measures text, so it belongs on the
		// draw thread.
		void Build();

		// One tab's worth of rows, appended to whatever is already there. The
		// heading a row is under is the last one built before it, so a category is
		// built in one run rather than a row at a time.
		void BuildCategory(Drawing::HookGroup* content, const std::string& category);

		// The row a heading is drawn as. A heading with no descriptor behind it
		// stands for the settings a category registered without one: it names them
		// in a search and is left out of the tab they are already under.
		void AddHeadingRow(Drawing::HookGroup* content, const std::string& category,
			const Settings::Descriptor* setting);

		// What a heading reads as, which is where the rows under it live: the
		// section alone within its own tab, and the tab as well while a search is
		// crossing them.
		std::string HeadingText(const Row& row);

		// Places the rows that pass the filter and hands them to the box. Also
		// switches off the ones that do not, since a hook nothing draws is still a
		// hook that would take a click where it used to be.
		void Relayout();

		void ApplyFilter();

		// Folding. Suspended while a search is running - a query that hid its own
		// matches would be no use - and the folds are kept rather than cleared, so
		// clearing the search restores exactly what the user had open.
		bool FoldingActive() { return query.empty(); };
		bool IsFolded(const Row& row);
		bool IsHeadingFolded(const Row& row);
		unsigned int SettingsUnder(int headingRow);
		void MeasureMarkers();

		// Greys out and makes inert every setting whose parent is off. Done every
		// frame, because a parent can be switched off at any moment - by this
		// panel, by a hotkey, or by a reload.
		void ApplyDependencies();
		void SetRowEnabled(Row& row, bool enabled);
		bool SettingIsOn(const Row& row);

		// A setting is something someone came looking for; a heading is something
		// the keyboard has to be able to reach, since folded by default would
		// otherwise put most of the window out of its reach entirely.
		bool IsSetting(const Row& row);
		bool IsFocusable(const Row& row);

		// Whatever carries the row's name: the label where there is one, the
		// control itself for the kinds that label themselves, and nothing at all
		// for a note.
		Drawing::Hook* NamedHook(const Row& row);

		// Draws every row's name in the colour that says what it is. The focused
		// row is marked by its name rather than by a band behind it, because the
		// panel's background is whatever the window's transparency makes it and
		// anything drawn behind the text is only as visible as that allows.
		//
		// Called every frame, since focus and the dependencies that grey a row out
		// both move without the rows being laid out again.
		void ApplyFocusColors();

		void UpdateHelp();

		// Keeps a box and the value behind it in step. Here rather than in the
		// module that owns the value, so there is one of these rather than one per
		// setting.
		void SyncNumbers();
		void SyncText();

		// Whether one of the panel's own boxes is being typed into, in which case
		// the keys that move the caret are the box's rather than the panel's.
		bool InputFocused();
		void ClearInputFocus();

		// Focus moves by position on screen; what it lands on is a setting.
		int FocusPosition();
		void SetFocusPosition(int position);
		void MoveFocus(int delta);
		void ActuateFocused();

	public:
		SettingsPanel(std::string category, const std::vector<std::string>& categories,
			Drawing::UI* ui);
		~SettingsPanel();

		// The tabs the window ended up with, which a panel cannot know for itself:
		// a category with nothing registered in it is not a tab, and modules go on
		// registering after the first panel is built. A change of them is a
		// rebuild, since the rows are the panel's whole view of the registry.
		void SetCategories(const std::vector<std::string>& all);

		// Called by the bar across a heading, which is the hook the click lands on.
		void ToggleHeading(unsigned int row);

		std::string GetSearchPlaceholder();
		std::string GetStatus();
		void Search(const std::string& text);
		void OnDraw();
		bool OnKey(bool up, BYTE key);
		void OnClose();
};
