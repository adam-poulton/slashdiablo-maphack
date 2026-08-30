#include "PropertyStats.h"
#include <algorithm>

namespace PropertyStats {

std::vector<std::string> Lines(const std::vector<Property>& properties) {
	std::vector<StatDescriptions::Stat> stats;
	for (unsigned int i = 0; i < properties.size(); i++) {
		StatDescriptions::CollectProperty(properties[i].code, properties[i].param,
			properties[i].min, properties[i].max, stats);
	}
	return StatDescriptions::BuildLines(stats);
}

std::vector<std::string> Lines(const std::vector<Property>& properties,
		const std::vector<std::string>& extraLines) {
	std::vector<std::string> lines = Lines(properties);
	lines.insert(lines.end(), extraLines.begin(), extraLines.end());
	return lines;
}

std::vector<std::string> CountedLines(const std::vector<Property>& properties) {
	std::vector<int> counts;
	for (unsigned int i = 0; i < properties.size(); i++) {
		// A count of one is a piece worn on its own, which is what the piece's
		// own properties already say.
		if (properties[i].itemCount < 2)
			continue;
		if (std::find(counts.begin(), counts.end(), properties[i].itemCount) == counts.end())
			counts.push_back(properties[i].itemCount);
	}
	std::sort(counts.begin(), counts.end());

	std::vector<std::string> lines;
	for (unsigned int c = 0; c < counts.size(); c++) {
		std::vector<Property> group;
		for (unsigned int i = 0; i < properties.size(); i++) {
			if (properties[i].itemCount == counts[c])
				group.push_back(properties[i]);
		}

		std::string tag = " (" + std::to_string(counts[c]) + " Items)";
		std::vector<std::string> rendered = Lines(group);
		for (unsigned int i = 0; i < rendered.size(); i++)
			lines.push_back(rendered[i] + tag);
	}
	return lines;
}

std::vector<StatDescriptions::StatTotal> Totals(
		const std::vector<Property>& properties) {
	std::vector<StatDescriptions::StatTotal> totals;
	for (unsigned int i = 0; i < properties.size(); i++) {
		StatDescriptions::CollectTotals(properties[i].code, properties[i].param,
			properties[i].min, properties[i].max, totals);
	}
	return totals;
}

};
