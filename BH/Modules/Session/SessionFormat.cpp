#include "SessionFormat.h"
#include <cmath>
#include <cstdio>

namespace SessionFormat {

std::string Clock(unsigned long long ms) {
	const unsigned long long seconds = ms / 1000;
	char text[32];
	sprintf_s(text, "%.2llu:%.2llu:%.2llu", seconds / 3600, (seconds / 60) % 60,
		seconds % 60);
	return text;
}

std::string Rate(double perSecond) {
	const char* unit = "";
	const double magnitude = std::fabs(perSecond);
	if (magnitude > 1E9) {
		perSecond /= 1E9;
		unit = "B";
	} else if (magnitude > 1E6) {
		perSecond /= 1E6;
		unit = "M";
	} else if (magnitude > 1E3) {
		perSecond /= 1E3;
		unit = "K";
	}
	char text[32];
	sprintf_s(text, "%s%.2f%s/s", perSecond >= 0 ? "+" : "", perSecond, unit);
	return text;
}

}
