#pragma once
#include <string>
#include <vector>
#include "../Tracking/HudBlock.h"
#include "SessionModel.h"

// What the session HUD says, worked out without the game: the rows of each of
// its views.
namespace HudLayout {

// The views, from least to most. The order is the order a setting counts them
// in and the order they are stepped through.
enum View {
	ViewLine,		// one line
	ViewCompact,	// a few lines of names and figures
	ViewFull,		// those, and what was killed and what last dropped
	ViewCount
};

// What the views are called, as a setting lists them and a command names them.
const char* NameOf(View view);

// The view a name means, whatever case it is typed in, or ViewCount for none.
View ViewNamed(const std::string& name);

View Next(View view, int step);

// What a view shows, read off a session at a moment on its clock.
struct Figures {
	unsigned long long sessionMs;
	unsigned long long playedMs;
	unsigned long long gameMs;
	unsigned int games;
	double gameRate;
	double playedRate;
	double recentRate;
	unsigned int kills;
	unsigned int deaths;
	std::string lastDrop;
};

Figures FiguresOf(const SessionModel& model, unsigned long long now);

std::vector<HudBlock::Section> Sections(View view, const Figures& figures);

}
