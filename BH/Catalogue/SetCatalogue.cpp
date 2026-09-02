#include "SetCatalogue.h"
#include <algorithm>
#include <map>
#include "../ItemDescription.h"
#include "../StatDescriptions.h"
#include "../StringUtil.h"
#include "../TableReader.h"

namespace SetCatalogue {

const char* const Kind = "set item";
const char* const BonusKind = "set bonus";

// How many groups of each shape the tables give a row.
static const int kOwnCount = 9;			// prop1-9 on a piece
static const int kPartialCount = 5;		// aprop1a-5b on a piece, PCode2a-5b on a set
static const int kFullCount = 8;		// FCode1-8 on a set

// add func decides whether a piece's aprop entries apply at all. Any other
// value, blank included, means they never do whatever the file lists against
// them.
static const int kAddTogether = 1;		// all of them, as soon as one other piece is worn
static const int kAddProgressive = 2;	// aprop N at N+1 pieces

// Read once and kept, since the panel asks for a source a row at a time and the
// stat index asks for all of them at once. The lookup points into the vector,
// which is not touched again once it is built.
static std::vector<Catalogue::Source> pieces;
static std::vector<Catalogue::Source> bonuses;
static std::map<std::string, const Catalogue::Source*> bonusByCode;
static bool loaded = false;

// "index" doubles as the string table key in both tables. The table is
// preferred because several pieces shipped under a working title the files
// still carry: "Tal Rasha's Fire-Spun Cloth" in the file is "Fine-Spun" in
// game.
static std::string NameOf(const std::string& code) {
	if (code.length() == 0)
		return code;
	std::string localized = StatDescriptions::GetString(code);
	return (localized.length() > 0) ? localized : code;
}

// The four columns one property is spread across. The tables name them
// differently in every group, so what they are called travels together with
// what is read out of them.
//
// low and high rather than min and max: windows.h leaves those defined as
// macros, and a member named either of them does not compile.
struct PropertyColumns {
	std::string code;
	std::string param;
	std::string low;
	std::string high;

	PropertyColumns(const std::string& code, const std::string& param,
			const std::string& low, const std::string& high) :
		code(code), param(param), low(low), high(high) {};

