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

}
