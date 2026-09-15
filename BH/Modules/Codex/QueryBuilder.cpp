#include "QueryBuilder.h"
#include <cstdlib>
#include "../../StringUtil.h"

using namespace Drawing;

// The row, left to right: what stat, how it compares, against what, and the
// button that takes the row away. Only the stat box grows with the window - the
// others hold a comparator and a number, neither of which reads better wide.
#define QB_CELL_GAP			4
#define QB_COMPARATOR_WIDTH	28
#define QB_VALUE_WIDTH		44
#define QB_ROW_GAP			3
#define QB_BAND_TOP			4
#define QB_BAND_BOTTOM		2

// Clear space above the add line, beyond the gap the rows already keep between
// themselves. It is not another row, and sitting a row's distance below the last
// one is what made it look like one.
#define QB_ADD_LINE_TOP		7

// How far the picker hangs below a stat box, in rows. Enough to choose from
// without covering the list it is narrowing.
#define QB_PICKER_ROWS		6

// Clear space between the stat box and the panel the picker draws itself on, so
// the two read as a box and a list under it rather than as one control.
#define QB_PICKER_DROP		5

// The picker shows one thing, which is what the stat reads as. The vocabulary
// promises those are distinct, so there is nothing for a second column to tell
// apart.
#define QB_PICKER_LABEL_WEIGHT	1

namespace {

// The three the index offers, in the order the combo box lists them.
const char* const kComparators[] = { ">", "<", "=" };

StatIndex::Comparator ComparatorAt(unsigned int index) {
	switch (index) {
		case 1:		return StatIndex::LessThan;
		case 2:		return StatIndex::EqualTo;
		default:	return StatIndex::GreaterThan;
	}
}

// Whether the box holds a number, as opposed to nothing or the start of one.
// A row with anything else in it is a row still being written, and filtering on
// a value nobody typed would empty the list while they wrote it.
bool ParseValue(const std::string& text, int& value) {
	std::string trimmed = Trim(text);
	if (trimmed.empty())
		return false;

	unsigned int digits = 0;
	for (unsigned int i = 0; i < trimmed.length(); i++) {
		if (i == 0 && (trimmed[i] == '-' || trimmed[i] == '+'))
			continue;
		if (trimmed[i] < '0' || trimmed[i] > '9')
			return false;
		digits++;
	}
	if (digits == 0)
		return false;

	value = atoi(trimmed.c_str());
	return true;
}

}	// namespace

