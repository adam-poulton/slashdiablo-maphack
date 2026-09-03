#include "SettingsPanel.h"
#include <algorithm>
#include <stdlib.h>
#include <utility>
#include "../../Common.h"
#include "../../D2Ptrs.h"

using namespace Drawing;

// One line per setting. The controls are shorter than this and are centred in it,
// rather than each being nudged into place by hand the way the old tabs did it.
#define SETTINGS_ROW_HEIGHT		15

// A heading gets a little more room, most of it above, so it reads as the start
// of something rather than as another row.
#define SETTINGS_HEADING_HEIGHT	20
#define SETTINGS_HEADING_TOP	8
#define SETTINGS_HEADING_COLOR	Gold
#define SETTINGS_HEADING_HOVER	Tan

// Between the tab and the section on a heading, while a search is crossing the
// tabs. A result says where it lives by what is written above it, which is also
// where the room for it is: the right of a row belongs to the control.
#define SETTINGS_HEADING_SEPARATOR	" / "

// What a heading is folded and unfolded with, and how many settings it is hiding.
// The markers share a column as wide as the wider of them, so the label does not
// move as it is folded. The count is only drawn where there is something to
// count: a heading over nothing but notes has no number worth showing.
#define SETTINGS_FOLDED			"+"
#define SETTINGS_UNFOLDED		"-"
#define SETTINGS_MARKER_GAP		3
#define SETTINGS_COUNT_GAP		4
#define SETTINGS_COUNT_COLOR	Grey

// How far a setting that depends on another sits in from it.
#define SETTINGS_INDENT			12

// A label beside a control is drawn as a checkbox draws its own: the reading
// order down the panel is the labels, and half of them being a different colour
// from the other half made two lists out of one.
#define SETTINGS_LABEL_COLOR	Gold
#define SETTINGS_LABEL_HOVER	Tan

// The row the keyboard is on, which is its name drawn brighter than the rest and
// left that way until focus moves. A colour of its own rather than the hover
// colour: with one colour for both, a hovered row and the focused row read as the
// same thing, and hovering one row while another is focused reads as two focused
// rows. Where the mouse is resting on the focused row the focus colour is what is
// drawn, since the mouse is about to move on and the keyboard is not.
#define SETTINGS_FOCUS_COLOR	White

// Focus on a setting that its parent has switched off. Still grey, because it
// still does nothing, but lighter than the rest: focus that disappeared as it
// crossed a greyed out row would read as the key having failed.
#define SETTINGS_FOCUS_DISABLED	Silver

// Controls whose width is not decided by their text.
#define SETTINGS_ENUM_WIDTH		120
#define SETTINGS_NUMBER_WIDTH	44
#define SETTINGS_TEXT_WIDTH		120

// A note is something to read rather than something to change, so it is drawn in
// the colour the old tabs used for the same kind of text. The cap on lines bounds
// how many Texthooks a note is built from; a note longer than that is cut rather
// than allowed to grow without limit.
#define SETTINGS_NOTE_COLOR		Gold
#define SETTINGS_NOTE_LINE		11
#define SETTINGS_NOTE_MAX_LINES	8

// The sections of a tab, in the order they are shown. A section is a heading and
// the settings registered under it, so this says for the inside of a tab what the
// window's category list says for the tab strip, and for the same reason:
// registration order is module load order, so left to itself a tab reads in the
// order its modules happen to be created in rather than one anybody chose.
//
// Settings a module registers without a heading are the tab's own, and come
// before every section of it whichever module they came from. A heading not named
// here follows the ones that are, in the order it was registered in.
static const struct {
	const char* category;
	const char* heading;
} SectionOrder[] = {
	{ Settings::Category::Map, "Path colors" },
	{ Settings::Category::Map, "Monster colors" },
	{ Settings::Category::Map, "Missile colors" },

};

static unsigned int SectionOrderCount() {
	return sizeof(SectionOrder) / sizeof(SectionOrder[0]);
}

static int SectionRank(const std::string& category, const std::string& heading) {
	for (unsigned int i = 0; i < SectionOrderCount(); i++) {
		if (category.compare(SectionOrder[i].category) == 0 &&
				heading.compare(SectionOrder[i].heading) == 0)
			return (int)i;
	}
	return (int)SectionOrderCount();
}

// The tab's settings, cut into sections at its headings and put in the order
// above. Stable, so anything the table does not name keeps the order it was
// registered in, and whole sections move rather than single settings: a colour
// belongs under the heading it was registered under wherever that heading lands.
static std::vector<const Settings::Descriptor*> InSectionOrder(
		const std::string& category) {
	std::vector<const Settings::Descriptor*> settings =
		Settings::InCategory(category);

	// A section runs from a heading to the next one. A module that registers into
	// a tab another module has already put a heading in starts a section of its own
	// rather than landing under that heading, which is why the owner changing also
	// breaks the run: a module's settings say nothing about a heading it never saw.
	typedef std::pair<int, std::vector<const Settings::Descriptor*> > Section;
	std::vector<Section> sections;
	std::string owner;
	for (unsigned int i = 0; i < settings.size(); i++) {
		bool heading = (settings[i]->kind == Settings::KindHeading);
		if (sections.empty() || heading || settings[i]->owner.compare(owner) != 0) {
			// Below every rank a heading can have, so an unheaded run stays above the
			// sections however those are ordered.
			int rank = heading ? SectionRank(category, settings[i]->label) : -1;
			sections.push_back(Section(rank,
				std::vector<const Settings::Descriptor*>()));
		}
		owner = settings[i]->owner;
		sections.back().second.push_back(settings[i]);
	}

	std::stable_sort(sections.begin(), sections.end(),
		[](const Section& a, const Section& b) -> bool {
			return a.first < b.first;
		});

	std::vector<const Settings::Descriptor*> ordered;
	for (unsigned int s = 0; s < sections.size(); s++) {
		ordered.insert(ordered.end(), sections[s].second.begin(),
			sections[s].second.end());
	}
	return ordered;
}

