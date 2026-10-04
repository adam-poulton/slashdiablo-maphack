#include "RoomFrontier.h"
#include <algorithm>
#include <map>
#include <utility>

namespace RoomFrontier {

namespace {

typedef std::pair<int, int> Span;
typedef std::vector<Span> Spans;

// The sides lying on one line, split by which way the room they belong to faces.
struct Line {
	Spans activeLow, activeHigh, inactiveLow, inactiveHigh;
};

// Sorted, with touching and overlapping spans joined.
Spans Merge(Spans spans) {
	std::sort(spans.begin(), spans.end());
	Spans merged;
	for (const Span& span : spans) {
		if (!merged.empty() && span.first <= merged.back().second)
			merged.back().second = std::max(merged.back().second, span.second);
		else
			merged.push_back(span);
	}
	return merged;
}

// Both merged. Points of contact are dropped.
Spans Intersect(const Spans& a, const Spans& b) {
	Spans overlap;
	size_t i = 0, j = 0;
	while (i < a.size() && j < b.size()) {
		int from = std::max(a[i].first, b[j].first);
		int to = std::min(a[i].second, b[j].second);
		if (from < to)
			overlap.push_back(Span(from, to));
		if (a[i].second < b[j].second)
			i++;
		else
			j++;
	}
	return overlap;
}

// A room's low side faces its high side's neighbour across the line, so an active
// low side meets an inactive high side and the other way round.
Spans Frontier(const Line& line) {
	Spans frontier = Intersect(Merge(line.activeLow), Merge(line.inactiveHigh));
	Spans other = Intersect(Merge(line.activeHigh), Merge(line.inactiveLow));
	frontier.insert(frontier.end(), other.begin(), other.end());
	return Merge(frontier);
}

}

std::vector<Edge> Trace(const std::vector<Rect>& active, const std::vector<Rect>& inactive) {
	// Keyed by the line's x for vertical sides and its y for horizontal ones.
	std::map<int, Line> vertical, horizontal;

	for (const Rect& room : active) {
		if (room.width <= 0 || room.height <= 0)
			continue;
		Span down(room.y, room.y + room.height), across(room.x, room.x + room.width);
		vertical[room.x].activeLow.push_back(down);
		vertical[room.x + room.width].activeHigh.push_back(down);
		horizontal[room.y].activeLow.push_back(across);
		horizontal[room.y + room.height].activeHigh.push_back(across);
	}
	for (const Rect& room : inactive) {
		if (room.width <= 0 || room.height <= 0)
			continue;
		Span down(room.y, room.y + room.height), across(room.x, room.x + room.width);
		vertical[room.x].inactiveLow.push_back(down);
		vertical[room.x + room.width].inactiveHigh.push_back(down);
		horizontal[room.y].inactiveLow.push_back(across);
		horizontal[room.y + room.height].inactiveHigh.push_back(across);
	}

	std::vector<Edge> edges;
	for (const auto& line : vertical) {
		for (const Span& span : Frontier(line.second))
			edges.push_back(Edge{ line.first, span.first, line.first, span.second });
	}
	for (const auto& line : horizontal) {
		for (const Span& span : Frontier(line.second))
			edges.push_back(Edge{ span.first, line.first, span.second, line.first });
	}
	return edges;
}

}
