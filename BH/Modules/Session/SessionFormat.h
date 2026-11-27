#pragma once
#include <string>

// How the session tracker writes its figures, wherever they are shown.
namespace SessionFormat {

// A length of time as hours, minutes and seconds, "01:02:03": the form
// %GAMETIME% has, so the two read alike side by side. Hours run past 99.
std::string Clock(unsigned long long ms);

// Experience per second, signed and shortened to thousands, millions or
// billions with two decimals, "+1.25M/s": the form %LASTXPPERSEC% has.
std::string Rate(double perSecond);

// The forms the HUD and the session window write, which have no run tracker
// token to match and are kept short: a length of time without the hours it has
// not reached, "3:21" and "1:02:03", and a rate to about three figures,
// "+980.4K/s" and "+1.25M/s".
std::string ShortClock(unsigned long long ms);
std::string ShortRate(double perSecond);

// A whole number with its thousands marked, "12,345,678".
std::string Count(long long number);

// An amount gained or lost, signed and to about three figures: "+37.18M",
// "+980.4K", "-125".
std::string ShortAmount(long long amount);

// How far a run is from the time it is compared with, signed: to a tenth of a
// second under a minute, "-12.3", and to the second past it, "+1:02".
std::string Delta(long long ms);

// A name as the game writes one, made a line of plain text: without the colour
// codes the game puts in it, and with a space where it broke onto a new line.
std::string Plain(const std::string& name);

}