SettingsHeadingBar::SettingsHeadingBar(HookGroup* group, SettingsPanel* panel,
		unsigned int row) :
	Boxhook(group, 0, 0, 0, 0), panel(panel), row(row) {
}

// On the release, as every other control in the panel acts. Folding relays the
// rows below the heading out, but that no longer costs anything: the gesture is
// paired to the hook that took the press, so nothing that moves under the cursor
// can take the release.
bool SettingsHeadingBar::OnLeftClick(bool up, unsigned int x, unsigned int y) {
	if (!InRange(x, y))
		return false;
	if (up)
		panel->ToggleHeading(row);
	return true;
}

// Breaks text to a width, on spaces where it can. A Texthook is one line and
// knows nothing about wrapping, so a note is drawn as several of them.
static std::vector<std::string> WrapText(const std::string& text,
		unsigned int width, unsigned int font) {
	std::vector<std::string> lines;
	if (width == 0 || text.empty()) {
		lines.push_back(text);
		return lines;
	}

	std::string line;
	std::string::size_type start = 0;
	while (start <= text.length()) {
		std::string::size_type space = text.find(' ', start);
		std::string word = (space == std::string::npos) ?
			text.substr(start) : text.substr(start, space - start);

		std::string candidate = line.empty() ? word : (line + " " + word);
		if (!line.empty() &&
				(unsigned int)Texthook::GetTextSize(candidate, font).x > width) {
			lines.push_back(line);
			line = word;
		} else {
			line = candidate;
		}

		if (space == std::string::npos)
			break;
		start = space + 1;
	}
	if (!line.empty())
		lines.push_back(line);
	if (lines.empty())
		lines.push_back("");
	return lines;
}

// Wide enough for the largest value the setting will take, so a ceiling in the
// thousands is not typed into a box that shows four digits. Measured the way the
// box measures itself, and never narrower than the minimum, since a box for a two
// digit number still has to look like a box.
static unsigned int NumberBoxWidth(unsigned int max) {
	unsigned int digits = 1;
	for (unsigned int at = max; at >= 10; at /= 10)
		digits++;
	unsigned int width = (digits * (unsigned int)Texthook::GetTextSize("A", 0).x) +
		(2 * INPUT_PADDING_X);
	return (width > SETTINGS_NUMBER_WIDTH) ? width : SETTINGS_NUMBER_WIDTH;
}

SettingsPanel::SettingsPanel(std::string category,
		const std::vector<std::string>& categories, UI* ui) :
	UIPanel(category, ui),
	category(category),
	categories(categories),
	shownHelp(-1),
	focusRow(-1),
	scrollToRow(-1),
	markerWidth(0),
	builtVersion(0),
	laidOutWidth(0),
	laidOutHeight(0),
	needsLayout(true) {

	box = new Scrollhook(tab, UI_CONTENT_MARGIN, 0, 0, 0);

	// Placed and switched on by UpdateHelp().
	helpTip = new Tooltiphook(InGame, 0, 0);
	helpTip->SetActive(false);
}

SettingsPanel::~SettingsPanel() {
	// The box owns every control that was put in a row, so there is nothing else
	// to take down here.
	delete box;
	delete helpTip;
}

void SettingsPanel::SetCategories(const std::vector<std::string>& all) {
	if (all == categories)
		return;
	categories = all;
	// Below any version the registry can be at, so the next draw builds again
	// against the tabs the window now has.
	builtVersion = 0;
}

std::string SettingsPanel::GetSearchPlaceholder() {
	return "Search all settings by name";
}

// Counted in settings rather than rows: the headings and notes between them are
// not things anyone came looking for.
//
// Against every tab while a search is running and against this one otherwise,
// which is the same list either way: what the panel would show if nothing were
// filtered or folded.
std::string SettingsPanel::GetStatus() {
	unsigned int total = 0, matching = 0;
	for (unsigned int i = 0; i < rows.size(); i++) {
		if (!IsSetting(rows[i]))
			continue;
		if (query.empty() && rows[i].category.compare(category) != 0)
			continue;
		total++;
		for (unsigned int p = 0; p < shown.size(); p++) {
			if (shown[p] == i) {
				matching++;
				break;
			}
		}
	}

	char line[64];
	if (query.length() > 0)
		sprintf_s(line, sizeof(line), "%u of %u settings", matching, total);
	else
		sprintf_s(line, sizeof(line), "%u setting%s", total, (total == 1) ? "" : "s");
	return line;
}

bool SettingsPanel::IsSetting(const Row& row) {
	if (!row.setting)
		return false;
	return row.setting->kind != Settings::KindNote &&
		row.setting->kind != Settings::KindHeading;
}

// A heading is reachable only while it folds. During a search it is a label and
// nothing more, and focus steps over it the way it steps over a note.
bool SettingsPanel::IsFocusable(const Row& row) {
	if (IsSetting(row))
		return true;
	return row.setting && row.setting->kind == Settings::KindHeading &&
		FoldingActive();
}