	// The same four with a group's number, and the slot within it where the
	// group has two.
	PropertyColumns At(const std::string& suffix) const {
		return PropertyColumns(code + suffix, param + suffix, low + suffix,
			high + suffix);
	};
};

// Every property group in both tables has this shape; only the columns and the
// piece count differ.
static void ReadProperty(const JSONObject* entry, const PropertyColumns& columns,
		int itemCount, std::vector<PropertyStats::Property>& into) {
	PropertyStats::Property property;
	property.code = Trim(entry->getString(columns.code));
	if (property.code.length() == 0)
		return;
	property.param = Trim(entry->getString(columns.param));
	property.min = atoi(entry->getString(columns.low).c_str());
	property.max = atoi(entry->getString(columns.high).c_str());
	property.itemCount = itemCount;
	into.push_back(property);
}

// The two slots each counted group carries. Only Trang-Oul's uses the second,
// for its three oskills.
static const char* const kSlots[] = { "a", "b" };
static const int kSlotCount = sizeof(kSlots) / sizeof(kSlots[0]);

static void ReadPartial(const JSONObject* entry, const PropertyColumns& columns,
		int index, int itemCount,
		std::vector<PropertyStats::Property>& into) {
	for (int slot = 0; slot < kSlotCount; slot++)
		ReadProperty(entry, columns.At(std::to_string(index) + kSlots[slot]),
			itemCount, into);
}

// What a set is worth wearing all of, and what a piece is worth on its own.
static const PropertyColumns kFullColumns("FCode", "FParam", "FMin", "FMax");
static const PropertyColumns kOwnColumns("prop", "par", "min", "max");

// The two tables name their counted groups differently: a set's are PCode2a
// through PMax5b, a piece's aprop1a through amax5b.
static const PropertyColumns kSetColumns("PCode", "PParam", "PMin", "PMax");
static const PropertyColumns kPieceColumns("aprop", "apar", "amin", "amax");

std::vector<Catalogue::Source> ReadBonuses(Table& table) {
	std::vector<Catalogue::Source> read;
	for (int i = 0; i < table.size(); i++) {
		JSONObject* entry = table.entryAt(i);
		if (!entry)
			continue;

		// The blank row the file ends on names no set.
		std::string code = Trim(entry->getString("index"));
		if (code.length() == 0)
			continue;

		Catalogue::Source source;
		source.code = code;
		source.name = NameOf(code);
		source.rarity = RaritySet;

		// PCodeN unlocks at N pieces.
		for (int count = 2; count <= kPartialCount; count++)
			ReadPartial(entry, kSetColumns, count, count, source.partial);

		for (int n = 1; n <= kFullCount; n++)
			ReadProperty(entry, kFullColumns.At(std::to_string(n)), 0,
				source.properties);

		source.lines = PropertyStats::Lines(source.properties);
		source.partialLines = PropertyStats::CountedLines(source.partial);
		read.push_back(source);
	}
	return read;
}

std::vector<Catalogue::Source> ReadPieces(Table& table) {
	std::vector<Catalogue::Source> read;
	for (int i = 0; i < table.size(); i++) {
		JSONObject* entry = table.entryAt(i);
		if (!entry)
			continue;

		// The dividers the file is padded out with name no base item.
		std::string baseCode = Trim(entry->getString("item"));
		if (baseCode.length() == 0)
			continue;

		std::string code = Trim(entry->getString("index"));
		if (code.length() == 0)
			continue;

		Catalogue::Source source;
		// The row itself, as a unique's is: what the game draws a set item's
		// file index from and what SET in a rule is written with. A set's own
		// bonus is numbered by nothing, since no item is one.
		source.fileIndex = i;
		source.code = code;
		source.name = NameOf(code);
		source.baseCode = baseCode;
		source.baseName = ItemDescription::BaseName(baseCode);
		source.setCode = Trim(entry->getString("set"));
		source.setName = NameOf(source.setCode);
		source.requiredLevel = atoi(entry->getString("lvl req").c_str());
		source.rarity = RaritySet;

		// What makes a search for "amulet" work; the base's name rarely says.
		const ItemDescription::Base* base = ItemDescription::FindBase(baseCode);
		if (base)
			source.itemType = base->typeName;

		for (int n = 1; n <= kOwnCount; n++)
			ReadProperty(entry, kOwnColumns.At(std::to_string(n)), 0,
				source.properties);

		// A blank add func is not merely a piece with no aprops listed: Civerb's
		// Cudgel lists a per level damage bonus the game has never granted it.
		int addFunc = atoi(entry->getString("add func").c_str());
		if (addFunc == kAddTogether || addFunc == kAddProgressive) {
			for (int n = 1; n <= kPartialCount; n++) {
				// apropN counts pieces besides this one, so it unlocks at N + 1.
				int count = (addFunc == kAddProgressive) ? (n + 1) : 2;
				ReadPartial(entry, kPieceColumns, n, count, source.partial);
			}
		}

		source.lines = PropertyStats::Lines(source.properties);
		source.partialLines = PropertyStats::CountedLines(source.partial);
		source.modifiers = ItemDescription::ReadModifiers(
			PropertyStats::Totals(source.properties));
		read.push_back(source);
	}
	return read;
}

// The pieces of one set, in the order the table holds them, which is the game's
// own head to toe order.
static void AppendPieces(const std::vector<Catalogue::Source>& read,
		const std::string& setCode, std::vector<Catalogue::Source>& into) {
	for (unsigned int i = 0; i < read.size(); i++) {
		if (read[i].setCode.compare(setCode) == 0)
			into.push_back(read[i]);
	}
}

// Nothing is kept until both the tables and the string table text are in, so
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
	if (Tables::SetItems.size() == 0 && !Tables::isInitialized())
		return;

	bonuses = ReadBonuses(Tables::Sets);
	std::sort(bonuses.begin(), bonuses.end(),
		[](const Catalogue::Source& a, const Catalogue::Source& b) {
			return ToLower(a.name) < ToLower(b.name);
		});

	// Where two sets go by the same code the first in the list keeps it, that
	// being the one a walk of the catalogue would have stopped at.
	for (unsigned int i = 0; i < bonuses.size(); i++)
		bonusByCode.insert(std::make_pair(bonuses[i].code, &bonuses[i]));

	// Each set's pieces once, taken with whichever bonuses kept the code: a
	// second set going by the same code would otherwise list them all again.
	std::vector<Catalogue::Source> read = ReadPieces(Tables::SetItems);
	for (unsigned int i = 0; i < bonuses.size(); i++) {
		if (bonusByCode[bonuses[i].code] == &bonuses[i])
			AppendPieces(read, bonuses[i].code, pieces);
	}

	// A piece whose set the sets table does not carry still goes in, at the end.
	for (unsigned int i = 0; i < read.size(); i++) {
		if (bonusByCode.find(read[i].setCode) == bonusByCode.end())
			pieces.push_back(read[i]);
	}

	loaded = true;
}

const std::vector<Catalogue::Source>& Pieces() {
	Load();
	return pieces;
}

const std::vector<Catalogue::Source>& Bonuses() {
	Load();
	return bonuses;
}

const Catalogue::Source* FindBonus(const std::string& setCode) {
	Load();
	std::map<std::string, const Catalogue::Source*>::iterator found =
		bonusByCode.find(setCode);
	return (found != bonusByCode.end()) ? found->second : NULL;
}

bool Loaded() {
	Load();
	return loaded;
}

}
