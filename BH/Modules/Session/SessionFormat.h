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

}