void SettingsPanel::AddHeadingRow(HookGroup* content, const std::string& category,
		const Settings::Descriptor* setting) {
	Row row;
	row.setting = setting;
	row.category = category;
	row.heading = true;
	// The bar is what a click lands on and the marker is what says which way the
	// heading is folded. Neither is built for the heading a category's unheaded
	// settings are gathered under: that one is never on screen except during a
	// search, where every heading is a label and nothing more.
	if (setting) {
		row.bar = new SettingsHeadingBar(content, this, (unsigned int)rows.size());
		row.marker = new Texthook(content, 0, 0, "");
		row.marker->SetColor(SETTINGS_HEADING_COLOR);
		row.count = new Texthook(content, 0, 0, "");
		row.count->SetColor(SETTINGS_COUNT_COLOR);
	}

	// Set as it is laid out, since what a heading reads as depends on whether a
	// search is running.
	row.label = new Texthook(content, 0, 0, "");
	row.label->SetColor(SETTINGS_HEADING_COLOR);

	rows.push_back(row);
}

// A control per setting, built once. What each kind gets follows from what the
// game already used for it, so a setting looks the way it always did even though
// nothing places it by hand any more.
//
// Every tab's settings, not only this one's: a search crosses the tabs, and a
// result has to be a control the user can reach for rather than a line of text
// saying where one is. Which of them are on screen is the filter's business.
void SettingsPanel::Build() {
	builtVersion = Settings::Version();
	// Not just the rows: the controls themselves go, or the previous set would
	// linger in the box, drawn by nothing and still clickable where it was.
	box->ClearContents();
	rows.clear();
	byKey.clear();
	focusRow = -1;

	HookGroup* content = box->GetContent();

	// Before the window has said what its tabs are, which is only ever the case if
	// a panel draws before the window has built them all.
	std::vector<std::string> tabs = categories;
	if (tabs.empty())
		tabs.push_back(category);

	for (unsigned int c = 0; c < tabs.size(); c++)
		BuildCategory(content, tabs[c]);

	// Every kind's name is coloured in one place, so a row starts out the colour
	// it will go on being drawn in.
	ApplyFocusColors();

	ApplyFilter();
	needsLayout = true;
}

void SettingsPanel::BuildCategory(HookGroup* content, const std::string& category) {
	std::vector<const Settings::Descriptor*> settings = InSectionOrder(category);

	// The settings a module registered without a heading come first, so the
	// heading that stands in for them is built before any of them. It says which
	// tab they are on and nothing else, which is all a search needs of it and more
	// than the tab itself needs.
	int heading = (int)rows.size();
	AddHeadingRow(content, category, NULL);

	for (unsigned int i = 0; i < settings.size(); i++) {
		const Settings::Descriptor* setting = settings[i];

		if (setting->kind == Settings::KindHeading) {
			// Folded on the way past, the first time it is ever seen. A heading
			// registered later starts folded like the rest rather than opening
			// itself, and one the user has since opened stays open through however
			// many rebuilds follow.
			if (seenHeadings.insert(setting->label).second)
				folded.insert(setting->label);
			heading = (int)rows.size();
			AddHeadingRow(content, category, setting);
			continue;
		}

		Row row;
		row.setting = setting;
		row.category = category;
		row.headingRow = heading;
		row.searchKey = ToLower(setting->label + " " + setting->key);
		row.indent = setting->parent.length() > 0 ? SETTINGS_INDENT : 0;

		switch (setting->kind) {
			case Settings::KindBool:
				row.control = new Checkhook(content, 0, 0, setting->boolValue,
					"%s", setting->label.c_str());
				break;

			case Settings::KindToggle:
				row.control = new Checkhook(content, 0, 0,
					&setting->toggleValue->state, "%s", setting->label.c_str());
				row.hotkey = new Keyhook(content, 0, 0,
					&setting->toggleValue->toggle, "");
				break;

			case Settings::KindKey:
				row.hotkey = new Keyhook(content, 0, 0, setting->intValue, "");
				row.label = new Texthook(content, 0, 0, "%s", setting->label.c_str());
				break;

			case Settings::KindEnum:
				row.label = new Texthook(content, 0, 0, "%s", setting->label.c_str());
				row.control = new Combohook(content, 0, 0, SETTINGS_ENUM_WIDTH,
					setting->intValue, setting->options);
				break;

			case Settings::KindColor:
				row.control = new Colorhook(content, 0, 0, setting->intValue,
					"%s", setting->label.c_str());
				break;

			case Settings::KindNumber:
				row.label = new Texthook(content, 0, 0, "%s", setting->label.c_str());
				row.control = new Inputhook(content, 0, 0,
					NumberBoxWidth(setting->numberMax),
					"%u", setting->intValue ? *setting->intValue : 0);
				((Inputhook*)row.control)->SetCompact(true);
				break;

			case Settings::KindText:
				row.label = new Texthook(content, 0, 0, "%s", setting->label.c_str());
				row.control = new Inputhook(content, 0, 0, SETTINGS_TEXT_WIDTH, "%s",
					setting->textValue ? setting->textValue->c_str() : "");
				// Selected rather than cleared: a name worth keeping stays readable
				// until it is typed over, and backspace still edits it.
				((Inputhook*)row.control)->SetSelectOnFocus(true);
				((Inputhook*)row.control)->SetCompact(true);
				break;

			case Settings::KindHeading:
				break;		// a row of its own, built before the switch is reached

			case Settings::KindNote:
				// A line at a time, up to the cap. Which of them carry text is
				// decided as it is laid out, since that depends on the width.
				for (unsigned int line = 0; line < SETTINGS_NOTE_MAX_LINES; line++) {
					Texthook* text = new Texthook(content, 0, 0, "");
					text->SetColor(SETTINGS_NOTE_COLOR);
					row.noteLines.push_back(text);
				}
				break;
		}

		if (setting->key.length() > 0)
			byKey[setting->key] = (unsigned int)rows.size();
		rows.push_back(row);
	}
}

