#include "UniqueCatalogue.h"
#include <algorithm>
#include <map>
#include "../ItemDescription.h"
#include "../StatDescriptions.h"
#include "../StringUtil.h"
#include "../TableReader.h"

namespace UniqueCatalogue {

const char* const Kind = "unique";

// How many prop/par/min/max groups UniqueItems.txt gives each row.
static const int kPropertyCount = 12;

// Read once and kept, since the panel asks for a source a row at a time and the
// stat index asks for all of them at once. The lookup points into the vector,
// which is not touched again once it is built.
static std::vector<Catalogue::Source> sources;
static std::map<std::string, const Catalogue::Source*> byCode;
static bool loaded = false;

// "index" doubles as the string table key, which is preferred so a localised
// client reads correctly. The index itself is the fallback, which is what a
// unique a realm has added is called.
static std::string NameOf(const std::string& code) {
	std::string localized = StatDescriptions::GetString(code);
	return (localized.length() > 0) ? localized : code;
}

static std::vector<PropertyStats::Property> ReadProperties(
		const JSONObject* entry) {
	std::vector<PropertyStats::Property> properties;
	for (int n = 1; n <= kPropertyCount; n++) {
		std::string suffix = std::to_string(n);
		PropertyStats::Property property;
		property.code = Trim(entry->getString("prop" + suffix));
		if (property.code.length() == 0)
			continue;
		property.param = Trim(entry->getString("par" + suffix));
		property.min = atoi(entry->getString("min" + suffix).c_str());
		property.max = atoi(entry->getString("max" + suffix).c_str());
		properties.push_back(property);
	}
	return properties;
}

static std::vector<Catalogue::Source> ReadRows(Table& table,
		bool requireEnabled) {
	std::vector<Catalogue::Source> read;
	for (int i = 0; i < table.size(); i++) {
		JSONObject* entry = table.entryAt(i);
		if (!entry)
			continue;
		if (requireEnabled && entry->getString("enabled").compare("1") != 0)
			continue;

		// The dividers and the unreleased rows the file is padded out with name
		// no base item, which is what leaves them out of the catalogue.
		std::string baseCode = Trim(entry->getString("code"));
		if (baseCode.length() == 0)
			continue;

		std::string code = Trim(entry->getString("index"));
		if (code.length() == 0)
			continue;

		Catalogue::Source source;
		// The row itself, since that is what the game draws a unique's file
		// index from and what UNI in a rule is written with. Counted over every
		// row of the table, so the dividers and the unreleased rows skipped
		// above still take their numbers up.
		source.fileIndex = i;
		source.code = code;
		source.name = NameOf(code);
		source.baseCode = baseCode;
		source.baseName = ItemDescription::BaseName(baseCode);
		source.requiredLevel = atoi(entry->getString("lvl req").c_str());
		source.rarity = RarityUnique;

		// What makes a search for "amulet" work; the base's name rarely says.
		const ItemDescription::Base* base = ItemDescription::FindBase(baseCode);
		if (base)
			source.itemType = base->typeName;

		source.properties = ReadProperties(entry);
		source.lines = PropertyStats::Lines(source.properties);
		source.modifiers = ItemDescription::ReadModifiers(
			PropertyStats::Totals(source.properties));
		read.push_back(source);
	}
	return read;
}

std::vector<Catalogue::Source> Read(Table& table) {
	// Only the rows flagged enabled; the file keeps unreleased and placeholder
	// items too. A table with nothing flagged falls back to every row, which is
	// what a realm that has dropped the column leaves behind.
	std::vector<Catalogue::Source> read = ReadRows(table, true);
	if (read.empty())
		read = ReadRows(table, false);
	return read;
}

// Nothing is kept until both the table and the string table text are in, so
// that a source is never remembered without the lines it words its properties
// into.
static void Load() {
	if (loaded)
		return;
	if (!StatDescriptions::IsInitialized())
		return;
	// Read as soon as there are rows to read, and once the game says its tables
	// are in whatever they hold, so that a table a realm has emptied leaves the
	// catalogue loaded and empty rather than waiting for rows that never come.
	if (Tables::UniqueItems.size() == 0 && !Tables::isInitialized())
		return;

	sources = Read(Tables::UniqueItems);

	// The facet jewels share a name across several bases, so the base is the
	// tiebreak and equal names still land next to each other.
	std::sort(sources.begin(), sources.end(),
		[](const Catalogue::Source& a, const Catalogue::Source& b) {
			std::string nameA = ToLower(a.name), nameB = ToLower(b.name);
			if (nameA != nameB)
				return nameA < nameB;
			return ToLower(a.baseName) < ToLower(b.baseName);
		});

	// Where two uniques go by the same code the first in the list keeps it,
	// that being the one a walk of the catalogue would have stopped at.
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
