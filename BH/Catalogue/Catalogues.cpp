#include "Catalogues.h"
#include <string>
#include <vector>
#include "../StatDescriptions.h"
#include "../StringUtil.h"
#include "RunewordCatalogue.h"
#include "SetCatalogue.h"
#include "StatIndex.h"
#include "UniqueCatalogue.h"

namespace Catalogue {

static bool loaded = false;

// The words a player types when looking for an item: what it is called, what it
// is made on or made from, what kind of thing that is, so that "amulet" finds
// Mara's and "jah" finds Enigma where neither name says so, and the set it
// belongs to. Whatever a source has no answer for is left out rather than
// joined in as a gap.
static std::string SearchKeyFor(const Source& source) {
	const std::string words[] = { source.name, source.baseName,
		source.ingredients, source.itemType, source.setName };
	std::string key;
	for (unsigned int i = 0; i < sizeof(words) / sizeof(words[0]); i++) {
		if (words[i].length() == 0)
			continue;
		if (key.length() > 0)
			key += " ";
		key += words[i];
	}
	return ToLower(key);
}

static void RegisterAll(const std::string& kind,
		const std::vector<Source>& sources) {
	for (unsigned int i = 0; i < sources.size(); i++)
		StatIndex::Register(kind, SearchKeyFor(sources[i]), sources[i]);
}

void Load() {
	if (loaded)
		return;
	// Nothing is worth reading before the string tables are in: a source
	// remembered without them would carry no lines for a player to read.
	if (!StatDescriptions::IsInitialized())
		return;
	if (!UniqueCatalogue::Loaded())
		return;
	if (!SetCatalogue::Loaded())
		return;
	if (!RunewordCatalogue::Loaded())
		return;

	RegisterAll(UniqueCatalogue::Kind, UniqueCatalogue::Sources());
	RegisterAll(SetCatalogue::Kind, SetCatalogue::Pieces());
	RegisterAll(SetCatalogue::BonusKind, SetCatalogue::Bonuses());
	RegisterAll(RunewordCatalogue::Kind, RunewordCatalogue::Sources());

	loaded = true;
}

bool Loaded() {
	return loaded;
}

}