// The section alone within its own tab, since the tab strip is already saying
// which tab that is. While a search is running the results come from every tab,
// so each heading says which one its rows are on - the heading standing in for a
// category's unheaded settings having nothing else to say at all.
std::string SettingsPanel::HeadingText(const Row& row) {
	std::string section = row.setting ? row.setting->label : std::string();
	if (query.empty())
		return section;
	if (section.empty())
		return row.category;
	return row.category + SETTINGS_HEADING_SEPARATOR + section;
}

// Whether the row is folded away under its heading. A heading is never folded by
// itself: what folds is everything under it.
bool SettingsPanel::IsFolded(const Row& row) {
	if (!FoldingActive() || row.headingRow < 0)
		return false;
	const Settings::Descriptor* heading = rows[row.headingRow].setting;
	return heading && folded.count(heading->label) > 0;
}

// Whether the heading itself is shut, which is a different question from whether
// a row is hidden by one.
bool SettingsPanel::IsHeadingFolded(const Row& row) {
	return row.setting && folded.count(row.setting->label) > 0;
}

// What a folded heading says it is hiding. Settings rather than rows: the notes
// and the headings between them are not what anyone is counting, and a number
// larger than the controls that appear when it is opened would read as wrong.
unsigned int SettingsPanel::SettingsUnder(int headingRow) {
	unsigned int count = 0;
	for (unsigned int i = 0; i < rows.size(); i++) {
		if (rows[i].headingRow == headingRow && IsSetting(rows[i]))
			count++;
	}
	return count;
}

void SettingsPanel::ToggleHeading(unsigned int row) {
	if (row >= rows.size() || !rows[row].setting || !FoldingActive())
		return;

	const std::string& label = rows[row].setting->label;
	if (folded.count(label) > 0)
		folded.erase(label);
	else
		folded.insert(label);

	// The heading that was clicked stays where it was clicked, so the section
	// opens under the cursor rather than the panel jumping. Not a reset to the
	// top: a fold is the same list, unlike a new query.
	scrollToRow = (int)row;
	focusRow = (int)row;
	ApplyFilter();
}

// What is on screen: this tab, or - while a search is running - whatever matched
// it, from every tab.
//
// A heading is never matched against the query and never shown for its own sake.
// It is shown when something under it survived, so that a result keeps the name
// of the section it lives in however few of that section's settings matched, and
// so that matching a heading's own words cannot bring back a section none of
// whose settings were asked for.
void SettingsPanel::ApplyFilter() {
	std::vector<bool> wanted(rows.size(), false);
	for (unsigned int i = 0; i < rows.size(); i++) {
		const Row& row = rows[i];

		if (query.empty()) {
			if (row.category.compare(category) != 0)
				continue;
			// A heading is the only way back to what it is folding, so it stays on
			// screen whether or not anything under it does. The one standing in for
			// the settings registered without a heading does not: it names the tab,
			// which the tab strip is already doing.
			if (row.heading)
				wanted[i] = (row.setting != NULL);
			else
				wanted[i] = !IsFolded(row);
			continue;
		}

		if (row.heading || row.searchKey.find(query) == std::string::npos)
			continue;
		wanted[i] = true;
		if (row.headingRow >= 0)
			wanted[row.headingRow] = true;
	}

	shown.clear();
	for (unsigned int i = 0; i < rows.size(); i++) {
		if (wanted[i])
			shown.push_back(i);
	}
	// Keep the setting the user was on if it survived the filter. A heading it
	// was on is let go of once a search starts: it is still on screen, but it is a
	// label for as long as the query lasts and there is nothing there to press.
	if (focusRow >= 0 && (FocusPosition() < 0 || !IsFocusable(rows[focusRow])))
		focusRow = -1;
	needsLayout = true;
}

// What a row needs to put every hook on it on one line of text.
//
// The hooks on a row are different heights and each draws its text at its own
// inset from its top - a label at nothing, a checkbox two pixels down, a text box
// five - so centring the hooks against each other steps their text. What is
// centred is the line of text: the room above it is whatever the deepest inset
// needs, the room below it whatever hangs under the text of the tallest hook, and
// each hook is then placed so its own text lands on that line.
struct RowMetrics {
	unsigned int above;		// hook top to the line of text
	unsigned int text;		// the line itself
	unsigned int below;		// the line to the bottom of the deepest hook
	unsigned int Height() const { return above + text + below; };
	RowMetrics() : above(0), text(0), below(0) {};
};

static RowMetrics MeasureRow(Hook* const* hooks, unsigned int count) {
	RowMetrics metrics;
	// Every control in the panel draws in the default font; a row measured off one
	// hook's text would be wrong for the others if that stopped being true.
	metrics.text = (unsigned int)Texthook::GetTextSize("A", 0).y;

	for (unsigned int i = 0; i < count; i++) {
		if (!hooks[i])
			continue;
		unsigned int inset = hooks[i]->GetTextInset();
		unsigned int height = hooks[i]->GetYSize();
		unsigned int under = (height > inset + metrics.text) ?
			(height - inset - metrics.text) : 0;
		if (inset > metrics.above)
			metrics.above = inset;
		if (under > metrics.below)
			metrics.below = under;
	}
	return metrics;
}

// Both markers, at the font every control in the panel draws in. Measured as the
// panel is laid out rather than once, since the font is not the panel's to fix.
void SettingsPanel::MeasureMarkers() {
	unsigned int shut = (unsigned int)Texthook::GetTextSize(SETTINGS_FOLDED, 0).x;
	unsigned int open = (unsigned int)Texthook::GetTextSize(SETTINGS_UNFOLDED, 0).x;
	markerWidth = (shut > open) ? shut : open;
}

