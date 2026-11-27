#pragma once
#include <functional>
#include <string>
#include <vector>
#include "RunStore.h"
#include "SessionModel.h"

// What the session HUD says and where it goes, worked out without the game: the
// text of each view, how that text is set out as a block, and where on the
// canvas the block sits.
namespace HudLayout {

// The views, from least to most. The order is the order a setting counts them
// in and the order they are stepped through.
enum View {
	ViewLine,		// one line
	ViewCompact,	// a few lines of names and figures
	ViewSplits,		// the run in hand, split by split
	ViewFull,		// the splits, and the session under them
	ViewCount
};

// What the views are called, as a setting lists them and a command names them.
const char* NameOf(View view);

// The view a name means, whatever case it is typed in, or ViewCount for none.
View ViewNamed(const std::string& name);

View Next(View view, int step);

enum Tone {
	ToneLabel,		// names what follows
	ToneValue,
	ToneDim,		// a figure that is not this run's: the time to beat
	ToneAhead,		// of the time it is compared with
	ToneBehind,
	ToneBest		// the fastest this stretch has been done
};

struct Cell {
	std::string text;
	Tone tone;

	// Stands against the left of its column, as a name does, whatever its tone:
	// the split a run is on is written as a figure is, where it stood as a name.
	bool left = false;
};
typedef std::vector<Cell> Row;

// Rows that share their columns. A view is one or more of them, one under the
// other, each with columns of its own.
typedef std::vector<Row> Section;

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

	// The run in hand, or one finished a moment ago and still worth reading,
	// set beside the best its route has been run.
	bool inRun;
	bool runEnded;
	RunCompare::Lines run;
};

// How long a finished run stays on the HUD.
const unsigned long long kRunLingersMs = 60 * 1000;

// `best` is what the run in hand is compared with, or NULL for nothing.
Figures FiguresOf(const SessionModel& model, unsigned long long now,
	const RunStore::Best* best = NULL, RunStore::Clock clock = RunStore::RealTime);

// A view without a run to show is the nearest one that needs none.
std::vector<Section> Sections(View view, const Figures& figures);

struct Size {
	int width;
	int height;
};

struct Point {
	int x;
	int y;
};

// How text is measured and spaced. The width of a string is asked for rather
// than known, since only the game can say what its fonts come to.
struct Metrics {
	std::function<int(const std::string&)> width;
	int textHeight;
	int rowGap;			// between one row and the next
	int labelGap;		// between a name and its figure
	int groupGap;		// between a figure and the next name
	int sectionGap;		// between one section and the next, over the row gap
	int padding;		// around the whole block
};

struct Placed {
	Point at;			// within the block
	std::string text;
	Tone tone;
};

struct Block {
	Size size;
	std::vector<Placed> cells;
};

// Sets rows out as a block. Cells stand in columns as wide as their widest
// cell, names against the left of theirs and figures against the right, so
// figures in a column line up by their last digit.
Block Arrange(const std::vector<Row>& rows, const Metrics& metrics);

// The same for sections one under the other, each in its own columns.
Block ArrangeSections(const std::vector<Section>& sections, const Metrics& metrics);

// Which edge of the canvas a block is held against, along one axis.
enum Side {
	SideNear,		// left, or top
	SideMiddle,
	SideFar			// right, or bottom
};

// Where a block is pinned: an edge or the middle along each axis, and how far
// from it. A distance is from the canvas's edge to the block's same edge, or
// from middle to middle, so a block keeps its pinned edge whatever size it or
// the canvas becomes.
struct Place {
	Side across;
	Side down;
	int x;
	int y;
};

// The block's top left corner, kept wholly on the canvas.
Point Resolve(const Place& place, Size canvas, Size block);

// The place that puts a block where it is: pinned to whichever third of the
// canvas its middle is in, along each axis.
Place PlaceOf(Point topLeft, Size block, Size canvas);

// Whether a block lies under one of the game's own side panels. An open panel
// takes its half of the canvas, whatever size the canvas is: where the panel's
// own art stops short, the game fills the rest of the half in around it.
bool UnderPanel(Point topLeft, Size block, Size canvas, bool leftOpen, bool rightOpen);

}
