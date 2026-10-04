#include "doctest.h"
#include "Modules/Maphack/RoomFrontier.h"

/*
 * Where the active rooms end.
 *
 * The frontier is drawn on the automap as the line past which the game sends no
 * units, so what matters is that it lies only between an active room and an
 * inactive one: never between two active rooms, never against the void around a
 * level, and in one piece where the rooms along it happen to be split.
 */

using RoomFrontier::Edge;
using RoomFrontier::Rect;
using RoomFrontier::Trace;

// Rooms on a grid of forty subtiles, the size of an outdoor room.
static Rect Room(int column, int row) {
	return Rect{ column * 40, row * 40, 40, 40 };
}

TEST_CASE("an active room with nothing around it has no frontier") {
	CHECK(Trace({ Room(0, 0) }, {}).empty());
}

TEST_CASE("the frontier is the side an active room shares with an inactive one") {
	std::vector<Edge> edges = Trace({ Room(0, 0) }, { Room(1, 0) });

	REQUIRE(edges.size() == 1);
	CHECK(edges[0] == Edge{ 40, 0, 40, 40 });
}

TEST_CASE("each side of an active room faces its own neighbour") {
	std::vector<Edge> edges = Trace({ Room(1, 1) },
		{ Room(0, 1), Room(2, 1), Room(1, 0), Room(1, 2) });

	REQUIRE(edges.size() == 4);
	CHECK(edges[0] == Edge{ 40, 40, 40, 80 });
	CHECK(edges[1] == Edge{ 80, 40, 80, 80 });
	CHECK(edges[2] == Edge{ 40, 40, 80, 40 });
	CHECK(edges[3] == Edge{ 40, 80, 80, 80 });
}

TEST_CASE("a side shared by two active rooms is not frontier") {
	std::vector<Edge> edges = Trace({ Room(0, 0), Room(1, 0) }, {});

	CHECK(edges.empty());
}

TEST_CASE("frontier along a row of rooms is one edge") {
	std::vector<Edge> edges = Trace({ Room(0, 0), Room(1, 0), Room(2, 0) },
		{ Room(0, 1), Room(1, 1), Room(2, 1) });

	REQUIRE(edges.size() == 1);
	CHECK(edges[0] == Edge{ 0, 40, 120, 40 });
}

TEST_CASE("one inactive room along two active ones is one edge") {
	Rect wide{ 0, 40, 80, 40 };
	std::vector<Edge> edges = Trace({ Room(0, 0), Room(1, 0) }, { wide });

	REQUIRE(edges.size() == 1);
	CHECK(edges[0] == Edge{ 0, 40, 80, 40 });
}

TEST_CASE("the frontier covers only the part of a side the inactive room lies against") {
	Rect narrow{ 40, 10, 20, 15 };
	std::vector<Edge> edges = Trace({ Room(0, 0) }, { narrow });

	REQUIRE(edges.size() == 1);
	CHECK(edges[0] == Edge{ 40, 10, 40, 25 });
}

TEST_CASE("rooms meeting only at a corner have no frontier") {
	CHECK(Trace({ Room(0, 0) }, { Room(1, 1) }).empty());
}

TEST_CASE("the frontier turns a corner as two edges meeting there") {
	// An L of active rooms with the inactive room in its crook.
	std::vector<Edge> edges = Trace({ Room(0, 0), Room(1, 0), Room(0, 1) }, { Room(1, 1) });

	REQUIRE(edges.size() == 2);
	CHECK(edges[0] == Edge{ 40, 40, 40, 80 });
	CHECK(edges[1] == Edge{ 40, 40, 80, 40 });
}

TEST_CASE("a room with no extent is ignored") {
	CHECK(Trace({ Room(0, 0) }, { Rect{ 40, 0, 0, 40 } }).empty());
	CHECK(Trace({ Rect{ 40, 0, 0, 40 } }, { Room(1, 0) }).empty());
}