void SettingsPanel::Relayout() {
	laidOutWidth = tab->GetXSize();
	laidOutHeight = tab->GetYSize();
	needsLayout = false;

	unsigned int width = (laidOutWidth > 2 * UI_CONTENT_MARGIN) ?
		(laidOutWidth - (2 * UI_CONTENT_MARGIN)) : 0;
	box->SetSize(width, laidOutHeight);

	// A row that the filter has dropped is not drawn, but it would still be
	// clicked where it used to be, so it is switched off rather than just left out
	// of the layout.
	for (unsigned int i = 0; i < rows.size(); i++) {
		Row& row = rows[i];
		if (row.bar) row.bar->SetActive(false);
		if (row.marker) row.marker->SetActive(false);
		if (row.count) row.count->SetActive(false);
		if (row.label) row.label->SetActive(false);
		if (row.control) row.control->SetActive(false);
		if (row.hotkey) row.hotkey->SetActive(false);
		for (unsigned int line = 0; line < row.noteLines.size(); line++)
			row.noteLines[line]->SetActive(false);
	}

	box->ClearRows();
	unsigned int contentWidth = box->GetContentWidth();
	MeasureMarkers();

	for (unsigned int position = 0; position < shown.size(); position++) {
		Row& row = rows[shown[position]];
		bool heading = row.heading;
		bool note = (row.setting && row.setting->kind == Settings::KindNote);

		// Before it is measured: what a heading reads as decides how wide it is,
		// and the count that follows a folded one is placed against that width.
		if (heading)
			row.label->SetText("%s", HeadingText(row).c_str());

		Hook* named = NamedHook(row);
		Hook* onRow[] = { named, (row.control != named) ? row.control : NULL,
			row.hotkey };
		RowMetrics metrics = MeasureRow(onRow, 3);

		// A note is as tall as its text needs, which depends on the width, so its
		// lines are worked out here rather than when it was built.
		std::vector<std::string> lines;
		if (note) {
			unsigned int room = (contentWidth > row.indent) ?
				(contentWidth - row.indent) : 0;
			lines = WrapText(row.setting->label, room, 0);
			if (lines.size() > row.noteLines.size())
				lines.resize(row.noteLines.size());
			row.height = (unsigned int)lines.size() * SETTINGS_NOTE_LINE;
		} else if (heading) {
			row.height = SETTINGS_HEADING_HEIGHT;
		} else {
			// Never less than the standard line, and more where what is on the row
			// needs it: a control that reached past the row would be drawn over its
			// neighbour below and take the clicks meant for it.
			row.height = (metrics.Height() > SETTINGS_ROW_HEIGHT) ?
				metrics.Height() : SETTINGS_ROW_HEIGHT;
		}

		unsigned int index = box->AddRow(row.height);
		unsigned int rowY = box->GetRowY(index);

		// The line every hook on the row puts its text on, with the slack left over
		// shared above and below it. A row can be taller than what is on it - the
		// standard line, or a note - so the slack is taken rather than assumed.
		unsigned int slack = (row.height > metrics.Height()) ?
			(row.height - metrics.Height()) : 0;
		unsigned int textY = rowY + metrics.above + (slack / 2);

		if (note) {
			for (unsigned int line = 0; line < lines.size(); line++) {
				Texthook* text = row.noteLines[line];
				text->SetText("%s", lines[line].c_str());
				text->SetBaseX(row.indent);
				text->SetBaseY(rowY + (line * SETTINGS_NOTE_LINE));
				box->AddToRow(index, text);
			}
			// The rest keep an empty string, so a shorter note leaves nothing of
			// the longer one it replaced behind.
			for (unsigned int line = (unsigned int)lines.size();
					line < row.noteLines.size(); line++)
				row.noteLines[line]->SetText("");
			continue;
		}

		// A heading carries its own furniture, and only while it folds: during a
		// search it is a plain label, since a marker beside a heading that will not
		// answer a click would be saying something untrue.
		unsigned int headingX = row.indent;
		if (heading && row.bar && FoldingActive()) {
			row.bar->SetBaseX(0);
			row.bar->SetBaseY(rowY);
			row.bar->SetXSize(contentWidth);
			row.bar->SetYSize(row.height);
			box->AddToRow(index, row.bar);

			row.marker->SetText("%s", IsHeadingFolded(row) ?
				SETTINGS_FOLDED : SETTINGS_UNFOLDED);
			// Centred in the column the two markers share, so neither sits off to
			// one side of the other.
			unsigned int own = row.marker->GetXSize();
			row.marker->SetBaseX(row.indent +
				((markerWidth > own) ? ((markerWidth - own) / 2) : 0));
			row.marker->SetBaseY(rowY + SETTINGS_HEADING_TOP);
			box->AddToRow(index, row.marker);

			headingX = row.indent + markerWidth + SETTINGS_MARKER_GAP;
		}

		if (named) {
			named->SetBaseX(heading ? headingX : row.indent);
			named->SetBaseY(heading ? (rowY + SETTINGS_HEADING_TOP) :
				(textY - named->GetTextInset()));
			box->AddToRow(index, named);
		}

		// Only where it is folded, and only where there is something to count: a
		// heading over nothing but notes would otherwise read [0].
		if (heading && row.count && FoldingActive() && IsHeadingFolded(row)) {
			unsigned int hidden = SettingsUnder((int)shown[position]);
			if (hidden > 0) {
				row.count->SetText("[%u]", hidden);
				row.count->SetBaseX(headingX + named->GetXSize() + SETTINGS_COUNT_GAP);
				row.count->SetBaseY(rowY + SETTINGS_HEADING_TOP);
				box->AddToRow(index, row.count);
			}
		}

		// The control, against the right edge so it tracks the window rather than
		// sitting at a column somebody once measured.
		if (row.control && row.control != named) {
			row.control->SetAlignment(Right);
			row.control->SetBaseY(textY - row.control->GetTextInset());
			box->AddToRow(index, row.control);
		}

		if (row.hotkey) {
			row.hotkey->SetAlignment(Right);
			row.hotkey->SetBaseY(textY - row.hotkey->GetTextInset());
			box->AddToRow(index, row.hotkey);
		}
	}

	// A heading that was just folded or unfolded holds its place, so the section
	// opens beneath the bar rather than the panel moving out from under it.
	// Otherwise the focused setting, and only if there is one: a resize would
	// otherwise throw the view back to the top.
	int position = -1;
	if (scrollToRow >= 0) {
		for (unsigned int i = 0; i < shown.size() && position < 0; i++) {
			if ((int)shown[i] == scrollToRow)
				position = (int)i;
		}
		scrollToRow = -1;
	}
	if (position < 0)
		position = FocusPosition();
	if (position >= 0)
		box->ScrollRowIntoView((unsigned int)position);
}

