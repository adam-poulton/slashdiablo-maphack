#include "AccountPanel.h"
#include "../../StringUtil.h"

using namespace Drawing;

#define AP_LIST_TOP_GAP		5	// panel top to the first row

// The band below the list. Its three rows are always the same height, whichever
// of the two things the band is showing, so the list never moves under the
// cursor as the panel changes what it offers.
#define AP_RULE_GAP_ABOVE	6	// list to the line
#define AP_RULE_GAP_BELOW	5	// the line to the first row
#define AP_ROW_GAP			4	// between the band's own rows
#define AP_BAND_BOTTOM_PAD	5	// last row to the foot of the panel
#define AP_ACTION_GAP		14	// between two things to click on one row
#define AP_IN_USE_GAP		4	// account name to what is said after it

// What the label box is built at, before there is a panel to ask how wide it is.
// ApplyLayout gives it the real width. Wider than the padding it subtracts, so
// GetCharacterLimit() cannot underflow if anything draws first.
#define AP_ROSTER_BOX_SEED	96

namespace {

// A hook that draws nothing and is here only to hear the keyboard.
//
// A panel's keys normally reach it through its window, which is driven by a
// module event raised only inside a game. A hook is offered keys wherever it is
// drawn, so on the login screen this is the only way a panel hears one at all.
//
// It answers only while an account is in hand, which leaves both keys to the
// game the rest of the time: enter on the login screen signs in, and that must
// go on working while the panel is merely being read.
class EditKeyListener : public Hook {
	private:
		AccountPanel* panel;

	public:
		EditKeyListener(HookGroup* group, AccountPanel* panel) :
			Hook(group, 0, 0), panel(panel) {};

		unsigned int GetXSize() { return 0; };
		unsigned int GetYSize() { return 0; };
		void OnDraw() {};

		bool OnKey(bool up, BYTE key, LPARAM lParam) {
			if (up || !panel->IsEditing())
				return false;

			// Enter finishes and keeps, escape finishes and does not. Enter is
			// heard here as well as by the box, because the box only hears it
			// while it holds the caret and an edit can be finished without ever
			// having clicked into it.
			if (key == VK_RETURN) {
				panel->RequestCommit();
				return true;
			}
			if (key == VK_ESCAPE) {
				panel->StopEditing();
				return true;
			}
			return false;
		};
};

}  // namespace

