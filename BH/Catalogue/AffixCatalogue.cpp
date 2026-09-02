#include "AffixCatalogue.h"
#include <algorithm>
#include <string>
#include <vector>
#include "../ItemDescription.h"
#include "../ItemRarity.h"
#include "../PropertyStats.h"
#include "../StatDescriptions.h"
#include "../StringUtil.h"
#include "../TableReader.h"

namespace AffixCatalogue {

const char* const PrefixKind = "prefix";
const char* const SuffixKind = "suffix";

// How many property entries an affix carries, and how many kinds of base it can
// name as the ones it rolls on and the ones it does not.
static const int kPropertyCount = 3;
static const int kTypeCount = 7;
static const int kExcludedTypeCount = 5;

// Read once and kept, since the stat index holds each source by address.
static std::vector<Catalogue::Source> prefixes;
static std::vector<Catalogue::Source> suffixes;
static bool loaded = false;

// What the player reads, out of the string table where there is an entry for
// it. The tables key an affix by the same word the English client shows, so a
// client whose strings we could not read still names it.
static std::string NameOf(const std::string& code) {
	std::string localized = StatDescriptions::GetString(code);
	return (localized.length() > 0) ? localized : code;
}

// The kinds of base one of the two columns of them names, in table order.
static void AppendTypes(JSONObject& entry, const char* column, int count,
		std::vector<std::string>& out) {
	for (int n = 1; n <= count; n++) {
		std::string code = Trim(entry.getString(column + std::to_string(n)));
		if (code.length() == 0)
			continue;
		// A row naming the same kind of base twice, which some of the tables
		// do, says nothing the once did not.
		std::string name = ItemDescription::TypeName(code);
		if (std::find(out.begin(), out.end(), name) == out.end())
			out.push_back(name);
	}
}

// The kinds of base an affix rolls on, down to the ones it does not: "Any
// Armor, Ring, Amulet" or "Melee Weapon (not Wand, Orb)". The exclusions are
// the tables' own way of carving a kind out of a broader one, so leaving them
// off would say the affix rolls where it cannot.
static std::string ItemTypeOf(JSONObject& entry) {
	std::vector<std::string> types;
	AppendTypes(entry, "itype", kTypeCount, types);

	std::vector<std::string> excluded;
	AppendTypes(entry, "etype", kExcludedTypeCount, excluded);

	std::string itemType = Join(types, ", ");
	// A row carving kinds out of nothing carves nothing, so the exclusions are
	// only worth saying where there is a kind for them to narrow.
	if (itemType.length() > 0 && !excluded.empty())
		itemType += " (not " + Join(excluded, ", ") + ")";
	return itemType;
}

static std::vector<Catalogue::Source> ReadRows(Table& table,
		bool requireSpawnable) {
	std::vector<Catalogue::Source> read;
	for (int i = 0; i < table.size(); i++) {
		JSONObject* entry = table.entryAt(i);
		if (!entry)
			continue;
		if (requireSpawnable && entry->getString("spawnable").compare("1") != 0)
			continue;

		Catalogue::Source source;
		source.code = Trim(entry->getString("Name"));
		if (source.code.length() == 0)
			continue;

		for (int n = 1; n <= kPropertyCount; n++) {
			std::string index = std::to_string(n);
			PropertyStats::Property property;
			property.code = ToLower(Trim(entry->getString("mod" + index + "code")));
			if (property.code.length() == 0)
				continue;
			property.param = Trim(entry->getString("mod" + index + "param"));
			property.min = atoi(entry->getString("mod" + index + "min").c_str());
			property.max = atoi(entry->getString("mod" + index + "max").c_str());
			source.properties.push_back(property);
		}
		// The dividers the file is padded out with grant nothing, which is what
		// leaves them out of the catalogue. An affix granting nothing would be
		// one the game rolls and the player could not tell it had.
		if (source.properties.empty())
			continue;

		source.name = NameOf(source.code);
		source.itemType = ItemTypeOf(*entry);
		source.level = atoi(entry->getString("level").c_str());
		source.requiredLevel = atoi(entry->getString("levelreq").c_str());
		// Magic is the only quality an affix is drawn in on its own. It also
		// goes on rares and on crafted items, but there it shares the name with
		// others and none of them colours it.
		source.rarity = RarityMagic;
		// Which class can carry it, where the affix is one only a class rolls.
		// A note rather than part of what it rolls on, because the tables
		// restrict a class affix by the class and not by the kinds of base:
		// several of them roll on a charm anyone can pick up.
		std::string classOnly = StatDescriptions::GetClassOnly(
			ToLower(Trim(entry->getString("classspecific"))));
		if (classOnly.length() > 0)
			source.notes.push_back(classOnly);
		source.lines = PropertyStats::Lines(source.properties);

		read.push_back(source);
	}
	return read;
}

std::vector<Catalogue::Source> Read(Table& table) {
	// Only what the game rolls, which leaves out the rows kept in the table but
	// turned off. A table carrying no such column would leave nothing at all,
	// so a second pass takes every row rather than no affixes.
	std::vector<Catalogue::Source> read = ReadRows(table, true);
	if (read.empty())
		read = ReadRows(table, false);
	return read;
}

// Nothing is kept until the string table text is in, so that a source is never
// remembered without the lines it words its properties into, nor without the
// name a player reads.
static void Load() {
	if (loaded)
		return;
	if (!StatDescriptions::IsInitialized())
		return;
	// Once the game says its tables are in, whatever they hold, so that a table
	// a realm has emptied leaves the catalogue loaded and empty rather than
	// waiting for rows that never come. Ahead of that, only when both tables
	// have rows and ItemTypes.txt does too, since an affix without it would be
	// kept for good saying nothing about what it rolls on.
	if (!Tables::isInitialized() && (Tables::MagicPrefix.size() == 0 ||
			Tables::MagicSuffix.size() == 0 || Tables::ItemTypes.size() == 0))
		return;

	prefixes = Read(Tables::MagicPrefix);
	suffixes = Read(Tables::MagicSuffix);

	loaded = true;
}

const std::vector<Catalogue::Source>& Prefixes() {
	Load();
	return prefixes;
}

const std::vector<Catalogue::Source>& Suffixes() {
	Load();
	return suffixes;
}

bool Loaded() {
	Load();
	return loaded;
}

}