QueryBuilder::QueryBuilder(UI* ui) : ui(ui), addLine(NULL), picker(NULL),
		shown(false), panelOffers(true), rowCount(1), maxRows(MaxRows), activeCount(0),
		revision(0), pickerRow(MaxRows), laidOutWidth(0), laidOutY(0) {

	HookGroup* chrome = ui->GetChrome();

	std::vector<std::string> comparators;
	for (unsigned int i = 0; i < (sizeof(kComparators) / sizeof(kComparators[0])); i++)
		comparators.push_back(kComparators[i]);

	// Every row is built now and switched on as it is needed. A control built
	// later would be drawn after the picker, and so over the top of it, the
	// chrome drawing its hooks in the order it was given them.
	for (unsigned int i = 0; i < MaxRows; i++) {
		Row* row = new Row();

		row->stat = new Inputhook(chrome, 0, 0, 0, "");
		row->stat->SetCompact(true);
		row->stat->SetPlaceholder("Stat");
		row->stat->SetSelectOnFocus(true);
		// A box draws its hint in grey, so an unfocused box whose text is also
		// grey says nothing about whether it holds a condition or is empty. The
		// row is read at a glance far more often than it is typed into, so what
		// the player put there is the part that has to stand out.
		row->stat->SetColor(Silver);

		row->comparator = new Combohook(chrome, 0, 0, QB_COMPARATOR_WIDTH,
			&row->comparatorIndex, comparators);
		// Its options are one character each, so an arrow saying it has a list
		// would be most of the box and would sit oddly beside a ">".
		row->comparator->SetArrow(false);

		row->value = new Inputhook(chrome, 0, 0, QB_VALUE_WIDTH, "");
		row->value->SetCompact(true);
		// An empty box means any amount, so a hint reading "0" would name a
		// comparison the row is not making.
		row->value->SetPlaceholder("Any");
		row->value->SetColor(Silver);

		row->remove = new Buttonhook(chrome, 0, 0, row->stat->GetYSize(), IconClose);
		row->remove->SetLeftCallback(RemoveClicked, this);

		rows.push_back(row);
	}

	addLine = new Texthook(chrome, 0, 0, "+ Add condition");
	addLine->SetColor(Gold);
	addLine->SetHoverColor(Tan);
	addLine->SetLeftCallback(AddClicked, this);

	// Last, so it is drawn over every row and over whichever panel is behind it.
	picker = new Listhook(chrome, 0, 0, 0, 0);
	std::vector<ListColumn> columns;
	columns.push_back(ListColumn("", 0, QB_PICKER_LABEL_WEIGHT, 0, White, Gold));
	picker->SetColumns(columns);
	// It hangs over whichever panel is behind it, so it brings its own panel
	// rather than leaving its rows on whatever they happen to cover.
	picker->SetFloating(true);
	// Ahead of the panel's own list, which it hangs over: a press is offered to
	// every hook, and the first to claim it holds the rest of the gesture.
	picker->SetZOrder(-1);

	ShowControls();
}

unsigned int QueryBuilder::RowHeight() {
	return rows[0]->stat->GetYSize();
}

unsigned int QueryBuilder::RowPitch() {
	return RowHeight() + QB_ROW_GAP;
}

unsigned int QueryBuilder::AddLineHeight() {
	return QB_ADD_LINE_TOP + addLine->GetYSize();
}

// What the window has to keep clear. Zero while the rows are put away, so a
// window whose conditions are hidden gives the room back to its list.
unsigned int QueryBuilder::GetHeight() {
	if (!shown || !panelOffers)
		return 0;
	unsigned int height = QB_BAND_TOP + (rowCount * RowPitch()) + QB_BAND_BOTTOM;
	if (rowCount < maxRows)
		height += AddLineHeight();
	return height;
}

void QueryBuilder::SetShown(bool show) {
	if (shown == show)
		return;
	shown = show;
	if (!show)
		ClosePicker();
	laidOutWidth = 0;	// so the next draw places whatever is now on screen
	ShowControls();
}

void QueryBuilder::SetOffered(bool offers) {
	if (panelOffers == offers)
		return;
	panelOffers = offers;
	if (!panelOffers)
		ClosePicker();
	ShowControls();
}

void QueryBuilder::SetKind(const std::string& newKind) {
	if (kind.compare(newKind) == 0)
		return;
	kind = newKind;
	ClosePicker();
	// The same stat is worded by the same tables whatever grants it, but one the
	// new kind never grants has no entry to be worded from and falls back to its
	// own name.
	for (unsigned int i = 0; i < rows.size(); i++)
		RestoreStatText(i);
}

// One row always, however little room there is: a builder with no rows is a
// builder that cannot be used, and an empty row narrows nothing anyway.
void QueryBuilder::SetHeightBudget(unsigned int pixels) {
	unsigned int spent = QB_BAND_TOP + QB_BAND_BOTTOM + AddLineHeight();
	unsigned int spare = (pixels > spent) ? (pixels - spent) : 0;
	unsigned int fits = spare / RowPitch();
	if (fits < 1)
		fits = 1;
	if (fits > MaxRows)
		fits = MaxRows;

	if (maxRows == fits)
		return;
	maxRows = fits;
	while (rowCount > maxRows)
		RemoveRow(rowCount - 1);
	laidOutWidth = 0;
	ShowControls();
}