// Whether the setting a row stands for is switched on, for the benefit of the
// settings that depend on it. Anything that is not a switch counts as on: a
// colour is not the kind of thing that gates other settings.
bool SettingsPanel::SettingIsOn(const Row& row) {
	if (!row.setting)
		return true;
	switch (row.setting->kind) {
		case Settings::KindBool:
			return row.setting->boolValue ? *row.setting->boolValue : true;
		case Settings::KindToggle:
			return row.setting->toggleValue ? row.setting->toggleValue->state : true;
		default:
			return true;
	}
}

void SettingsPanel::SetRowEnabled(Row& row, bool enabled) {
	if (row.label) row.label->SetEnabled(enabled);
	if (row.control) row.control->SetEnabled(enabled);
	if (row.hotkey) row.hotkey->SetEnabled(enabled);
	for (unsigned int line = 0; line < row.noteLines.size(); line++)
		row.noteLines[line]->SetEnabled(enabled);
}

// One pass in registration order, so a setting under a setting that is itself
// under a third is off when either above it is. A parent registered after its own
// child is not found, and the child is left alone rather than guessed at.
void SettingsPanel::ApplyDependencies() {
	for (unsigned int i = 0; i < rows.size(); i++) {
		Row& row = rows[i];
		row.enabled = true;

		if (row.setting && row.setting->parent.length() > 0) {
			std::map<std::string, unsigned int>::iterator found =
				byKey.find(row.setting->parent);
			if (found != byKey.end() && found->second < i) {
				Row& above = rows[found->second];
				row.enabled = above.enabled && SettingIsOn(above);
			}
		}

		SetRowEnabled(row, row.enabled);
	}
}

// Whether the cursor is over a hook, whether or not that hook is taking input.
// Hook::InRange answers no for anything disabled, which is the wrong question
// here: a setting greyed out because its parent is off is precisely the one whose
// help someone is reaching for.
static bool Over(Hook* hook) {
	if (!hook || !hook->IsActive())
		return false;
	return UI::InPos((*p_D2CLIENT_MouseX), (*p_D2CLIENT_MouseY),
		hook->GetX(), hook->GetY(), hook->GetXSize(), hook->GetYSize());
}

Hook* SettingsPanel::NamedHook(const Row& row) {
	return row.label ? (Hook*)row.label : row.control;
}

// The three hooks a name can be drawn by, given the colours to draw it in. They
// share no base class that knows about text colour, so which of them a row has is
// decided from its kind, the same way it was built.
static void ColorName(Hook* named, Settings::Kind kind, TextColor resting,
		TextColor hover, TextColor off) {
	switch (kind) {
		case Settings::KindBool:
		case Settings::KindToggle: {
			Checkhook* check = (Checkhook*)named;
			check->SetTextColor(resting);
			check->SetHoverColor(hover);
			check->SetDisabledColor(off);
			break;
		}

		case Settings::KindColor: {
			Colorhook* color = (Colorhook*)named;
			color->SetTextColor(resting);
			color->SetHoverColor(hover);
			color->SetDisabledColor(off);
			break;
		}

		// Everything else is named by a Texthook: a label beside a control, or a
		// heading.
		default: {
			Texthook* text = (Texthook*)named;
			text->SetColor(resting);
			text->SetHoverColor(hover);
			text->SetDisabledColor(off);
			break;
		}
	}
}

void SettingsPanel::ApplyFocusColors() {
	for (unsigned int i = 0; i < rows.size(); i++) {
		Row& row = rows[i];
		if (!row.setting || row.setting->kind == Settings::KindNote)
			continue;
		Hook* named = NamedHook(row);
		if (!named)
			continue;

		// A heading is not a value to change, but it does fold, and the whole bar
		// is what folds it: it lifts under the mouse anywhere along the row rather
		// than only over its text, which is as far as a Texthook would know to
		// look. Its own hover colour is therefore settled here rather than left to
		// the hook.
		bool heading = (row.setting->kind == Settings::KindHeading);
		TextColor resting = SETTINGS_LABEL_COLOR;
		TextColor hover = SETTINGS_LABEL_HOVER;
		if (heading) {
			resting = (row.bar && Over(row.bar)) ?
				SETTINGS_HEADING_HOVER : SETTINGS_HEADING_COLOR;
			hover = resting;
		}
		TextColor off = DISABLED_TEXT_COLOR;

		if ((int)i == focusRow) {
			resting = hover = SETTINGS_FOCUS_COLOR;
			off = SETTINGS_FOCUS_DISABLED;
		}

		ColorName(named, row.setting->kind, resting, hover, off);

		// The marker reads as part of the label, so it goes wherever the label goes.
		if (heading && row.marker)
			row.marker->SetColor(resting);
	}
}