AccountPanel::AccountPanel(UI* ui, AccountActions* actions) :
		UIPanel("Accounts", ui),
		actions(actions),
		forgetAsked(false),
		laidOut(false),
		needsRefresh(true),
		foldOnPush(true),
		commitRequested(false),
		drawnForgetAsked(false),
		drawnFavourite(false),
		drawnPasswordLength(0),
		drawnCount(0),
		laidOutWidth(0),
		laidOutHeight(0) {

	// Built here and measured nowhere: every control is created without asking
	// the game how wide anything is. The columns and the layout wait for the
	// first draw, which is on the thread allowed to ask.
	list = new Listhook(tab, UI_CONTENT_MARGIN, 0, 0, 0);

	// The line between the list and the band. A hook rather than something the
	// panel draws for itself, because a panel draws before its window paints its
	// frame and anything drawn there is painted over.
	rule = new Boxhook(tab, UI_CONTENT_MARGIN, 0, 0, 1);
	rule->SetColor(Grey);
	rule->SetTransparency(BTNormal);

	// Not gold, which is for what can be clicked. This only names the account
	// the actions below it act on.
	inHandLabel = new Texthook(tab, UI_CONTENT_MARGIN, 0, "");
	inHandLabel->SetColor(Silver);
	inHandLabel->SetActive(false);

	doneAction = new Texthook(tab, UI_CONTENT_MARGIN, 0, "Done");
	doneAction->SetAlignment(Right);
	doneAction->SetColor(Gold);
	doneAction->SetHoverColor(Tan);
	doneAction->SetLeftCallback(OnDoneClicked, this);
	doneAction->SetActive(false);

	rosterBox = new Inputhook(tab, UI_CONTENT_MARGIN, 0, AP_ROSTER_BOX_SEED, "");
	// Label, not roster, which is the word for it everywhere the player reads it.
	rosterBox->SetPlaceholder("Label");
	rosterBox->SetSelectOnFocus(true);
	rosterBox->SetCompact(true);
	rosterBox->SetActive(false);

	favouriteAction = new Texthook(tab, UI_CONTENT_MARGIN, 0, "");
	favouriteAction->SetColor(Gold);
	favouriteAction->SetHoverColor(Tan);
	favouriteAction->SetLeftCallback(OnFavouriteClicked, this);
	favouriteAction->SetActive(false);

	forgetAction = new Texthook(tab, 0, 0, "");
	forgetAction->SetColor(Gold);
	forgetAction->SetHoverColor(Tan);
	forgetAction->SetLeftCallback(OnForgetClicked, this);
	forgetAction->SetActive(false);

	hintLabel = new Texthook(tab, UI_CONTENT_MARGIN, 0, "Right click an account to edit");
	hintLabel->SetColor(Grey);
	hintLabel->SetActive(false);

	captureLabel = new Texthook(tab, UI_CONTENT_MARGIN, 0, "");
	captureLabel->SetColor(Grey);
	captureLabel->SetActive(false);

	keepAction = new Texthook(tab, UI_CONTENT_MARGIN, 0, "");
	keepAction->SetColor(Gold);
	keepAction->SetHoverColor(Tan);
	keepAction->SetLeftCallback(OnKeepClicked, this);
	keepAction->SetActive(false);

	// Owned by the tab like every other hook here, so it goes when the panel does.
	new EditKeyListener(tab, this);
}

// The columns, said once, on the first frame drawn. Setting them measures text.
void AccountPanel::ApplyColumns() {
	std::vector<ListColumn> columns;
	// The name, and after it whether the account is signed in somewhere else.
	// The second column flows from the end of the first rather than standing in
	// a column of its own: a column at a fixed offset is either wasted width on
	// every row that has nothing to say or off the end of a narrow panel, and
	// this panel is narrow. Both lift under the mouse, the whole row being one
	// thing to click, and each one rung brighter so the two stay told apart and
	// the lift reads as one gesture.
	//
	// The panel spends four colours and each means one thing: gold is something
	// to click, white is the mouse, silver is an account name at rest, and grey
	// is text there is nothing to do with.
	columns.push_back(ListColumn("", 0, 1, 0, Silver, White));
	columns.push_back(ListColumn("", 0, 0, AP_IN_USE_GAP, Grey, Silver, true));
	list->SetColumns(columns);
}

