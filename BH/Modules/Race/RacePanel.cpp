#include "RacePanel.h"
#include "EditPage.h"
#include "RunPage.h"

using namespace Drawing;

RacePanel::RacePanel(UI* ui) : UIPanel("Race", ui), editing(false) {
	run = new RunPage(tab);
	edit = new EditPage(tab);
	edit->SetShown(false);
}

RacePanel::~RacePanel() {
	delete run;
	delete edit;
}

void RacePanel::ShowRun() {
	if (!editing)
		return;
	edit->Leave();
	edit->SetShown(false);
	run->SetShown(true);
	editing = false;
}

void RacePanel::OnDraw() {
	if (!IsActive()) {
		// A caret left in a box that is not drawn would go on taking the keys.
		if (editing)
			edit->Leave();
		return;
	}

	const unsigned int width = tab->GetXSize();
	const unsigned int height = tab->GetYSize();
	if (editing) {
		edit->Draw(width, height, run->GetRoutesWidth());
		if (!edit->TakeDone())
			return;
		ShowRun();
	}

	run->Draw(width, height);
	std::string id;
	bool rename = false;
	if (run->TakeEdit(id, rename)) {
		run->SetShown(false);
		edit->SetShown(true);
		edit->Begin(id, rename);
		editing = true;
		edit->Draw(width, height, run->GetRoutesWidth());
	}
}

// A window opened again is opened on its routes and the run, not on whichever
// route was last being changed.
void RacePanel::OnClose() {
	ShowRun();
}

bool RacePanel::OnKey(bool up, BYTE key) {
	return editing && edit->OnKey(up, key);
}

std::string RacePanel::GetStatus() {
	return editing ? edit->GetStatus() : run->GetStatus();
}

unsigned int RacePanel::GetMinWidth() {
	return RunPage::MinWidth();
}

unsigned int RacePanel::GetMinHeight() {
	return RunPage::MinHeight();
}
