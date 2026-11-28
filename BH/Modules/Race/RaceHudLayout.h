#pragma once
#include <string>
#include <vector>
#include "../Tracking/HudBlock.h"
#include "RunModel.h"
#include "RunStore.h"

// What the race HUD says, worked out without the game: the rows of each of its
// views. It has something to say only of a run: one in hand, or one finished a
// moment ago and still worth reading.
namespace RaceHudLayout {

// The views, from least to most. The order is the order a setting counts them
// in and the order they are stepped through.
enum View {
	ViewLine,		// the split the run is on, its clock, and how it stands
	ViewSplits,		// the run split by split
	ViewCount
};

// What the views are called, as a setting lists them and a command names them.
const char* NameOf(View view);

// The view a name means, whatever case it is typed in, or ViewCount for none.
View ViewNamed(const std::string& name);

View Next(View view, int step);

// A run set beside the best its route has been run.
struct Figures {
	bool inRun;				// there is a run to show
	bool runEnded;
	std::string route;		// what its route is called
	RunCompare::Lines run;
};

// How long a finished run stays on the HUD.
const unsigned long long kRunLingersMs = 60 * 1000;

// `best` is what the run is compared with, or NULL for nothing.
Figures FiguresOf(const RunModel& model, unsigned long long now,
	const RunStore::Best* best = NULL, RunStore::Clock clock = RunStore::RealTime);

// A run as it stands at a moment on its clock, whatever state it is in: what a
// route looks like before it is run, for a HUD shown with no run going.
Figures FiguresOfRun(const RunModel::Run& run, unsigned long long clockNow,
	const RunStore::Best* best, RunStore::Clock clock);

// Nothing where there is no run to show.
std::vector<HudBlock::Section> Sections(View view, const Figures& figures);

}
