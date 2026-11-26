#include "MapIconStyle.h"
#include "../../Config.h"

void ReadMapIconStyle(Config* config, const std::string& prefix,
		MapIconStyle& style, const MapIconStyle& defaults) {
	std::vector<std::string> names;
	for (int i = 0; i < MapIcon::IconCount; i++)
		names.push_back(MapIcon::Name((MapIcon::Icon)i));

	config->ReadNamed(prefix + " Icon", style.icon, names, defaults.icon);
	config->ReadInt(prefix + " Size", style.size, defaults.size);
	config->ReadInt(prefix + " Color", style.color, defaults.color);
	style.line = defaults.line;
	config->ReadBoolean(prefix + " Line", style.line);
	config->ReadInt(prefix + " Line Color", style.lineColor, defaults.lineColor);

	if (style.size < MapIcon::MinSize)
		style.size = MapIcon::MinSize;
	if (style.size > MapIcon::MaxSize)
		style.size = MapIcon::MaxSize;
}
