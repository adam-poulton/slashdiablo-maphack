#include "Catalogues.h"
#include <string>
#include <vector>
#include "../ItemDescription.h"
#include "../StatDescriptions.h"
#include "../StringUtil.h"
#include "BaseItemCatalogue.h"
#include "RecipeCatalogue.h"
#include "RunewordCatalogue.h"
#include "SetCatalogue.h"
#include "StatIndex.h"
#include "UniqueCatalogue.h"

namespace Catalogue {

static bool loaded = false;

// The words a player types when looking for an item: what it is called, what it
// is made on or made from, what kind of thing that is, so that "amulet" finds
// Mara's and "jah" finds Enigma where neither name says so, the set it belongs
// to, the tier it is built in, the heading it sits under, and whatever else is
// said about it, so that "ladder" finds the recipes a ladder allows. Whatever a
// source has no answer for is left out rather than joined in as a gap.
static std::string SearchKeyFor(const Source& source) {
	std::vector<std::string> words = { source.name, source.baseName,
		source.ingredients, source.itemType, source.setName,
		ItemDescription::TierName(source.tier), source.heading };
	words.insert(words.end(), source.notes.begin(), source.notes.end());

	// A base item is the one source built in a tier of its own, and the one
	// whose table code a player would type: three letters is how the community
	// says "7gd" for a Colossus Blade. Every other kind is keyed by a code that
	// is either its name again or an internal one, so neither is worth
	// searching.
	if (source.tier != ItemDescription::TierNone)
		words.push_back(source.code);

	std::string key;
	for (unsigned int i = 0; i < words.size(); i++) {
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
	if (!RecipeCatalogue::Loaded())
		return;
	if (!BaseItemCatalogue::Loaded())
		return;

	RegisterAll(UniqueCatalogue::Kind, UniqueCatalogue::Sources());
	RegisterAll(SetCatalogue::Kind, SetCatalogue::Pieces());
	RegisterAll(SetCatalogue::BonusKind, SetCatalogue::Bonuses());
	RegisterAll(RunewordCatalogue::Kind, RunewordCatalogue::Sources());
	RegisterAll(RecipeCatalogue::Kind, RecipeCatalogue::Sources());
	RegisterAll(BaseItemCatalogue::Kind, BaseItemCatalogue::Sources());

	loaded = true;
}

bool Loaded() {
	return loaded;
}

}
