#include "doctest.h"
#include "Drawing/Advanced/Sliderhook/SliderRange.h"

/*
 * The values a slider will take.
 *
 * A slider exists where a number typed into a box would be dangerous: the join
 * wait is the case it was written for, since a wait shorter than loading into a
 * game fails every join, and the settings window that would put it back opens
 * only in game. So what is checked here is that there is no way to end up outside
 * the range - from a drag, from an arrow key, or from a config file naming
 * something the range never offered.
 */

using Drawing::SliderRange;

// The join wait: a second to four, in half seconds.
static SliderRange FailToJoin() {
	return SliderRange(1000, 4000, 500);
}

TEST_CASE("a range offers every step from its floor to its ceiling") {
	SliderRange range = FailToJoin();

	CHECK(range.StepCount() == 6);		// seven positions, six gaps
	CHECK(range.ValueForIndex(0) == 1000);
	CHECK(range.ValueForIndex(1) == 1500);
	CHECK(range.ValueForIndex(2) == 2000);
	CHECK(range.ValueForIndex(3) == 2500);
	CHECK(range.ValueForIndex(4) == 3000);
	CHECK(range.ValueForIndex(5) == 3500);
	CHECK(range.ValueForIndex(6) == 4000);
}

TEST_CASE("a position past the end of the rail is the end of the rail") {
	SliderRange range = FailToJoin();

	CHECK(range.ValueForIndex(7) == 4000);
	CHECK(range.ValueForIndex(9999) == 4000);
}

TEST_CASE("a value from outside the range is brought inside it") {
	SliderRange range = FailToJoin();

	// What a file written by the box this slider replaced can hold: zero meant
	// leave the client's own wait alone, and anything at all could be typed.
	CHECK(range.Snap(0) == 1000);
	CHECK(range.Snap(1) == 1000);
	CHECK(range.Snap(999) == 1000);
	CHECK(range.Snap(4001) == 4000);
	CHECK(range.Snap(60000) == 4000);
}

TEST_CASE("a value between two steps lands on the nearer one") {
	SliderRange range = FailToJoin();

	CHECK(range.Snap(1100) == 1000);
	CHECK(range.Snap(1249) == 1000);
	CHECK(range.Snap(1250) == 1500);	// the halfway point rounds up
	CHECK(range.Snap(1400) == 1500);
	CHECK(range.Snap(3900) == 4000);
}

TEST_CASE("a value already on a step is left alone") {
	SliderRange range = FailToJoin();

	for (unsigned int at = 1000; at <= 4000; at += 500)
		CHECK(range.Snap(at) == at);
}

TEST_CASE("a range that does not divide evenly still reaches its ceiling") {
	// Four full steps to 900 and a short one to the top, rather than a rail that
	// stops at 900 and a maximum nothing can reach.
	SliderRange range(100, 1000, 200);

	CHECK(range.StepCount() == 5);
	CHECK(range.ValueForIndex(4) == 900);
	CHECK(range.ValueForIndex(5) == 1000);
	CHECK(range.Snap(1000) == 1000);

	// The short gap is measured from both its ends: 900 to 1000 turns over at 950,
	// not half a step of 200 above 900.
	CHECK(range.Snap(940) == 900);
	CHECK(range.Snap(950) == 1000);
	CHECK(range.Snap(960) == 1000);
}

TEST_CASE("a range with nowhere to go holds its one value") {
	SliderRange range(2000, 2000, 500);

	CHECK(range.StepCount() == 0);
	CHECK(range.Snap(0) == 2000);
	CHECK(range.Snap(9999) == 2000);
	CHECK(range.IndexForValue(9999) == 0);
}

TEST_CASE("a range built the wrong way round is the range it names") {
	SliderRange range(4000, 1000, 500);

	CHECK(range.min == 1000);
	CHECK(range.max == 4000);
	CHECK(range.Snap(0) == 1000);
	CHECK(range.Snap(9999) == 4000);
}

TEST_CASE("a step of nothing still leaves a usable rail") {
	// Would divide by zero everywhere below if it were taken at its word.
	SliderRange range(1000, 4000, 0);

	CHECK(range.step == 1);
	CHECK(range.StepCount() == 3000);
	CHECK(range.Snap(2500) == 2500);
	CHECK(range.Snap(0) == 1000);
}

TEST_CASE("dragging along the rail picks the position under the cursor") {
	SliderRange range = FailToJoin();
	const unsigned int travel = 120;	// pixels the thumb has to run in

	CHECK(range.IndexForTravel(0, travel) == 0);
	CHECK(range.IndexForTravel(20, travel) == 1);
	CHECK(range.IndexForTravel(60, travel) == 3);
	CHECK(range.IndexForTravel(120, travel) == 6);
}

TEST_CASE("a drag past either end of the rail stops at that end") {
	SliderRange range = FailToJoin();
	const unsigned int travel = 120;

	CHECK(range.IndexForTravel(-1, travel) == 0);
	CHECK(range.IndexForTravel(-500, travel) == 0);
	CHECK(range.IndexForTravel(121, travel) == 6);
	CHECK(range.IndexForTravel(9999, travel) == 6);
}

TEST_CASE("a drag turns over at the halfway point between two positions") {
	SliderRange range = FailToJoin();
	const unsigned int travel = 120;	// 20 pixels to a step

	CHECK(range.IndexForTravel(9, travel) == 0);
	CHECK(range.IndexForTravel(10, travel) == 1);	// halfway, rounds up
	CHECK(range.IndexForTravel(29, travel) == 1);
	CHECK(range.IndexForTravel(30, travel) == 2);
}

TEST_CASE("a rail with no room to drag reports its first position") {
	SliderRange range = FailToJoin();

	CHECK(range.IndexForTravel(50, 0) == 0);
}

TEST_CASE("every position on the rail is a value the range offers") {
	SliderRange range = FailToJoin();
	const unsigned int travel = 120;

	// Whatever the cursor is doing, what comes back is on a step: this is the
	// guarantee the whole control exists for.
	for (int along = -20; along <= 140; along++) {
		unsigned int value = range.ValueForIndex(range.IndexForTravel(along, travel));
		CHECK(value >= range.min);
		CHECK(value <= range.max);
		CHECK(range.Snap(value) == value);
	}
}