// Follows the mouse over the setting's name, which is the target the cursor is
// already on. Rebuilt only when the row changes, as the cursor rests on one row
// for many frames.
void SettingsPanel::UpdateHelp() {
	int over = -1;
	unsigned int overPosition = 0;
	if (IsActive()) {
		for (unsigned int position = 0; position < shown.size() && over < 0; position++) {
			Row& row = rows[shown[position]];
			if (!row.setting || row.setting->help.empty())
				continue;
			if (Over(NamedHook(row))) {
				over = (int)shown[position];
				overPosition = position;
			}
		}
	}

	if (over < 0) {
		helpTip->SetActive(false);
		shownHelp = -1;
		return;
	}

	if (over != shownHelp) {
		// The help alone: the row it is under names the setting already, and
		// repeating that name in the panel made a two line tip of a one line one.
		std::vector<TooltipLine> lines;
		lines.push_back(TooltipLine(rows[over].setting->help, Grey));
		helpTip->SetLines(lines);
		shownHelp = over;
	}

	// Over the whole row rather than the name the cursor happens to be on, so the
	// tip lands in the same place whatever kind of row it describes. Rows go into
	// the box in the order they are shown, so a row's position is also its row in
	// the box. Must follow SetLines(): where it fits depends on how big it turned
	// out.
	HookGroup* content = box->GetContent();
	unsigned int width = box->GetContentWidth();
	helpTip->SetMaxWidth(width);
	helpTip->PlaceAbove(content->GetX(),
		content->GetY() + box->GetRowY(overPosition), width,
		box->GetRowHeight(overPosition));
	helpTip->SetActive(true);
}

// A number box holds text; the setting behind it is a number. Nothing else knows
// how to get from one to the other, so this does, in one place rather than in
// every module that owns a number.
void SettingsPanel::SyncNumbers() {
	for (unsigned int i = 0; i < rows.size(); i++) {
		Row& row = rows[i];
		if (!row.setting || row.setting->kind != Settings::KindNumber)
			continue;
		Inputhook* input = (Inputhook*)row.control;
		if (!input || !row.setting->intValue)
			continue;

		std::string text = input->GetText();
		if (input->IsFocused()) {
			// Typed into, so the box leads and the setting follows. Anything that
			// is not a number reads as nothing typed yet rather than as zero.
			if (text.length() > 0) {
				unsigned int typed = (unsigned int)strtoul(text.c_str(), NULL, 10);
				if (row.setting->numberMax > 0 && typed > row.setting->numberMax)
					typed = row.setting->numberMax;
				*row.setting->intValue = typed;
			}
			continue;
		}

		// Not being typed into, so the setting leads: it may have been changed by a
		// reload or by something else entirely.
		char expected[16];
		sprintf_s(expected, sizeof(expected), "%u", *row.setting->intValue);
		if (text.compare(expected) != 0)
			input->SetText("%s", expected);
	}
}

// A text box holds the setting itself, so there is nothing to convert - only the
// length limit to keep, which the box knows nothing about.
void SettingsPanel::SyncText() {
	for (unsigned int i = 0; i < rows.size(); i++) {
		Row& row = rows[i];
		if (!row.setting || row.setting->kind != Settings::KindText)
			continue;
		Inputhook* input = (Inputhook*)row.control;
		if (!input || !row.setting->textValue)
			continue;

		std::string text = input->GetText();
		if (input->IsFocused()) {
			// Typed into, so the box leads. Cut to the limit as it is typed rather
			// than when it is saved, so what is on screen is what the setting holds.
			unsigned int max = row.setting->textMax;
			if (max > 0 && text.length() > max) {
				text = text.substr(0, max);
				input->SetText("%s", text.c_str());
			}
			if (text.compare(*row.setting->textValue) != 0)
				*row.setting->textValue = text;
			continue;
		}

		// Not being typed into, so the setting leads: it may have been reverted or
		// read again from the file.
		if (text.compare(*row.setting->textValue) != 0)
			input->SetText("%s", row.setting->textValue->c_str());
	}
}

// The caret does not outlive the window being closed: a box left focused would
// still have it when the window came back, in front of someone who had not clicked
// into anything.
void SettingsPanel::ClearInputFocus() {
	for (unsigned int i = 0; i < rows.size(); i++) {
		Row& row = rows[i];
		if (!row.setting)
			continue;
		if (row.setting->kind != Settings::KindNumber &&
				row.setting->kind != Settings::KindText)
			continue;
		Inputhook* input = (Inputhook*)row.control;
		if (input)
			input->SetFocused(false);
	}
}

// Both kinds of box, since either can have the caret.
bool SettingsPanel::InputFocused() {
	for (unsigned int i = 0; i < rows.size(); i++) {
		Row& row = rows[i];
		if (!row.setting)
			continue;
		if (row.setting->kind != Settings::KindNumber &&
				row.setting->kind != Settings::KindText)
			continue;
		Inputhook* input = (Inputhook*)row.control;
		if (input && input->IsActive() && input->IsFocused())
			return true;
	}
	return false;
}

void SettingsPanel::Search(const std::string& text) {
	query = ToLower(Trim(text));
	box->SetScrollRow(0);
	ApplyFilter();
}

void SettingsPanel::OnDraw() {
	if (builtVersion != Settings::Version())
		Build();

	if (needsLayout || tab->GetXSize() != laidOutWidth ||
			tab->GetYSize() != laidOutHeight)
		Relayout();

	ApplyDependencies();
	ApplyFocusColors();
	SyncNumbers();
	SyncText();
	UpdateHelp();
}

