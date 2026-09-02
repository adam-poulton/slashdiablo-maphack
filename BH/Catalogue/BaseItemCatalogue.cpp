#include "BaseItemCatalogue.h"
#include <algorithm>
#include <map>
#include "../ItemDescription.h"
#include "../StatDescriptions.h"
#include "../TableReader.h"

namespace BaseItemCatalogue {

const char* const Kind = "base";

// Read once and kept, since the panel asks for a source a row at a time and the
// stat index asks for all of them at once. The lookup points into the vector,
// which is not touched again once it is built.
static std::vector<Catalogue::Source> sources;
static std::map<std::string, const Catalogue::Source*> byCode;
static bool loaded = false;

namespace {

// Where a base's item type falls in the order the headings are shown in, which
// is not a question about the base and so is not the source's to carry.
struct Grouped {
	Catalogue::Source source;
	int group;
};

}  // namespace

static Catalogue::Source SourceFor(const ItemDescription::Base& base) {
	Catalogue::Source source;
	source.code = base.code;
	source.name = base.name;
	// A base is the base it is made on, so that whatever reads a source's base
	// reads the same field whichever catalogue the source came out of.
	source.baseCode = base.code;
	source.baseName = base.name;
	source.itemType = base.typeName;
	source.tier = base.tier;
	source.level = base.level;
	source.requiredLevel = base.requirements.level;
	// Nothing has been made of it, which is how the game draws one.
	source.rarity = RarityNormal;
	return source;
}

static std::vector<Grouped> ReadBases(
		const std::vector<const ItemDescription::Base*>& bases,
		bool requireSpawnable) {
	std::vector<Grouped> read;

	// Headings in the order the tables first reach them, which walks the
	// weapons, then the armour, then everything else, rather than scattering the
	// two hundred armour bases through the alphabet.
	std::map<std::string, int> groupOrder;

	for (unsigned int i = 0; i < bases.size(); i++) {
		const ItemDescription::Base& base = *bases[i];
		if (requireSpawnable && !base.spawnable)
			continue;
		// A base with no name or no item type has nothing to be listed under.
		if (base.name.length() == 0 || base.typeName.length() == 0)
			continue;

		Grouped grouped;
		grouped.source = SourceFor(base);

		std::map<std::string, int>::iterator group =
			groupOrder.find(base.typeName);
		if (group == groupOrder.end()) {
			int next = (int)groupOrder.size();
			group = groupOrder.insert(std::make_pair(base.typeName, next)).first;
		}
		grouped.group = group->second;

		read.push_back(grouped);
	}
	return read;
}

std::vector<Catalogue::Source> Read(
		const std::vector<const ItemDescription::Base*>& bases) {
	// Only what the game drops, which leaves out the quest pieces and the rows
	// that were never finished. A table carrying no such column would leave
	// nothing at all, so a second pass takes everything rather than no bases.
	std::vector<Grouped> read = ReadBases(bases, true);
	if (read.empty())
		read = ReadBases(bases, false);

	// Stable, so bases sharing a tier and a level keep their table order, which
	// is the game's own progression through them.
	std::stable_sort(read.begin(), read.end(),
			[](const Grouped& a, const Grouped& b) {
		if (a.group != b.group)
			return a.group < b.group;
		if (a.source.tier != b.source.tier)
			return a.source.tier < b.source.tier;
		return a.source.level < b.source.level;
	});

	std::vector<Catalogue::Source> ordered;
	ordered.reserve(read.size());
	for (unsigned int i = 0; i < read.size(); i++)
		ordered.push_back(read[i].source);
	return ordered;
}

// Nothing is kept until the string tables are in, since a base takes the name
// the player reads from there and Misc.txt calls a small charm "Charm Small".
static void Load() {
	if (loaded)
		return;
	if (!StatDescriptions::IsInitialized())
		return;
	// ItemDescription keeps no base until every table one is read out of has
	// been read, so an empty list is either data still on its way or a realm
	// that has emptied a table. The game saying its tables are in tells them
	// apart, and the second leaves the catalogue loaded and empty.
	if (ItemDescription::AllBases().empty() && !Tables::isInitialized())
		return;

	sources = Read(ItemDescription::AllBases());

	for (unsigned int i = 0; i < sources.size(); i++)
		byCode.insert(std::make_pair(sources[i].code, &sources[i]));

	loaded = true;
}

const std::vector<Catalogue::Source>& Sources() {
	Load();
	return sources;
}

const Catalogue::Source* Find(const std::string& code) {
	Load();
	std::map<std::string, const Catalogue::Source*>::iterator found =
		byCode.find(code);
	return (found != byCode.end()) ? found->second : NULL;
}

bool Loaded() {
	Load();
	return loaded;
}

}
