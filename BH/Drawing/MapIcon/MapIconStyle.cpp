#include "MapIconStyle.h"
#include "../../Config.h"

static void Clamp(unsigned int& value, unsigned int min, unsigned int max) {
	if (value < min)
		value = min;
	if (value > max)
		value = max;
}

void ReadMapIconStyle(Config* config, const std::string& prefix,
		MapIconStyle& style, const MapIconStyle& defaults) {
	std::vector<std::string> names;
	for (int i = 0; i < MapIcon::IconCount; i++)
		names.push_back(MapIcon::Name((MapIcon::Icon)i));

	config->ReadNamed(prefix + " Icon", style.icon, names, defaults.icon);
	config->ReadInt(prefix + " Size", style.size, defaults.size);
	config->ReadInt(prefix + " Fill Color", style.fillColor, defaults.fillColor);
	config->ReadInt(prefix + " Border Color", style.borderColor, defaults.borderColor);
	config->ReadInt(prefix + " Border Width", style.borderWidth, defaults.borderWidth);
	style.line = defaults.line;
	config->ReadBoolean(prefix + " Line", style.line);
	config->ReadInt(prefix + " Line Color", style.lineColor, defaults.lineColor);

	Clamp(style.size, MapIcon::MinSize, MapIcon::MaxSize);
	Clamp(style.borderWidth, MapIcon::MinBorder, MapIcon::MaxBorder);
}
