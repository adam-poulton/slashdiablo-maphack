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

std::string ShortClock(unsigned long long ms) {
	const unsigned long long seconds = ms / 1000;
	char text[32];
	if (seconds >= 3600)
		sprintf_s(text, "%llu:%.2llu:%.2llu", seconds / 3600, (seconds / 60) % 60, seconds % 60);
	else
		sprintf_s(text, "%llu:%.2llu", seconds / 60, seconds % 60);
	return text;
}

std::string ShortRate(double perSecond) {
	const char* sign = perSecond >= 0 ? "+" : "-";
	const double magnitude = std::fabs(perSecond);
	char text[32];
	if (magnitude >= 1E9)
		sprintf_s(text, "%s%.2fB/s", sign, magnitude / 1E9);
	else if (magnitude >= 1E6)
		sprintf_s(text, "%s%.2fM/s", sign, magnitude / 1E6);
	else if (magnitude >= 1E3)
		sprintf_s(text, "%s%.1fK/s", sign, magnitude / 1E3);
	else
		sprintf_s(text, "%s%.0f/s", sign, magnitude);
	return text;
}

std::string Count(long long number) {
	const std::string digits = std::to_string(number < 0 ? -number : number);
	std::string text;
	for (size_t i = 0; i < digits.size(); i++) {
		if (i > 0 && (digits.size() - i) % 3 == 0)
			text += ',';
		text += digits[i];
	}
	return number < 0 ? "-" + text : text;
}

}