// Which controls are on screen at all. A switched off hook is neither drawn nor
// clickable, so a row put away cannot be typed into by accident.
void QueryBuilder::ShowControls() {
	for (unsigned int i = 0; i < rows.size(); i++) {
		bool on = shown && panelOffers && (i < rowCount);
		rows[i]->stat->SetActive(on);
		rows[i]->comparator->SetActive(on);
		rows[i]->value->SetActive(on);
		rows[i]->remove->SetActive(on);
		if (!on) {
			rows[i]->stat->SetFocused(false);
			rows[i]->value->SetFocused(false);
		}
	}
	addLine->SetActive(shown && panelOffers && rowCount < maxRows);
	if (!shown || !panelOffers)
		picker->SetActive(false);
}

void QueryBuilder::ApplyLayout() {
	unsigned int width = ui->GetChrome()->GetXSize();
	unsigned int top = ui->GetSearchExtraY() - ui->GetY() + QB_BAND_TOP;
	laidOutWidth = width;
	laidOutY = top;

	unsigned int height = RowHeight();

	// Everything but the stat box is a fixed width, so the stat box is whatever
	// the row has left over. Clamped rather than allowed to wrap, a window
	// dragged narrow enough leaving nothing for it.
	unsigned int fixed = QB_COMPARATOR_WIDTH + QB_VALUE_WIDTH + height +
		(3 * QB_CELL_GAP);
	unsigned int statWidth = (width > fixed + 1) ? (width - fixed) : 1;

	for (unsigned int i = 0; i < rows.size(); i++) {
		unsigned int y = top + (i * RowPitch());
		unsigned int x = 0;

		rows[i]->stat->SetBaseX(x);
		rows[i]->stat->SetBaseY(y);
		rows[i]->stat->SetXSize(statWidth);
		x += statWidth + QB_CELL_GAP;

		rows[i]->comparator->SetBaseX(x);
		rows[i]->comparator->SetBaseY(y);
		x += QB_COMPARATOR_WIDTH + QB_CELL_GAP;

		rows[i]->value->SetBaseX(x);
		rows[i]->value->SetBaseY(y);
		x += QB_VALUE_WIDTH + QB_CELL_GAP;

		rows[i]->remove->SetBaseX(x);
		rows[i]->remove->SetBaseY(y);
		rows[i]->remove->SetSize(height);
	}

	addLine->SetBaseX(0);
	addLine->SetBaseY(top + (rowCount * RowPitch()) + QB_ADD_LINE_TOP);

	picker->SetXSize(statWidth);
}

void QueryBuilder::AddRow() {
	if (rowCount >= maxRows)
		return;
	// A fresh row rather than whatever the last one to hold this slot said.
	ClearRow(rowCount);
	rowCount++;
	laidOutWidth = 0;
	ShowControls();
}

// Back to a row that asks nothing, without taking it away.
void QueryBuilder::ClearRow(unsigned int index) {
	if (index >= rows.size())
		return;
	rows[index]->stat_.clear();
	rows[index]->stat->Clear();
	rows[index]->stat->SetFocused(false);
	rows[index]->value->Clear();
	rows[index]->value->SetFocused(false);
	rows[index]->comparatorIndex = 0;
}

// The rows below close the gap, so the one that was clicked is the one that
// goes rather than the one at the end.
//
// The only row is emptied rather than taken away. A builder has to keep a row to
// be usable at all, and a button that does nothing on the one row people most
// often want rid of would leave no way to call off a filter short of closing the
// window.
void QueryBuilder::RemoveRow(unsigned int index) {
	if (index >= rowCount)
		return;
	ClosePicker();

	if (rowCount == 1) {
		ClearRow(index);
		return;
	}

	for (unsigned int i = index; i + 1 < rowCount; i++) {
		rows[i]->stat_ = rows[i + 1]->stat_;
		rows[i]->comparatorIndex = rows[i + 1]->comparatorIndex;
		rows[i]->stat->SetText("%s", rows[i + 1]->stat->GetText().c_str());
		rows[i]->value->SetText("%s", rows[i + 1]->value->GetText().c_str());
	}
	rowCount--;
	ClearRow(rowCount);

	laidOutWidth = 0;
	ShowControls();
}

