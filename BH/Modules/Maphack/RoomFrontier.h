#pragma once
#include <vector>

// Where the active rooms meet the rooms around them.
//
// The active rooms are the ones whose units the game is sending, so this is the
// line past which nothing is drawn on the automap. Edges against the void around
// a level are not part of it: there is nothing there to send.
namespace RoomFrontier {

// A room's extent, in subtiles.
struct Rect {
	int x, y, width, height;
};

// A straight run of frontier, in subtiles. Always axis-aligned, and always
// running from the lesser coordinate to the greater.
struct Edge {
	int x1, y1, x2, y2;

	bool operator==(const Edge& other) const {
		return x1 == other.x1 && y1 == other.y1 && x2 == other.x2 && y2 == other.y2;
	}
};

// Rooms never overlap, so an active room's side is frontier exactly where an
// inactive room's opposite side lies against it. Collinear runs are joined into
// one edge, and rooms meeting only at a corner give none.
std::vector<Edge> Trace(const std::vector<Rect>& active, const std::vector<Rect>& inactive);

}
