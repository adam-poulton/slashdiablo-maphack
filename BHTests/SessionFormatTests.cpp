#include "doctest.h"
#include "Modules/Session/SessionFormat.h"

/*
 * How the session tracker writes a length of time and a rate, which is how the
 * run tracker's own tokens write theirs.
 */

using SessionFormat::Clock;
using SessionFormat::Count;
using SessionFormat::Rate;
using SessionFormat::ShortClock;
using SessionFormat::ShortRate;

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

TEST_CASE("a short length of time leaves out the hours it has not reached") {
	CHECK(ShortClock(0) == "0:00");
	CHECK(ShortClock(9 * 1000) == "0:09");
	CHECK(ShortClock((3 * 60 + 21) * 1000) == "3:21");
	CHECK(ShortClock((59 * 60 + 59) * 1000) == "59:59");
	CHECK(ShortClock(3600 * 1000) == "1:00:00");
	CHECK(ShortClock((3600 + 2 * 60 + 3) * 1000) == "1:02:03");
	CHECK(ShortClock(100ull * 3600 * 1000) == "100:00:00");
}

TEST_CASE("a short rate is signed and kept to about three figures") {
	CHECK(ShortRate(0) == "+0/s");
	CHECK(ShortRate(125) == "+125/s");
	CHECK(ShortRate(1000) == "+1.0K/s");
	CHECK(ShortRate(12500) == "+12.5K/s");
	CHECK(ShortRate(980400) == "+980.4K/s");
	CHECK(ShortRate(1250000) == "+1.25M/s");
	CHECK(ShortRate(2500000000.0) == "+2.50B/s");
	CHECK(ShortRate(-12500) == "-12.5K/s");
	CHECK(ShortRate(-125) == "-125/s");
}

TEST_CASE("a count has its thousands marked") {
	CHECK(Count(0) == "0");
	CHECK(Count(999) == "999");
	CHECK(Count(1000) == "1,000");
	CHECK(Count(12345678) == "12,345,678");
	CHECK(Count(3520485254ll) == "3,520,485,254");
	CHECK(Count(-1234) == "-1,234");
	CHECK(Count(-999) == "-999");
}
