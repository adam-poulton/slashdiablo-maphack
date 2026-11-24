#include "doctest.h"
#include "Modules/TownPortal/PortalDelay.h"

/*
 * When a town portal is refused.
 *
 * The game refuses one for as long as the player carries just_portaled. With
 * the delay shortened, it is refused only until the delay has passed since the
 * state was gained, and never once the state is gone. Ticks are 32-bit and
 * wrap, which must not reopen a portal early or hold one shut.
 */

using PortalDelay::Clock;
using PortalDelay::Refuses;
using PortalDelay::Since;

TEST_CASE("a portal is refused while the state is held and the delay has not passed") {
	CHECK(Refuses(true, 0, 1000));
	CHECK(Refuses(true, 500, 1000));
	CHECK(Refuses(true, 999, 1000));
}

TEST_CASE("a portal is allowed once the delay has passed") {
	CHECK_FALSE(Refuses(true, 1000, 1000));
	CHECK_FALSE(Refuses(true, 1200, 1000));
}

TEST_CASE("a portal is never refused without the state") {
	CHECK_FALSE(Refuses(false, 0, 1000));
	CHECK_FALSE(Refuses(false, 0, 3000));
}

TEST_CASE("a delay of nothing never refuses") {
	CHECK_FALSE(Refuses(true, 0, 0));
}

TEST_CASE("the time since a tick counts across the tick count wrapping") {
	CHECK(Since(100, 600) == 500);
	CHECK(Since(0xFFFFFF00u, 0x00000100u) == 0x200);
	CHECK(Since(0xFFFFFFFFu, 0) == 1);
}

TEST_CASE("the clock refuses until the delay has passed since the state was gained") {
	Clock clock;
	clock.Gained(10000);
	clock.Observe(true, 10040);

	CHECK(clock.Refuses(true, 10500, 1000));
	CHECK_FALSE(clock.Refuses(true, 11000, 1000));
	CHECK_FALSE(clock.Refuses(true, 11200, 1000));
}

TEST_CASE("the clock counts across the tick count wrapping") {
	Clock clock;
	clock.Gained(0xFFFFFE00u);
	clock.Observe(true, 0xFFFFFE00u);

	CHECK(clock.Refuses(true, 0x00000100u, 1000));
	CHECK_FALSE(clock.Refuses(true, 0x00000200u, 1000));
}

TEST_CASE("the clock times from the packet, not from when the state was first seen") {
	Clock clock;
	clock.Gained(10000);
	clock.Observe(false, 10010);
	clock.Observe(true, 10400);

	CHECK_FALSE(clock.Refuses(true, 11000, 1000));
}

TEST_CASE("a state that arrives without its packet is timed from when it was seen") {
	Clock clock;
	clock.Observe(false, 9000);
	clock.Observe(true, 10000);

	CHECK(clock.Refuses(true, 10900, 1000));
	CHECK_FALSE(clock.Refuses(true, 11000, 1000));
}

TEST_CASE("a state that was never timed is refused, as the game would") {
	Clock clock;
	CHECK(clock.Refuses(true, 10000, 1000));
}

TEST_CASE("another portal taken while the state is held restarts the clock") {
	Clock clock;
	clock.Gained(10000);
	clock.Observe(true, 10000);
	clock.Observe(true, 12000);
	clock.Gained(12000);

	CHECK(clock.Refuses(true, 12500, 1000));
}

TEST_CASE("a state that went away and came back is timed afresh") {
	Clock clock;
	clock.Gained(10000);
	clock.Observe(true, 10000);
	clock.Observe(false, 15000);
	clock.Observe(true, 20000);

	CHECK(clock.Refuses(true, 20500, 1000));
	CHECK_FALSE(clock.Refuses(true, 21000, 1000));
}

TEST_CASE("a reset clock refuses a held state until it is timed again") {
	Clock clock;
	clock.Gained(10000);
	clock.Observe(true, 10000);
	clock.Reset();

	CHECK(clock.Refuses(true, 50000, 1000));

	clock.Observe(true, 50000);
	CHECK_FALSE(clock.Refuses(true, 51000, 1000));
}

TEST_CASE("the clock never refuses without the state") {
	Clock clock;
	clock.Gained(10000);
	CHECK_FALSE(clock.Refuses(false, 10000, 3000));
}
