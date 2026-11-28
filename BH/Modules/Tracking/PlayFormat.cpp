#include "PlayFormat.h"
#include <cmath>
#include <cstdio>

namespace PlayFormat {

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

namespace {
	// A signed quantity to about three figures, in thousands, millions or
	// billions where it runs to them.
	std::string Short(double quantity) {
		const char* sign = quantity >= 0 ? "+" : "-";
		const double magnitude = std::fabs(quantity);
		char text[32];
		if (magnitude >= 1E9)
			sprintf_s(text, "%s%.2fB", sign, magnitude / 1E9);
		else if (magnitude >= 1E6)
			sprintf_s(text, "%s%.2fM", sign, magnitude / 1E6);
		else if (magnitude >= 1E3)
			sprintf_s(text, "%s%.1fK", sign, magnitude / 1E3);
		else
			sprintf_s(text, "%s%.0f", sign, magnitude);
		return text;
	}
}

std::string ShortRate(double perSecond) {
	return Short(perSecond) + "/s";
}

std::string ShortAmount(long long amount) {
	return Short((double)amount);
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

std::string Delta(long long ms) {
	const char* sign = ms < 0 ? "-" : "+";
	const unsigned long long magnitude = (unsigned long long)(ms < 0 ? -ms : ms);
	if (magnitude >= 60 * 1000)
		return sign + ShortClock(magnitude);
	char text[32];
	sprintf_s(text, "%s%llu.%llu", sign, magnitude / 1000, (magnitude % 1000) / 100);
	return text;
}

std::string Plain(const std::string& name) {
	std::string text;
	for (size_t i = 0; i < name.size(); i++) {
		// A colour code is 0xFF, 'c' and the colour.
		if ((unsigned char)name[i] == 0xFF && i + 1 < name.size() && name[i + 1] == 'c') {
			i += 2;
			continue;
		}
		const bool breaks = name[i] == '\n' || name[i] == '\r';
		if (breaks && (text.empty() || text.back() == ' '))
			continue;
		text += breaks ? ' ' : name[i];
	}
	while (!text.empty() && text.back() == ' ')
		text.pop_back();
	return text;
}

}
