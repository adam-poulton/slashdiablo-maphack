#include "RunewordCatalogue.h"
#include <algorithm>
#include <map>
#include "../ItemDescription.h"
#include "../StatDescriptions.h"
#include "../StringUtil.h"
#include "../TableReader.h"

namespace RunewordCatalogue {

const char* const Kind = "runeword";

// How many runes, allowed types, excluded types and own properties runes.txt
// gives each row.
static const int kRuneCount = 6;
static const int kTypeCount = 6;
static const int kExcludedTypeCount = 3;
static const int kPropertyCount = 7;

// How many rune bonuses gems.txt gives each kind of base.
static const int kGemModCount = 3;

// gems.txt gives every rune three sets of bonuses, which is why the same
// runeword rolls differently depending on what it is made in. The words are the
// prefixes its columns are named by.
static const char* const kKindWeapon = "weapon";
static const char* const kKindHelm = "helm";
static const char* const kKindShield = "shield";

// How far up the Equiv chain a type is followed before the walk is called lost.
// The shipped chain is three deep at most; the bound is what stops a realm's
// table that points a type at itself from being walked for ever.
static const int kEquivDepth = 12;

// Read once and kept, since the panel asks for a source a row at a time and the
// stat index asks for all of them at once. The lookup points into the vector,
// which is not touched again once it is built.
static std::vector<Catalogue::Source> sources;
static std::map<std::string, const Catalogue::Source*> byCode;
static bool loaded = false;

// A recipe the realm enables without shipping it in runes.txt, in a runes.txt
// row's shape so it is read and adds up like the rest. What a rune contributes
// still comes from gems.txt, so only the runeword's own bonuses are listed;
// anything the property tables cannot express goes in lines.
static const int kExtraPropertyCount = 8;
static const int kExtraLineCount = 4;

struct ExtraRuneword {
	const char* name;
	const char* runes[kRuneCount];
	const char* itemType;
	PropertyStats::Property properties[kExtraPropertyCount];
	const char* lines[kExtraLineCount];
};

static const ExtraRuneword kExtraRunewords[] = {
	{
		"Plague", { "r32", "r19", "r22" }, "weap",	// Cham + Fal + Um
		{
			{ "hit-skill",    "Poison Nova",   25,  15 },
			{ "gethit-skill", "Lower Resist",  20,  12 },
			{ "aura",         "Cleansing",     13,  17 },
			{ "allskills",    "",               1,   2 },
			{ "dmg-demon",    "",             260, 380 },
			{ "pierce-pois",  "",              23,  23 },
			{ "dmg-fire",     "",               5,  30 },
		},
		{
			// Per level amounts are held in eighths, which cannot express 0.3%.
			"0.3% Deadly Strike (Based on Character Level)",
		}
	},
};

// The string table key is in "Name" ("Runeword1"), which is preferred so a
// localised client reads correctly. A row the string table has no entry for is
// called what the table calls it, which is what a runeword a realm has added
// goes by.
static std::string NameOf(JSONObject* entry) {
	std::string code = Trim(entry->getString("Name"));
	std::string localized = StatDescriptions::GetString(code);
	if (localized.length() > 0)
		return localized;

	std::string name;
	const char* fields[] = { "Rune Name", "*Rune Name" };
	for (int i = 0; i < 2 && name.length() == 0; i++)
		name = Trim(entry->getString(fields[i]));
	return (name.length() > 0) ? name : code;
}

// Rune codes ("r14") come from runes.txt; what a rune is called comes from the
// string table, the same as any other base item.
static std::string RuneName(const std::string& code) {
	std::string name = ItemDescription::BaseName(code);
	// "El Rune" reads better as just "El" in a recipe list.
	const std::string suffix = " Rune";
	if (name.length() > suffix.length() &&
		name.compare(name.length() - suffix.length(), suffix.length(),
			suffix) == 0) {
		name.erase(name.length() - suffix.length());
	}
	return name;
}

// Which of the three kinds of base a type belongs to, by walking the Equiv chain
// in ItemTypes.txt up to the root categories. A shield is looked for before any
// armour, since the shields hang off "Any Armor" and take rune bonuses of their
// own.
static const char* BaseKindFor(const std::string& code) {
	std::string current = code;
	for (int depth = 0; depth < kEquivDepth && current.length() > 0; depth++) {
		if (current.compare("shld") == 0)
			return kKindShield;
		if (current.compare("weap") == 0)
			return kKindWeapon;
		if (current.compare("armo") == 0 || current.compare("tors") == 0 ||
			current.compare("helm") == 0)
			return kKindHelm;
		JSONObject* entry = Tables::ItemTypes.findEntry("Code", current);
		if (!entry)
			break;
		current = Trim(entry->getString("Equiv1"));
	}
	// What the game does with the leftover types runewords are allowed in.
	return kKindWeapon;
}

// A runeword can be made as soon as its highest rune can be worn, which is what
// the runes ask for as base items.
static int LevelForRunes(const std::vector<std::string>& runeCodes) {
	int level = 0;
	for (unsigned int i = 0; i < runeCodes.size(); i++) {
		const ItemDescription::Base* rune =
			ItemDescription::FindBase(runeCodes[i]);
		if (rune && rune->requirements.level > level)
			level = rune->requirements.level;
	}
	return level;
}

// One variant per distinct kind of base, in the order the types are listed, so
// that a runeword allowed in swords and shields is worded once for each. The
// label is the first type of that kind, which is what a line only that kind
// grants is tagged with.
static void AddVariant(std::vector<Catalogue::Variant>& variants,
		const std::string& typeCode, const std::string& typeName) {
	std::string baseKind = BaseKindFor(typeCode);
	for (unsigned int i = 0; i < variants.size(); i++) {
		if (variants[i].baseKind.compare(baseKind) == 0)
			return;
	}

	Catalogue::Variant variant;
	variant.baseKind = baseKind;
	variant.label = typeName;
	variants.push_back(variant);
}

// What each rune of the runeword adds in one kind of base, on the end of the
// runeword's own bonuses, which is the order the game adds them up in.
static std::vector<PropertyStats::Property> PropertiesForKind(
		const std::vector<PropertyStats::Property>& own,
		const std::vector<std::string>& runeCodes, const std::string& baseKind) {
	std::vector<PropertyStats::Property> properties = own;
	for (unsigned int r = 0; r < runeCodes.size(); r++) {
		JSONObject* gem = Tables::Gems.findEntry("code", runeCodes[r]);
		if (!gem)
			continue;
		for (int n = 1; n <= kGemModCount; n++) {
			std::string prefix = baseKind + "Mod" + std::to_string(n);
			PropertyStats::Property property;
			property.code = Trim(gem->getString(prefix + "Code"));
			if (property.code.length() == 0)
				continue;
			property.param = Trim(gem->getString(prefix + "Param"));
			property.min = atoi(gem->getString(prefix + "Min").c_str());
			property.max = atoi(gem->getString(prefix + "Max").c_str());
			properties.push_back(property);
		}
	}
	return properties;
}

// The lines a player reads: what every kind of base grants, then what only some
// of them do, each tagged with the kind that grants it. The ready made lines
// read the same whatever the base, so they are in every variant and come out in
// the common block.
static std::vector<std::string> MergeVariantLines(
		const std::vector<Catalogue::Variant>& variants) {
	std::vector<std::string> lines;
	for (unsigned int i = 0; i < variants[0].lines.size(); i++) {
		bool everywhere = true;
		for (unsigned int v = 1; v < variants.size() && everywhere; v++) {
			everywhere = std::find(variants[v].lines.begin(),
				variants[v].lines.end(), variants[0].lines[i]) !=
				variants[v].lines.end();
		}
		if (everywhere)
			lines.push_back(variants[0].lines[i]);
	}
	for (unsigned int v = 0; v < variants.size(); v++) {
		for (unsigned int i = 0; i < variants[v].lines.size(); i++) {
			bool common = std::find(lines.begin(), lines.end(),
				variants[v].lines[i]) != lines.end();
			if (common)
				continue;
			lines.push_back(variants[v].lines[i] + "  (" + variants[v].label +
				")");
		}
	}
	return lines;
}

// Words what the runeword grants, once per kind of base it is allowed in, and
// then the one list a player reads. A runeword the table allows in nothing has
// only its own bonuses to word.
static void WordStats(Catalogue::Source& source,
		const std::vector<std::string>& extraLines) {
	for (unsigned int v = 0; v < source.variants.size(); v++) {
		Catalogue::Variant& variant = source.variants[v];
		variant.properties = PropertiesForKind(source.properties,
			source.ingredientCodes, variant.baseKind);
		variant.lines = PropertyStats::Lines(variant.properties, extraLines);
	}

	source.lines = source.variants.empty() ?
		PropertyStats::Lines(source.properties, extraLines) :
		MergeVariantLines(source.variants);
}

static std::vector<Catalogue::Source> ReadRows(Table& table,
		bool requireComplete) {
	std::vector<Catalogue::Source> read;
	for (int i = 0; i < table.size(); i++) {
		JSONObject* entry = table.entryAt(i);
		if (!entry)
			continue;
		if (requireComplete && entry->getString("complete").compare("1") != 0)
			continue;

		Catalogue::Source source;
		source.code = Trim(entry->getString("Name"));
		if (source.code.length() == 0)
			continue;

		// The dividers and the placeholder rows the file is padded out with
		// name no runes, which is what leaves them out of the catalogue.
		std::vector<std::string> runeNames;
		for (int n = 1; n <= kRuneCount; n++) {
			std::string code = Trim(entry->getString("Rune" +
				std::to_string(n)));
			if (code.length() == 0)
				continue;
			source.ingredientCodes.push_back(code);
			runeNames.push_back(RuneName(code));
		}
		if (runeNames.empty())
			continue;

		source.name = NameOf(entry);
		source.ingredients = Join(runeNames, " + ");
		source.requiredLevel = LevelForRunes(source.ingredientCodes);
		source.rarity = RarityRuneword;

		std::vector<std::string> types;
		for (int n = 1; n <= kTypeCount; n++) {
			std::string code = Trim(entry->getString("itype" +
				std::to_string(n)));
			if (code.length() == 0)
				continue;
			types.push_back(ItemDescription::TypeName(code));
			AddVariant(source.variants, code, types.back());
		}
		source.itemType = Join(types, ", ");

		std::vector<std::string> excluded;
		for (int n = 1; n <= kExcludedTypeCount; n++) {
			std::string code = Trim(entry->getString("etype" +
				std::to_string(n)));
			if (code.length() > 0)
				excluded.push_back(ItemDescription::TypeName(code));
		}
		if (!excluded.empty())
			source.itemType += " (not " + Join(excluded, ", ") + ")";

		for (int n = 1; n <= kPropertyCount; n++) {
			std::string suffix = std::to_string(n);
			PropertyStats::Property property;
			property.code = Trim(entry->getString("T1Code" + suffix));
			if (property.code.length() == 0)
				continue;
			property.param = Trim(entry->getString("T1Param" + suffix));
			property.min = atoi(entry->getString("T1Min" + suffix).c_str());
			property.max = atoi(entry->getString("T1Max" + suffix).c_str());
			source.properties.push_back(property);
		}

		WordStats(source, std::vector<std::string>());
		read.push_back(source);
	}
	return read;
}

// The recipes shipped outside the table, read into the same shape and worded the
// same way. A realm that has since added one to its table keeps its own, which
// is what the name comparison leaves alone.
static void AddExtras(std::vector<Catalogue::Source>& read) {
	const int count = sizeof(kExtraRunewords) / sizeof(kExtraRunewords[0]);
	for (int i = 0; i < count; i++) {
		const ExtraRuneword& extra = kExtraRunewords[i];

		bool known = false;
		for (unsigned int n = 0; n < read.size() && !known; n++)
			known = (read[n].name.compare(extra.name) == 0);
		if (known)
			continue;

		Catalogue::Source source;
		source.code = extra.name;
		source.name = extra.name;
		source.rarity = RarityRuneword;

		std::vector<std::string> runeNames;
		for (int n = 0; n < kRuneCount && extra.runes[n]; n++) {
			source.ingredientCodes.push_back(extra.runes[n]);
			runeNames.push_back(RuneName(extra.runes[n]));
		}
		source.ingredients = Join(runeNames, " + ");
		source.requiredLevel = LevelForRunes(source.ingredientCodes);

		source.itemType = ItemDescription::TypeName(extra.itemType);
		AddVariant(source.variants, extra.itemType, source.itemType);

		for (int n = 0; n < kExtraPropertyCount &&
				extra.properties[n].code.length() > 0; n++)
			source.properties.push_back(extra.properties[n]);

		std::vector<std::string> extraLines;
		for (int n = 0; n < kExtraLineCount && extra.lines[n]; n++)
			extraLines.push_back(extra.lines[n]);

		WordStats(source, extraLines);
		read.push_back(source);
	}
}

std::vector<Catalogue::Source> Read(Table& table) {
	// Only the rows flagged complete; the file keeps placeholders too. A table
	// with nothing flagged falls back to every row that names runes, which is
	// what a realm that has dropped the column leaves behind.
	std::vector<Catalogue::Source> read = ReadRows(table, true);
	if (read.empty())
		read = ReadRows(table, false);

	AddExtras(read);

	std::sort(read.begin(), read.end(),
		[](const Catalogue::Source& a, const Catalogue::Source& b) {
			return ToLower(a.name) < ToLower(b.name);
		});
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
	// Once the game says its tables are in, whatever they hold, so that a table
	// a realm has emptied leaves the catalogue loaded with only the recipes
	// shipped outside it rather than waiting for rows that never come. Ahead of
	// that, only when every table a runeword is read out of has rows: what a
	// rune grants is in gems.txt and which kind of base a type is in
	// ItemTypes.txt, and a source built without either would be kept for good
	// missing what its runes add.
	if (!Tables::isInitialized() && (Tables::Runewords.size() == 0 ||
			Tables::Gems.size() == 0 || Tables::ItemTypes.size() == 0))
		return;

	sources = Read(Tables::Runewords);

	// Where two runewords go by the same code the first in the list keeps it,
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
