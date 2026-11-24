#include "ShopHighlights.h"
#include "../Item/ItemFilter.h"

namespace ShopHighlights {

namespace {

// A rule's colours run past the palette for the values that mean "none":
// UNDEFINED_COLOR where the rule gives none, DEAD_COLOR where it asks for none.
bool IsPaletteColor(int color) {
	return color >= 0 && color <= 0xFF;
}

}  // namespace

unsigned int BackdropColor(const std::vector<const Action*>& mapActions,
		ColorSource source, unsigned int fixed) {
	if (source == ColorFixed || mapActions.empty())
		return fixed;
	const Action* first = mapActions[0];
	if (IsPaletteColor(first->borderColor))
		return first->borderColor;
	if (IsPaletteColor(first->colorOnMap))
		return first->colorOnMap;
	return fixed;
}

std::string AnnounceLine(const std::string& npc, const std::string& singular,
		const std::string& plural, const std::vector<std::string>& names) {
	std::string line = npc + " has " + std::to_string(names.size()) + " " +
		(names.size() == 1 ? singular : plural) + ":";
	unsigned int listed = 0;
	for (; listed < names.size() && listed < ANNOUNCE_NAMES; listed++)
		line += (listed == 0 ? " " : ", ") + names[listed];
	if (listed < names.size())
		line += " and " + std::to_string(names.size() - listed) + " more";
	return line;
}

}