void QueryBuilder::OpenPicker(unsigned int index) {
	pickerRow = index;
	FillPicker();
}

// Filled from what is typed in the row's stat box, so the list narrows as the
// box does. Placed under the box each time, since how tall it is depends on how
// much is left to choose from.
void QueryBuilder::FillPicker() {
	if (pickerRow >= rowCount) {
		picker->SetActive(false);
		return;
	}

	Inputhook* box = rows[pickerRow]->stat;
	pickerText = box->GetText();
	offered = StatVocabulary::Matching(kind, pickerText);
	if (offered.empty()) {
		picker->SetActive(false);
		return;
	}

	std::vector<std::vector<std::string>> pickerRows;
	pickerRows.reserve(offered.size());
	for (unsigned int i = 0; i < offered.size(); i++) {
		std::vector<std::string> row;
		row.push_back(offered[i].label);
		pickerRows.push_back(row);
	}

	unsigned int visible = (unsigned int)offered.size();
	if (visible > QB_PICKER_ROWS)
		visible = QB_PICKER_ROWS;

	picker->SetSize(box->GetXSize(), visible * picker->GetRowHeight());
	picker->SetBaseX(box->GetBaseX());
	picker->SetBaseY(box->GetBaseY() + box->GetYSize() + QB_PICKER_DROP);
	picker->SetRows(pickerRows);
	picker->SetScrollTop(0);
	picker->SetActive(true);
}

void QueryBuilder::CommitPick(unsigned int offeredRow) {
	if (pickerRow >= rowCount || offeredRow >= offered.size())
		return;

	rows[pickerRow]->stat_ = offered[offeredRow].stat;
	rows[pickerRow]->stat->SetText("%s", offered[offeredRow].label.c_str());
	rows[pickerRow]->stat->SetTextPos(0);
	rows[pickerRow]->stat->SetFocused(false);
	ClosePicker();
}

// A press inside the picker takes the caret out of the stat box, so an unfocused
// box is not by itself a reason to shut the list: the click that shut it may be
// the click that was choosing from it. Held while the mouse is on it, and while
// its scrollbar is being dragged, which carries the mouse off it.
bool QueryBuilder::PickerHasMouse() {
	if (!picker->IsActive())
		return false;
	if (picker->IsScrolling())
		return true;
	return picker->InRange((unsigned int)Hook::GetMouseX(),
		(unsigned int)Hook::GetMouseY());
}

bool QueryBuilder::ClosePicker() {
	if (pickerRow >= MaxRows)
		return false;
	unsigned int was = pickerRow;
	pickerRow = MaxRows;
	picker->SetActive(false);
	RestoreStatText(was);
	return true;
}

// A box left holding a search rather than a stat reads as though it were
// filtering on what is typed in it, which it is not. It goes back to naming
// whatever the row actually names, and to empty where the row names nothing.
void QueryBuilder::RestoreStatText(unsigned int index) {
	if (index >= rows.size())
		return;
	Row* row = rows[index];
	if (row->stat_.empty()) {
		row->stat->Clear();
		return;
	}
	row->stat->SetText("%s", StatVocabulary::LabelFor(kind, row->stat_).c_str());
	row->stat->SetTextPos(0);
}