// The one place anything is sized or positioned. The band is laid out from the
// bottom of the tab upwards, and the list is given what is left.
void AccountPanel::ApplyLayout() {
	laidOutWidth = tab->GetXSize();
	laidOutHeight = tab->GetYSize();

	unsigned int contentWidth = (laidOutWidth > 2 * UI_CONTENT_MARGIN) ?
		(laidOutWidth - (2 * UI_CONTENT_MARGIN)) : 0;

	unsigned int textHeight = Texthook::GetTextSize("A", hintLabel->GetFont()).y;
	unsigned int boxHeight = rosterBox->GetYSize();

	// The last row clears the bottom of the tab, which is where the window draws
	// the line above its footer.
	unsigned int bandHeight = AP_RULE_GAP_ABOVE + 1 + AP_RULE_GAP_BELOW +
		boxHeight + AP_ROW_GAP + textHeight + AP_ROW_GAP + textHeight +
		AP_BAND_BOTTOM_PAD;
	unsigned int spent = AP_LIST_TOP_GAP + bandHeight;
	unsigned int listHeight = (laidOutHeight > spent) ? (laidOutHeight - spent) : 0;

	list->SetBaseY(AP_LIST_TOP_GAP);
	list->SetSize(contentWidth, listHeight);

	unsigned int ruleY = AP_LIST_TOP_GAP + listHeight + AP_RULE_GAP_ABOVE;
	rule->SetBaseY(ruleY);
	rule->SetXSize(contentWidth);

	// The box has this row to itself and takes all of it: the label cannot be
	// scrolled, so width is the only thing deciding how much of it can be read.
	// The text standing in its place with nothing in hand is centred on the
	// box's line, both states of the band being the same height.
	unsigned int firstRow = ruleY + 1 + AP_RULE_GAP_BELOW;
	unsigned int centred = firstRow + ((boxHeight > textHeight) ?
		((boxHeight - textHeight) / 2) : 0);
	rosterBox->SetBaseY(firstRow);
	rosterBox->SetXSize(contentWidth);
	hintLabel->SetBaseY(centred);

	// The account the band is about, from hand or from the game's own boxes.
	unsigned int secondRow = firstRow + boxHeight + AP_ROW_GAP;
	inHandLabel->SetBaseY(secondRow);
	captureLabel->SetBaseY(secondRow);

	// Everything clickable on one row, so no action stands on its own.
	unsigned int thirdRow = secondRow + textHeight + AP_ROW_GAP;
	favouriteAction->SetBaseY(thirdRow);
	forgetAction->SetBaseY(thirdRow);
	doneAction->SetBaseY(thirdRow);
	keepAction->SetBaseY(thirdRow);
}

const AccountRow* AccountPanel::RowAt(int row) {
	if (row < 0 || row >= (int)rows.size())
		return NULL;
	if (rows[row].heading)
		return NULL;
	return &rows[row];
}

void AccountPanel::PushRows() {
	rows = BuildAccountRows(actions->Accounts(),
		[this](const std::string& name) { return actions->InUse(name); });

	std::vector<ListRow> listRows;
	listRows.reserve(rows.size());
	for (unsigned int i = 0; i < rows.size(); i++) {
		std::vector<std::string> cells;
		cells.push_back(rows[i].label);
		// A claim is advice rather than a refusal, so it is said after the name
		// rather than drawn as though the row were switched off.
		cells.push_back(rows[i].inUse ? "(in use)" : "");
		listRows.push_back(ListRow(cells, rows[i].heading));
	}
	list->SetRows(listRows);

	// Whatever was in hand may have just been forgotten, here or in another
	// client, leaving the band nothing to act on.
	if (!inHand.empty() && actions->Accounts().Find(inHand) == NULL)
		TakeInHand(std::string());

	FoldGroups();
	RevealInHand();
}

// Once, on the first push with anything in it. Rows are pushed again for every
// mark, label and account kept, so refolding each time would shut a group under
// the player as they worked in it.
//
// A first push with no headings spends the fold anyway, which is what leaves a
// label made later arriving open.
void AccountPanel::FoldGroups() {
	if (!foldOnPush || rows.empty())
		return;
	foldOnPush = false;

	for (unsigned int i = 0; i < rows.size(); i++) {
		if (rows[i].heading && !rows[i].favourites)
			list->SetGroupFolded((int)i, true);
	}
}

// A no-op after a right click, that row having been on screen to click. It is
// for the times the panel took an account in hand itself: one just kept, or one
// whose favourite mark just moved it into a folded label.
void AccountPanel::RevealInHand() {
	if (inHand.empty())
		return;

	for (unsigned int i = 0; i < rows.size(); i++) {
		if (rows[i].heading || rows[i].account != inHand)
			continue;
		int group = list->GetGroupRowFor((int)i);
		if (group >= 0)
			list->SetGroupFolded(group, false);
		return;
	}
}

