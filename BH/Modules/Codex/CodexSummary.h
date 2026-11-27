#pragma once
#include <vector>
#include "../../Drawing.h"

// What the docked pane says while there is no row in hand, centred across the
// width it is given. A hook of its own rather than a Texthook placed by its
// width, so the text is only ever measured as it is drawn: the pane is laid out
// as the panel is built, which is before the game has a font to measure with.
class CodexSummaryHint : public Drawing::Hook {
	private:
		unsigned int xSize;
	public:
		CodexSummaryHint(Drawing::HookGroup* group);

		unsigned int GetXSize() { return xSize; };
		void SetXSize(unsigned int width);
		unsigned int GetYSize();
		void OnDraw();
};

// What the row in hand says about itself, for every panel of the codex: the
// description the game would give the item, as lines of text.
//
// Where the panel is wide enough the description is docked in a pane of its own
// down the panel's right side, and the list gives up that much width to it. The
// pane is always there, so nothing the description says ever covers the list it
// was raised from, and one too long for the pane scrolls. In a narrower panel
// there is no room to spare, and the description floats beside the window as a
// tooltip instead.
//
// A panel hands over the lines when the row changes and says each frame whether
// there is a row to describe; which of the two ways it is shown is not the
// panel's business.
class CodexSummary {
	private:
		Drawing::UITab* tab;
		Drawing::Tooltiphook* floating;

		// The docked pane: a rule down its left side, the description in a box
		// that scrolls, and what the pane says while there is no row in hand.
		Drawing::Boxhook* rule;
		Drawing::Scrollhook* pane;
		CodexSummaryHint* hint;

		std::vector<Drawing::TooltipLine> lines;
		bool docked;
		bool shown;

		// Fills the pane with the lines, wrapped to the width it has now.
		void FillPane();

		// Whether the pointer is on the docked pane.
		bool OverPane();

	public:
		CodexSummary(Drawing::UITab* tab);

		// Takes its share of a panel's content, which is the width and height
		// given, and returns the width that leaves for the list beside it.
		unsigned int Layout(unsigned int contentWidth, unsigned int height);

		// What to say about the row in hand. Measures text, so call it from the
		// draw thread.
		void SetLines(const std::vector<Drawing::TooltipLine>& newLines);

		// Called every frame: Show() while there is a row to describe, after
		// SetLines() where the row has changed, and Hide() while there is none.
		void Show();
		void Hide();
};