// A row with no amount in it asks for the stat and nothing more, which is the
// question "what grants this at all". The comparator is left showing rather than
// switched off: it says what the box beside it would mean, and a row being
// filled in should not have to be set up twice.
std::vector<StatIndex::Criterion> QueryBuilder::GetCriteria() {
	std::vector<StatIndex::Criterion> criteria;
	for (unsigned int i = 0; i < rowCount; i++) {
		if (rows[i]->stat_.empty())
			continue;

		int value = 0;
		if (!ParseValue(rows[i]->value->GetText(), value)) {
			criteria.push_back(StatIndex::Criterion::OnStat(rows[i]->stat_,
				StatIndex::Granted, 0));
			continue;
		}
		criteria.push_back(StatIndex::Criterion::OnStat(rows[i]->stat_,
			ComparatorAt(rows[i]->comparatorIndex), value));
	}
	return criteria;
}

std::string QueryBuilder::DescribeConditions() {
	if (activeCount == 0)
		return "";
	return std::to_string(activeCount) +
		((activeCount == 1) ? " condition" : " conditions");
}

// Everything a panel would have to ask again about, in one string. Comparing
// this is what notices a keystroke, a comparator, a row added and a row taken
// away without each of them having to report itself.
std::string QueryBuilder::Signature() {
	std::string out;
	for (unsigned int i = 0; i < rowCount; i++) {
		out += rows[i]->stat_;
		out += '|';
		out += std::to_string(rows[i]->comparatorIndex);
		out += '|';
		out += Trim(rows[i]->value->GetText());
		out += ';';
	}
	return out;
}

void QueryBuilder::Clear() {
	ClosePicker();
	rowCount = 1;
	for (unsigned int i = 0; i < rows.size(); i++)
		ClearRow(i);
	shown = false;
	laidOutWidth = 0;
	ShowControls();
}

void QueryBuilder::OnDraw() {
	if (laidOutWidth != ui->GetChrome()->GetXSize() ||
			laidOutY != (ui->GetSearchExtraY() - ui->GetY() + QB_BAND_TOP))
		ApplyLayout();

	if (shown && panelOffers) {
		// A pick arrives on the release, by which time the press has already
		// taken the caret out of the stat box. Taken before the caret is looked
		// at, so the row that was clicked is not thrown away with the picker.
		int picked = picker->TakeClickedRow();
		if (picked >= 0) {
			CommitPick((unsigned int)picked);
		} else {
			unsigned int focused = MaxRows;
			for (unsigned int i = 0; i < rowCount; i++) {
				if (rows[i]->stat->IsFocused())
					focused = i;
			}

			if (focused < rowCount && focused != pickerRow) {
				// The caret has arrived in a box, which is a stat being looked
				// for.
				ClosePicker();
				OpenPicker(focused);
			} else if (focused == pickerRow && pickerRow < rowCount) {
				// Only when what is typed has changed, so the list keeps where
				// it was scrolled to between keystrokes.
				if (pickerText != rows[pickerRow]->stat->GetText())
					FillPicker();
			} else if (pickerRow < rowCount && !PickerHasMouse()) {
				// The caret has gone elsewhere and the mouse is not on the
				// list, so no pick is coming. Whatever the box was left holding
				// goes back to the stat the row names.
				ClosePicker();
			}
		}
	}

	// Read after the picks have been taken, so a stat chosen this frame is part
	// of what the panels are told about this frame.
	activeCount = (unsigned int)GetCriteria().size();
	std::string now = Signature();
	if (now != signature) {
		signature = now;
		revision++;
	}
}

unsigned int QueryBuilder::RowOf(Hook* hook) {
	for (unsigned int i = 0; i < rows.size(); i++) {
		if (rows[i]->remove == hook)
			return i;
	}
	return MaxRows;
}

bool QueryBuilder::RemoveClicked(bool up, Hook* hook, void* context) {
	if (up && context) {
		QueryBuilder* builder = (QueryBuilder*)context;
		builder->RemoveRow(builder->RowOf(hook));
	}
	return true;
}

bool QueryBuilder::AddClicked(bool up, Hook* hook, void* context) {
	if (up && context)
		((QueryBuilder*)context)->AddRow();
	return true;
}
