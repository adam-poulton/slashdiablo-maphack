#include "Catalogues.h"
#include <string>
#include <vector>
#include "../StatDescriptions.h"
#include "../StringUtil.h"
#include "StatIndex.h"
#include "UniqueCatalogue.h"

namespace Catalogue {

static bool loaded = false;

// The words a player types when looking for an item: what it is called, what it
// is made on, and what kind of thing that is, so that "amulet" finds Mara's
// where its own name never says so.
static std::string SearchKeyFor(const Source& source) {
	return ToLower(source.name + " " + source.baseName + " " + source.itemType);
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

	RegisterAll(UniqueCatalogue::Kind, UniqueCatalogue::Sources());

	loaded = true;
}

bool Loaded() {
	return loaded;
}

}