// Keeps whatever label was typed and lets the account go. The label is only
// written where it differs from the one already kept, so finishing without
// having changed anything writes nothing and costs no reread.
void AccountPanel::CommitAndClose() {
	const Account* held = actions->Accounts().Find(inHand);
	if (held) {
		std::string typed = Trim(rosterBox->GetText());
		if (typed != Trim(held->roster) && actions->SetRoster(inHand, typed))
			Refresh();
	}
	TakeInHand(std::string());
}

void AccountPanel::TakeInHand(const std::string& accountName) {
	inHand = accountName;
	forgetAsked = false;
	rosterBox->SetFocused(false);

	const Account* held = inHand.empty() ? NULL : actions->Accounts().Find(inHand);
	rosterBox->SetText("%s", held ? held->roster.c_str() : "");
}

// Rebuilt only when something it says has changed, since it is asked every frame
// and setting text measures it.
void AccountPanel::UpdateBand() {
	const Account* held = inHand.empty() ? NULL : actions->Accounts().Find(inHand);
	bool editing = (held != NULL);

	std::string typed = Trim(actions->TypedAccountName());
	unsigned int passwordLength = actions->TypedPasswordLength();

	bool favourite = (held != NULL && held->favourite);

	unsigned int kept = actions->Accounts().Count();
	bool bandChanged = (inHand != drawnInHand || forgetAsked != drawnForgetAsked ||
		favourite != drawnFavourite);
	bool captureChanged = (typed != drawnTyped || passwordLength != drawnPasswordLength);
	if (!bandChanged && !captureChanged && kept == drawnCount)
		return;

	drawnInHand = inHand;
	drawnForgetAsked = forgetAsked;
	drawnFavourite = favourite;
	drawnTyped = typed;
	drawnPasswordLength = passwordLength;
	drawnCount = kept;

	inHandLabel->SetActive(editing);
	doneAction->SetActive(editing);
	rosterBox->SetActive(editing);
	favouriteAction->SetActive(editing);
	forgetAction->SetActive(editing);

	// Nothing to right click while nothing is kept, so the row that says to is
	// left out until there is.
	hintLabel->SetActive(!editing && kept > 0);
	captureLabel->SetActive(!editing);
	keepAction->SetActive(false);

	if (editing) {
		inHandLabel->SetText("%s", held->name.c_str());
		favouriteAction->SetText(favourite ? "Unfavourite" : "Favourite");
		forgetAction->SetText(forgetAsked ? "Forget?" : "Forget");
		forgetAction->SetColor(forgetAsked ? Red : Gold);
		// Placed against what the favourite line says, which changes with it.
		forgetAction->SetBaseX(UI_CONTENT_MARGIN + favouriteAction->GetXSize() +
			AP_ACTION_GAP);
		return;
	}

	// Nothing in hand, so the band says what the game's own boxes are holding and
	// what can be done with it. A password is shown as one star per character:
	// enough to see that it is being read, and nothing more than that.
	if (typed.empty()) {
		captureLabel->SetColor(Grey);
		captureLabel->SetText("Type an account and password below");
		return;
	}

	// The offer is always there to be read, and only clickable once there is
	// something worth writing. An account with no password signs nobody in, so
	// offering to keep one would be offering to keep nothing.
	keepAction->SetActive(true);
	keepAction->SetEnabled(passwordLength > 0);

	if (passwordLength == 0) {
		captureLabel->SetColor(Grey);
		captureLabel->SetText("%s", typed.c_str());
		keepAction->SetText("Enter a password to save");
		return;
	}

	std::string stars(passwordLength, '*');
	captureLabel->SetColor(Silver);
	captureLabel->SetText("%s  %s", typed.c_str(), stars.c_str());
	const Account* known = actions->Accounts().Find(typed);
	keepAction->SetText(known ? "Save new password" : "Save this account");
}

