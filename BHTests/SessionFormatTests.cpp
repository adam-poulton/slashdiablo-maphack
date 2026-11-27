#include "doctest.h"
#include "Modules/Session/SessionFormat.h"

/*
 * How the session tracker writes a length of time and a rate, which is how the
 * run tracker's own tokens write theirs.
 */

using SessionFormat::Clock;
using SessionFormat::Rate;

TEST_CASE("a length of time is hours, minutes and seconds, two digits each") {
	CHECK(Clock(0) == "00:00:00");
	CHECK(Clock(999) == "00:00:00");
	CHECK(Clock(1000) == "00:00:01");
	CHECK(Clock(59 * 1000) == "00:00:59");
	CHECK(Clock(60 * 1000) == "00:01:00");
	CHECK(Clock((3600 + 2 * 60 + 3) * 1000) == "01:02:03");
}

TEST_CASE("hours run past two digits rather than wrapping") {
	CHECK(Clock(100ull * 3600 * 1000) == "100:00:00");
}

TEST_CASE("a rate is signed, to two decimals, per second") {
	CHECK(Rate(0) == "+0.00/s");
	CHECK(Rate(12.25) == "+12.25/s");
	CHECK(Rate(999) == "+999.00/s");
	CHECK(Rate(-12.5) == "-12.50/s");
}

TEST_CASE("a large rate is shortened to thousands, millions or billions") {
	CHECK(Rate(1500) == "+1.50K/s");
	CHECK(Rate(1250000) == "+1.25M/s");
	CHECK(Rate(2500000000.0) == "+2.50B/s");
	CHECK(Rate(-1500) == "-1.50K/s");
}

TEST_CASE("a rate is shortened only once it is past the unit, as the run tracker's is") {
	CHECK(Rate(1000) == "+1000.00/s");
	CHECK(Rate(1000000) == "+1000.00K/s");
}
