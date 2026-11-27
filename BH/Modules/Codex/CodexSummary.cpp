#include "CodexSummary.h"

using namespace Drawing;

// The least a list is left with before the pane is docked beside it, which is a
// runeword's name and the longest run of runes; and the pane's share of the
// panel past that, between the narrowest a stat line reads at without wrapping
// most of them and the widest the game's own description of an item gets.
#define SUMMARY_LIST_MIN		372
#define SUMMARY_PANE_PERCENT	45
#define SUMMARY_PANE_MIN		250
#define SUMMARY_PANE_MAX		400

// Between the list and the rule, between the rule and the text, and above the
// first line.
#define SUMMARY_GAP				6
#define SUMMARY_PAD				6
#define SUMMARY_TOP				4

// The pitch a floating description's lines are drawn at, so the two read alike.
#define SUMMARY_LINE_HEIGHT		12
#define SUMMARY_FONT			0

#define SUMMARY_HINT_TEXT		"Point at a row to read it here"
#define SUMMARY_HINT_TOP		12
#define SUMMARY_HINT_COLOR		Grey

CodexSummaryHint::CodexSummaryHint(HookGroup* group) : Hook(group, 0, 0), xSize(0) {
}

void CodexSummaryHint::SetXSize(unsigned int width) {
	Lock();
	xSize = width;
	Unlock();
}

unsigned int CodexSummaryHint::GetYSize() {
	return SUMMARY_LINE_HEIGHT;
}

void CodexSummaryHint::OnDraw() {
	if (!IsActive())
		return;
	Texthook::Draw(GetX() + xSize / 2, GetY(), Center, SUMMARY_FONT, SUMMARY_HINT_COLOR,
		SUMMARY_HINT_TEXT);
}

CodexSummary::CodexSummary(UITab* tab) : tab(tab), docked(false), shown(false) {
	// Placed and switched on by Show().
	floating = new Tooltiphook(InGame, 0, 0);
	floating->SetActive(false);

	rule = new Boxhook(tab, 0, 0, 1, 0);
	rule->SetColor(UI_SURFACE_RULE);
	rule->SetTransparency(BTNormal);
	rule->SetActive(false);

	pane = new Scrollhook(tab, 0, 0, 0, 0);
	pane->SetActive(false);

	hint = new CodexSummaryHint(tab);
	hint->SetActive(false);
}

unsigned int CodexSummary::Layout(unsigned int contentWidth, unsigned int height) {
	bool wasDocked = docked;
	unsigned int paneWidth = pane->GetXSize();

	docked = contentWidth >= SUMMARY_LIST_MIN + SUMMARY_GAP + SUMMARY_PANE_MIN;
	rule->SetActive(docked);
	pane->SetActive(docked);
	if (!docked) {
		hint->SetActive(false);
		floating->SetMaxWidth(contentWidth);
		if (wasDocked && shown)
			floating->SetLines(lines);
		return contentWidth;
	}

	unsigned int share = (contentWidth * SUMMARY_PANE_PERCENT) / 100;
	if (share < SUMMARY_PANE_MIN)
		share = SUMMARY_PANE_MIN;
	if (share > SUMMARY_PANE_MAX)
		share = SUMMARY_PANE_MAX;
	unsigned int listWidth = contentWidth - SUMMARY_GAP - share;

	unsigned int ruleX = UI_CONTENT_MARGIN + listWidth + SUMMARY_GAP;
	rule->SetBaseX(ruleX);
	rule->SetBaseY(0);
	rule->SetYSize(height);

	unsigned int textX = ruleX + 1 + SUMMARY_PAD;
	unsigned int textWidth = share - 1 - SUMMARY_PAD;
	pane->SetBaseX(textX);
	pane->SetBaseY(SUMMARY_TOP);
	pane->SetSize(textWidth, height > SUMMARY_TOP ? height - SUMMARY_TOP : 0);

	hint->SetBaseX(textX);
	hint->SetBaseY(SUMMARY_HINT_TOP);
	hint->SetXSize(pane->GetContentWidth());
	hint->SetActive(!shown);

	floating->SetActive(false);
	if (shown && (!wasDocked || textWidth != paneWidth))
		FillPane();
	return listWidth;
}

// One row of the box to each line of text, and one hook to each line that says
// anything, centred as the game centres an item's description.
void CodexSummary::FillPane() {
	pane->ClearContents();
	unsigned int width = pane->GetContentWidth();
	for (const TooltipLine& line : lines) {
		if (line.text.empty()) {
			pane->AddRow(SUMMARY_LINE_HEIGHT);
			continue;
		}
		for (const std::string& piece :
				Texthook::WrapBalanced(line.text, width, SUMMARY_FONT)) {
			unsigned int row = pane->AddRow(SUMMARY_LINE_HEIGHT);
			unsigned int pieceWidth =
				(unsigned int)Texthook::GetTextSize(piece, SUMMARY_FONT).x;
			Texthook* text = new Texthook(pane->GetContent(),
				width > pieceWidth ? (width - pieceWidth) / 2 : 0,
				pane->GetRowY(row), "%s", piece.c_str());
			text->SetColor(line.color);
			pane->AddToRow(row, text);
		}
	}
}

void CodexSummary::SetLines(const std::vector<TooltipLine>& newLines) {
	lines = newLines;
	if (docked)
		FillPane();
	else
		floating->SetLines(lines);
}

void CodexSummary::Show() {
	shown = true;
	if (docked) {
		hint->SetActive(false);
		return;
	}
	// Where it fits depends on how big the lines made it.
	floating->PlaceBeside(tab->GetX(), tab->GetY(), tab->GetXSize(), tab->GetYSize());
	floating->SetActive(true);
}

// Anywhere past the list's right edge, the gap before the rule included, so the
// pointer does not let go of the pane on its way across to it.
bool CodexSummary::OverPane() {
	unsigned int x = Hook::GetMouseX(), y = Hook::GetMouseY();
	unsigned int left = rule->GetX() > SUMMARY_GAP ? rule->GetX() - SUMMARY_GAP : 0;
	return x >= left && x < pane->GetX() + pane->GetXSize() &&
		y >= rule->GetY() && y < rule->GetY() + rule->GetYSize();
}

// A description in the pane stays while the pointer is on the pane, although
// that takes it off the row that raised it: reaching the pane is how one too
// long for it is scrolled.
void CodexSummary::Hide() {
	floating->SetActive(false);
	if (shown && docked && tab->IsActive() && OverPane())
		return;
	if (shown && docked)
		pane->ClearContents();
	shown = false;
	hint->SetActive(docked);
}