void AccountPanel::OnDraw() {
	if (!laidOut) {
		laidOut = true;
		ApplyColumns();
		ApplyLayout();
	} else if (laidOutWidth != tab->GetXSize() || laidOutHeight != tab->GetYSize()) {
		ApplyLayout();
	}

	if (needsRefresh) {
		needsRefresh = false;
		PushRows();
	}

	// A left click signs in as the account it landed on, and is the whole of
	// what a left click on a row ever does.
	int clicked = list->TakeClickedRow();
	const AccountRow* row = RowAt(clicked);
	if (row) {
		// Kept selected, so clicking the same row twice signs in twice rather
		// than letting go of the highlight on the second click.
		list->SetSelectedRow(clicked);
		const Account* account = actions->Accounts().Find(row->account);
		if (account)
			actions->SignIn(*account);
	}

	int held = list->TakeRightClickedRow();
	const AccountRow* heldRow = RowAt(held);
	if (heldRow) {
		TakeInHand(heldRow->account);
	} else if (held >= 0) {
		// A heading holds no account, so the right button there lets go of
		// whatever was in hand rather than doing nothing at all.
		TakeInHand(std::string());
	}

	// Enter finishes editing, from the box or from anywhere else in the panel,
	// and Done is the same gesture with the mouse. Both are taken here so that
	// one of them cannot come to mean something the other does not.
	bool submitted = rosterBox->TakeSubmitted();
	if ((submitted || commitRequested) && !inHand.empty())
		CommitAndClose();
	commitRequested = false;

	UpdateBand();
}

void AccountPanel::OnOpen() {
	Refresh();
	TakeInHand(std::string());
}

void AccountPanel::OnClose() {
	TakeInHand(std::string());
	list->ClearSelection();
}

std::string AccountPanel::GetStatus() {
	unsigned int kept = actions->Accounts().Count();
	if (kept == 0)
		return "Nothing kept yet";

	unsigned int inUse = 0;
	for (unsigned int i = 0; i < rows.size(); i++) {
		if (!rows[i].heading && rows[i].inUse)
			inUse++;
	}

	char said[64];
	if (inUse > 0)
		sprintf_s(said, "%u accounts, %u in use", kept, inUse);
	else
		sprintf_s(said, "%u accounts", kept);
	return std::string(said);
}

bool AccountPanel::OnFavouriteClicked(bool up, Hook* hook, void* self) {
	if (!up)
		return true;
	AccountPanel* panel = (AccountPanel*)self;
	if (panel->inHand.empty())
		return true;
	const Account* held = panel->actions->Accounts().Find(panel->inHand);
	if (held && panel->actions->SetFavourite(panel->inHand, !held->favourite))
		panel->Refresh();
	panel->forgetAsked = false;
	return true;
}

// Asked once and done on the second click, with the question where the player is
// already looking rather than in a box over the middle of the screen.
bool AccountPanel::OnForgetClicked(bool up, Hook* hook, void* self) {
	if (!up)
		return true;
	AccountPanel* panel = (AccountPanel*)self;
	if (panel->inHand.empty())
		return true;

	if (!panel->forgetAsked) {
		panel->forgetAsked = true;
		return true;
	}

	std::string forgetting = panel->inHand;
	panel->forgetAsked = false;
	if (panel->actions->Forget(forgetting)) {
		panel->TakeInHand(std::string());
		panel->Refresh();
	}
	return true;
}

bool AccountPanel::OnKeepClicked(bool up, Hook* hook, void* self) {
	if (!up)
		return true;
	AccountPanel* panel = (AccountPanel*)self;
	if (panel->actions->SaveTyped()) {
		panel->Refresh();
		// Taken in hand, a name just kept being the one a roster is about to be
		// typed for.
		panel->TakeInHand(Trim(panel->actions->TypedAccountName()));
	}
	return true;
}

// The same as pressing enter, so that the mouse and the keyboard finish an edit
// the same way.
bool AccountPanel::OnDoneClicked(bool up, Hook* hook, void* self) {
	if (!up)
		return true;
	AccountPanel* panel = (AccountPanel*)self;
	if (panel->IsEditing())
		panel->CommitAndClose();
	return true;
}