// Where the focused setting currently sits on screen, or -1 if it is filtered out
// or there is nothing focused. Rows on screen and rows in the panel are not the
// same list, and this is the only place that knows how to get from one to the
// other.
int SettingsPanel::FocusPosition() {
	if (focusRow < 0)
		return -1;
	for (unsigned int i = 0; i < shown.size(); i++) {
		if ((int)shown[i] == focusRow)
			return (int)i;
	}
	return -1;
}

// Headings and notes are stepped over rather than landed on: there is nothing to
// be done to them, so stopping there would read as the key having failed.
void SettingsPanel::SetFocusPosition(int position) {
	if (shown.empty()) {
		focusRow = -1;
		return;
	}
	if (position < 0)
		position = 0;
	if (position >= (int)shown.size())
		position = (int)shown.size() - 1;

	int at = position;
	while (at < (int)shown.size() && !IsFocusable(rows[shown[at]]))
		at++;
	if (at >= (int)shown.size()) {
		// Nothing focusable below it, so look back up instead.
		at = position;
		while (at >= 0 && !IsFocusable(rows[shown[at]]))
			at--;
	}
	if (at < 0)
		return;

	focusRow = (int)shown[at];
	box->ScrollRowIntoView((unsigned int)at);
}

void SettingsPanel::MoveFocus(int delta) {
	int position = FocusPosition();
	// Nothing focused yet, so the first press lands on something the user can
	// already see rather than back at the top of the panel.
	if (position < 0) {
		SetFocusPosition((int)box->GetFirstVisibleRow());
		return;
	}

	// Headings and notes are stepped over rather than counted as travel, so a
	// press always moves by a whole setting.
	int at = position;
	int steps = (delta < 0) ? -delta : delta;
	int direction = (delta < 0) ? -1 : 1;
	for (int taken = 0; taken < steps; taken++) {
		int next = at + direction;
		while (next >= 0 && next < (int)shown.size() &&
				!IsFocusable(rows[shown[next]]))
			next += direction;
		if (next < 0 || next >= (int)shown.size())
			break;
		at = next;
	}
	SetFocusPosition(at);
}

// Space and enter do whatever the focused row's control does, for the kinds where
// that means anything. A colour or a hotkey opens a picker of its own, which is a
// gesture rather than a value to step through.
void SettingsPanel::ActuateFocused() {
	// Only a setting actually on screen, and only one whose parent is on: a greyed
	// out setting does nothing when clicked, so it should do nothing here either.
	if (focusRow < 0 || focusRow >= (int)rows.size() || FocusPosition() < 0)
		return;
	if (!rows[focusRow].enabled)
		return;

	const Settings::Descriptor* setting = rows[focusRow].setting;
	if (!setting)
		return;

	// A heading has no value to step through: what it does is fold.
	if (setting->kind == Settings::KindHeading) {
		ToggleHeading((unsigned int)focusRow);
		return;
	}

	switch (setting->kind) {
		case Settings::KindBool:
			if (setting->boolValue)
				*setting->boolValue = !*setting->boolValue;
			break;
		case Settings::KindToggle:
			if (setting->toggleValue)
				setting->toggleValue->state = !setting->toggleValue->state;
			break;
		case Settings::KindEnum:
			if (setting->intValue && !setting->options.empty()) {
				*setting->intValue =
					(*setting->intValue + 1) % (unsigned int)setting->options.size();
			}
			break;
		default:
			break;
	}
}

bool SettingsPanel::OnKey(bool up, BYTE key) {
	// A box being typed into owns the keys that move the caret. The panel gets
	// first refusal on every key, so without this, home and end would jump the list
	// while someone was part way through editing a name.
	if (InputFocused())
		return false;

	switch (key) {
		case VK_UP:
		case VK_DOWN:
		case VK_PRIOR:
		case VK_NEXT:
		case VK_HOME:
		case VK_END: {
			if (up)
				return true;
			// A boxful less one row, so the row being read stays on screen.
			int visible = (int)box->GetVisibleRowCount();
			int step = (visible > 1) ? (visible - 1) : 1;
			switch (key) {
				case VK_UP:		MoveFocus(-1); break;
				case VK_DOWN:	MoveFocus(1); break;
				case VK_PRIOR:	MoveFocus(-step); break;
				case VK_NEXT:	MoveFocus(step); break;
				case VK_HOME:	SetFocusPosition(0); break;
				case VK_END:	SetFocusPosition((int)shown.size() - 1); break;
			}
			return true;
		}

		// Folding from the keyboard, as the sets panel does it. Left on a setting
		// goes out to its heading and shuts it, so there is always a way back out
		// of a section without reaching for the mouse.
		case VK_LEFT:
		case VK_RIGHT: {
			if (!FoldingActive() || focusRow < 0)
				return false;
			if (up)
				return true;

			Row& row = rows[focusRow];
			bool heading = row.setting &&
				row.setting->kind == Settings::KindHeading;

			if (key == VK_LEFT) {
				if (heading) {
					if (IsHeadingFolded(row))
						return true;		// already shut, and nowhere further out
					ToggleHeading((unsigned int)focusRow);
				} else if (row.headingRow >= 0) {
					ToggleHeading((unsigned int)row.headingRow);
				}
			} else if (heading) {
				if (IsHeadingFolded(row))
					ToggleHeading((unsigned int)focusRow);
				else
					MoveFocus(1);			// open already, so step into it
			}
			return true;
		}

		case VK_SPACE:
		case VK_RETURN:
			if (focusRow < 0)
				return false;
			if (!up)
				ActuateFocused();
			return true;
	}
	return false;
}

void SettingsPanel::OnClose() {
	ClearInputFocus();
	helpTip->SetActive(false);
	shownHelp = -1;
	focusRow = -1;
	Search("");
}
