#include "RecipeCatalogue.h"
#include <algorithm>
#include <map>
#include <set>
#include "../ItemDescription.h"
#include "../ItemRarity.h"
#include "../StatDescriptions.h"
#include "../StringUtil.h"
#include "../TableReader.h"

namespace RecipeCatalogue {

const char* const Kind = "recipe";

// How many inputs and how many property entries CubeMain.txt gives each row,
// and how many an affix carries in MagicPrefix.txt and MagicSuffix.txt.
static const int kInputCount = 7;
static const int kPropertyCount = 5;
static const int kAffixPropertyCount = 3;

// What crafting rolls on top of the bonuses a recipe guarantees, and the item
// level at which the roll will always be the maximum.
static const char* const kCraftAffixLine = "+1-4 Random Affixes";
static const int kCraftAffixLevel = 71;

// The cube's own wildcard, standing for any item at all rather than for a base
// item or an item type. Its qualifiers are what narrow it, so it reads as
// "Socketed Item" or "Rare Item". The game never writes it out anywhere, so
// there is no string of its own to read the word from.
static const char* const kAnyItem = "any";

// Outputs that name the first input rather than an item of their own: one
// remade as a new item of the same kind, one kept and altered. Either way the
// result is named after what went in.
static const char* const kOutputType = "usetype";
static const char* const kOutputItem = "useitem";

// The length of one of the tables of words below, none of which is a vector
// because each is a fixed list written out in the file.
template <typename T, int N>
static int Count(const T (&table)[N]) {
	return N;
}

// Read once and kept, since the panel asks for a source a row at a time and the
// stat index asks for all of them at once. The lookup points into the vector,
// which is not touched again once it is built.
static std::vector<Catalogue::Source> sources;
static std::map<std::string, const Catalogue::Source*> byCode;
static bool loaded = false;

// The game's own wording, so a localised client reads correctly. The fallbacks
// are what the English tables say, for a client whose strings we could not read,
// and for the words the game has no string of its own for: an item's quality and
// its tier are only ever shown as a colour and a name in game, never written
// out, so there is nothing to read them from.
static std::string Label(const char* key, const char* fallback) {
	std::string label = StatDescriptions::GetString(key);
	return (label.length() > 0) ? label : fallback;
}

// One input or output cell of CubeMain.txt: an item or item type code followed
// by the qualifiers that narrow it, as `"rin,mag,qty=3"`. The quotes belong to
// the file, which wraps any cell holding a comma, and not to the token.
struct CubeToken {
	std::string code;
	std::vector<std::string> params;

	bool Has(const char* param) const {
		for (unsigned int i = 0; i < params.size(); i++) {
			if (params[i].compare(param) == 0)
				return true;
		}
		return false;
	};

	// The number after a `name=` qualifier, or fallback where the token has none.
	int Number(const char* name, int fallback) const {
		std::string prefix = std::string(name) + "=";
		for (unsigned int i = 0; i < params.size(); i++) {
			if (params[i].compare(0, prefix.length(), prefix) == 0)
				return atoi(params[i].c_str() + prefix.length());
		}
		return fallback;
	};

	bool Empty() const { return code.length() == 0; };
};

static CubeToken ParseToken(const std::string& cell) {
	CubeToken token;
	std::string text = Trim(cell);
	if (text.length() >= 2 && text[0] == '"' && text[text.length() - 1] == '"')
		text = text.substr(1, text.length() - 2);

	size_t start = 0;
	while (start <= text.length()) {
		size_t comma = text.find(',', start);
		size_t end = (comma == std::string::npos) ? text.length() : comma;
		std::string piece = Trim(text.substr(start, end - start));
		if (piece.length() > 0) {
			if (token.code.length() == 0)
				token.code = piece;
			else
				token.params.push_back(ToLower(piece));
		}
		if (comma == std::string::npos)
			break;
		start = comma + 1;
	}
	return token;
}

// A qualifier and the word that says what it means.
struct CubeWord {
	const char* param;
	const char* key;		// string table key, empty where the game has none
	const char* fallback;
};

// The state an item has to be in, or comes out in. Read before the tier and the
// quality, since that is the order the words read in.
static const CubeWord kFlags[] = {
	{ "sock", "Socketable", "Socketed" },
	{ "nos",  "",           "Unsocketed" },
	{ "eth",  "",           "Ethereal" },
	{ "noe",  "",           "Non-Ethereal" },
};

// A base item's tier. `bas` is the basic tier and is left unsaid, since a base
// item's own name is already the basic one.
static const CubeWord kTiers[] = {
	{ "exc", "", "Exceptional" },
	{ "eli", "", "Elite" },
};

// An item's quality, which is also what colour it is drawn in.
struct CubeQuality {
	CubeWord word;
	ItemRarity rarity;
};

static const CubeQuality kQualities[] = {
	{ { "low", "Low Quality", "Low Quality" }, RarityInferior },
	{ { "nor", "",            "Normal"      }, RarityNormal },
	{ { "hiq", "Hiquality",   "Superior"    }, RaritySuperior },
	{ { "mag", "strBSMagic",  "Magic"       }, RarityMagic },
	{ { "set", "",            "Set"         }, RaritySet },
	{ { "rar", "",            "Rare"        }, RarityRare },
	{ { "uni", "",            "Unique"      }, RarityUnique },
	{ { "crf", "",            "Crafted"     }, RarityCrafted },
};

// What an output does to the item that went in, beyond remaking it. Each is a
// state the result comes out in, so each reads as a word in front of its name.
static const CubeWord kOutputFlags[] = {
	{ "rep", "", "Repaired" },
	{ "uns", "", "Unsocketed" },
};

// The recipes that open a portal rather than make an item. CubeMain.txt names
// their output outright; two of them lead somewhere the game has a name for,
// which is the name it writes on the portal itself.
struct CubePortal {
	const char* output;
	const char* key;
	const char* fallback;
};

static const CubePortal kPortals[] = {
	{ "Cow Portal", "To The Moo Moo Farm", "To The Secret Cow Level" },
	{ "Pandemonium Finale Portal", "To Tristram", "To Tristram" },
};

// The families of crafted item. CubeMain.txt gives them no name of their own,
// since the game never shows one - a crafted item is named by the affixes it
// happens to roll - but the recipes are known by their family, so it is read out
// of the description the row carries, which is the only place the row says which
// family it belongs to. A family not named here reads as a crafted item, which
// is what every crafted recipe read as before any of them were named.
struct CubeCraftFamily {
	const char* marker;		// as the description writes it, in lower case
	const char* name;
};

static const CubeCraftFamily kCraftFamilies[] = {
	{ "hitpower", "Hit Power" },
	{ "blood",    "Blood" },
	{ "caster",   "Caster" },
	{ "safety",   "Safety" },
};

static std::string CraftFamily(const std::string& description) {
	std::string text = ToLower(description);
	for (int i = 0; i < Count(kCraftFamilies); i++) {
		if (text.find(kCraftFamilies[i].marker) != std::string::npos)
			return kCraftFamilies[i].name;
	}
	return "";
}

// The item types worth gathering recipes under, nearest first. Each is a real
// ItemTypes.txt row, so a heading is whatever the realm calls that type and a
// type the realm adds is gathered under whichever of these it descends from.
static const char* kHeadingTypes[] = {
	"gem", "rune", "jewl", "poti", "misl", "ring", "amul", "char", "scro",
	"book", "ques", "weap", "armo",
};

// The two heading types the catalogue has something of its own to say about: a
// rune is drawn in the colour the game gives a rune rather than as a plain item,
// and a quest item is gathered under the quest recipes rather than under its
// type.
static const char* const kTypeRune = "rune";
static const char* const kTypeQuest = "ques";

// Headings for what a recipe does, where that says more than what it makes
// does: every socket recipe makes a different item, but they are all one recipe
// to anyone looking for one. The game writes none of these anywhere, so there
// is no string of its own to read them from.
static const char* const kHeadingCrafting = "Crafting";
static const char* const kHeadingSockets = "Sockets";
static const char* const kHeadingRepairing = "Repairing";
static const char* const kHeadingUpgrading = "Upgrading";
static const char* const kHeadingRerolling = "Rerolling";
static const char* const kHeadingQuest = "Quest";

// The nearest heading type at or above a code's own item type. Both Equiv
// columns are followed, since a type can descend from two - a shield is armour
// and a second hand - and the nearer of them wins.
static std::string HeadingTypeFor(const std::string& code) {
	std::vector<std::string> level;
	const ItemDescription::Base* base = ItemDescription::FindBase(code);
	if (base)
		level.push_back(base->type);
	else if (Tables::ItemTypes.findEntry("Code", code))
		level.push_back(code);

	std::set<std::string> seen(level.begin(), level.end());
	while (!level.empty()) {
		for (unsigned int i = 0; i < level.size(); i++) {
			for (int n = 0; n < Count(kHeadingTypes); n++) {
				if (level[i].compare(kHeadingTypes[n]) == 0)
					return level[i];
			}
		}

		std::vector<std::string> parents;
		for (unsigned int i = 0; i < level.size(); i++) {
			JSONObject* entry = Tables::ItemTypes.findEntry("Code", level[i]);
			if (!entry)
				continue;
			const char* columns[] = { "Equiv1", "Equiv2" };
			for (int c = 0; c < 2; c++) {
				std::string parent = Trim(entry->getString(columns[c]));
				// seen also stops a table whose Equiv chain loops back on itself.
				if (parent.length() > 0 && seen.insert(parent).second)
					parents.push_back(parent);
			}
		}
		level = parents;
	}
	return "";
}

// What a cell's code names. Base items are looked up before item types, since a
// few codes are both: "rin" is a ring and "ring" is the type of all rings.
static std::string CodeName(const std::string& code) {
	if (code.compare(kAnyItem) == 0)
		return "Item";

	const ItemDescription::Base* base = ItemDescription::FindBase(code);
	if (base && base->name.length() > 0)
		return base->name;
	if (Tables::ItemTypes.findEntry("Code", code))
		return ItemDescription::TypeName(code);

	// A quest item is named outright rather than by its code, and the name it is
	// given is its own string table key. So are the portals, once the two the
	// game has a name for are asked for under that name.
	for (int i = 0; i < Count(kPortals); i++) {
		if (code.compare(kPortals[i].output) == 0)
			return Label(kPortals[i].key, kPortals[i].fallback);
	}
	std::string localized = ItemDescription::NameLine(
		StatDescriptions::GetString(code));
	return (localized.length() > 0) ? localized : code;
}

// A forced prefix or suffix, by the row it sits on in its table.
// CubeMain.txt names it by row rather than by name.
// What an affix grants is on the same row, so it is collected here.
static std::string ReadAffix(Table& table, int row,
		std::vector<PropertyStats::Property>& properties) {
	JSONObject* entry = table.entryAt(row);
	if (!entry)
		return "";

	for (int n = 1; n <= kAffixPropertyCount; n++) {
		std::string index = std::to_string(n);
		PropertyStats::Property property;
		property.code = ToLower(Trim(entry->getString("mod" + index + "code")));
		if (property.code.length() == 0)
			continue;
		property.param = Trim(entry->getString("mod" + index + "param"));
		property.min = atoi(entry->getString("mod" + index + "min").c_str());
		property.max = atoi(entry->getString("mod" + index + "max").c_str());
		properties.push_back(property);
	}

	std::string name = Trim(entry->getString("Name"));
	std::string localized = StatDescriptions::GetString(name);
	return (localized.length() > 0) ? localized : name;
}

static void AppendWords(const CubeToken& token, const CubeWord* words, int count,
		std::vector<std::string>& out) {
	for (int i = 0; i < count; i++) {
		if (token.Has(words[i].param))
			out.push_back(Label(words[i].key, words[i].fallback));
	}
}

// The quality a token names, or none where it names no quality at all.
static const CubeQuality* FindQuality(const CubeToken& token) {
	for (int i = 0; i < Count(kQualities); i++) {
		if (token.Has(kQualities[i].word.param))
			return &kQualities[i];
	}
	return NULL;
}

// "3 Socketed Magic Weapon": how many are wanted, the state they have to be in,
// and what the game calls them. The count is left out where only one is wanted,
// so a recipe reads the way the runewords panel reads its runes.
static std::string InputPhrase(const CubeToken& token) {
	std::vector<std::string> words;
	AppendWords(token, kFlags, Count(kFlags), words);
	AppendWords(token, kTiers, Count(kTiers), words);
	const CubeQuality* quality = FindQuality(token);
	if (quality)
		words.push_back(Label(quality->word.key, quality->word.fallback));
	words.push_back(CodeName(token.code));

	std::string phrase = Join(words, " ");
	int quantity = token.Number("qty", 1);
	if (quantity > 1)
		phrase = std::to_string(quantity) + " " + phrase;
	return phrase;
}

// Which heading a recipe belongs under. CubeMain.txt has no column saying so,
// so it is read out of the row: first what the recipe does to the item, which is
// what the recipes under one heading have in common, and failing that what kind
// of item it makes.
static std::string HeadingFor(const CubeToken& output, const CubeToken& named,
		const std::string& headingType, const CubeQuality* made,
		const std::string& family, bool socketed) {
	if (made && made->rarity == RarityCrafted)
		return (family.length() > 0) ? family : kHeadingCrafting;
	if (socketed || output.Has("uns"))
		return kHeadingSockets;
	if (output.Has("rep") || output.Has("rch"))
		return kHeadingRepairing;
	if (output.Has("mod") && (output.Has("exc") || output.Has("eli")))
		return kHeadingUpgrading;

	const ItemDescription::Base* base = ItemDescription::FindBase(named.code);
	if (base && base->quest > 0)
		return kHeadingQuest;

	if (headingType.length() > 0) {
		return (headingType.compare(kTypeQuest) == 0) ? kHeadingQuest :
			ItemDescription::TypeName(headingType);
	}
	// A recipe that names no item at all makes none: it opens a portal.
	return (named.code.compare(kAnyItem) == 0) ?
		kHeadingRerolling : kHeadingQuest;
}

// The sockets a recipe adds and the levels it costs are held as properties, but
// the game gives neither a stat line of its own: it shows them in the shape of
// the item instead. So they are read out here and said in words rather than
// left to StatDescriptions, which would drop them.
static bool ReadStructuralProperty(const PropertyStats::Property& property,
		Catalogue::Source& recipe, bool& socketed) {
	if (property.code.compare("sock") == 0) {
		std::string sockets = std::to_string(property.min);
		if (property.max > property.min) {
			sockets += " " + Label("ItemStast1k", "to") + " " +
				std::to_string(property.max);
		}
		recipe.notes.push_back(Label("Socketable", "Socketed") + " (" + sockets + ")");
		socketed = true;
		return true;
	}
	if (property.code.compare("levelreq") == 0) {
		recipe.notes.push_back(Label("ItemStats1p", "Required Level:") + " +" +
			std::to_string(property.max));
		return true;
	}
	return false;
}

// One CubeMain row, or false where the row is one of the dividers and unfinished
// entries the file is padded out with: it makes nothing, or nothing goes into it.
static bool ReadRow(JSONObject* entry, Table& prefixes, Table& suffixes,
		Catalogue::Source& recipe) {
	CubeToken output = ParseToken(entry->getString("output"));
	if (output.Empty())
		return false;

	std::vector<CubeToken> inputs;
	for (int n = 1; n <= kInputCount; n++) {
		CubeToken input = ParseToken(
			entry->getString("input " + std::to_string(n)));
		if (!input.Empty())
			inputs.push_back(input);
	}
	if (inputs.empty())
		return false;

	// The row's own description, which is the only name CubeMain.txt gives it.
	recipe.code = Trim(entry->getString("description"));

	bool socketed = false;		// whether the result comes out socketed

	std::vector<std::string> ingredients;
	for (unsigned int n = 0; n < inputs.size(); n++)
		ingredients.push_back(InputPhrase(inputs[n]));
	recipe.ingredients = Join(ingredients, " + ");

	for (int n = 1; n <= kPropertyCount; n++) {
		std::string index = std::to_string(n);
		PropertyStats::Property property;
		property.code = ToLower(Trim(entry->getString("mod " + index)));
		if (property.code.length() == 0)
			continue;
		property.param = Trim(entry->getString("mod " + index + " param"));
		property.min = atoi(entry->getString("mod " + index + " min").c_str());
		property.max = atoi(entry->getString("mod " + index + " max").c_str());
		if (!ReadStructuralProperty(property, recipe, socketed))
			recipe.properties.push_back(property);
	}

	// An output that names the first input takes its tier and its quality from
	// it as well, since it is that item that comes back out. Nothing else
	// carries over: what state the input had to be in is a condition on the
	// recipe, not a promise about the result.
	bool fromInput = (output.code.compare(kOutputType) == 0 ||
		output.code.compare(kOutputItem) == 0);
	const CubeToken& named = fromInput ? inputs[0] : output;

	// A recipe can also add sockets through a qualifier on the output rather
	// than through a bonus, which comes to the same thing.
	int forcedSockets = output.Number("sock", 0);
	if (forcedSockets > 0) {
		PropertyStats::Property sockets = { "sock", "", forcedSockets, forcedSockets };
		ReadStructuralProperty(sockets, recipe, socketed);
	}

	std::vector<std::string> words;
	AppendWords(output, kOutputFlags, Count(kOutputFlags), words);
	if (socketed)
		words.push_back(Label("Socketable", "Socketed"));

	AppendWords(output.Has("exc") || output.Has("eli") ? output : named,
		kTiers, Count(kTiers), words);

	// A crafted result is one the output itself says is crafted; a quality the
	// result only inherits was the input's already, and inheriting it does not
	// make the recipe a crafting recipe.
	const CubeQuality* made = FindQuality(output);
	const CubeQuality* quality = made ? made : FindQuality(named);

	// An affix or a craft family names the result outright, so the quality word
	// would only repeat what the name already says; the colour still carries it.
	std::string prefix = ReadAffix(prefixes, output.Number("pre", -1),
		recipe.properties);
	std::string suffix = ReadAffix(suffixes, output.Number("suf", -1),
		recipe.properties);
	std::string family;
	if (made && made->rarity == RarityCrafted)
		family = CraftFamily(entry->getString("description"));
	if (quality && prefix.length() == 0 && suffix.length() == 0 &&
			family.length() == 0) {
		words.push_back(Label(quality->word.key, quality->word.fallback));
	}
	// A rune carries no quality, but the game still gives its name a colour of
	// its own, which is the one BH draws a rune in everywhere.
	std::string headingType = HeadingTypeFor(named.code);
	if (quality)
		recipe.rarity = quality->rarity;
	else if (headingType.compare(kTypeRune) == 0)
		recipe.rarity = RarityRune;

	if (family.length() > 0)
		words.push_back(family);
	if (prefix.length() > 0)
		words.push_back(prefix);
	words.push_back(CodeName(named.code));
	if (suffix.length() > 0)
		words.push_back(suffix);
	recipe.name = Join(words, " ");

	// A crafted item rolls rare affix bonuses of its own on top of the ones the
	// recipe guarantees. They arrive as ready made text because no property
	// entry describes them, and they are worded after the described ones.
	bool crafted = (made && made->rarity == RarityCrafted);
	std::vector<std::string> extraLines;
	if (crafted)
		extraLines.push_back(kCraftAffixLine);

	// An input allowed at any tier is allowed as its exceptional and its elite
	// version too, which is not something its own name says.
	for (unsigned int n = 0; n < inputs.size(); n++) {
		if (!inputs[n].Has("upg"))
			continue;

		std::vector<std::string> upgrades;
		const ItemDescription::Base* base =
			ItemDescription::FindBase(inputs[n].code);
		if (base) {
			const std::string codes[] = { base->exceptional, base->elite };
			for (int c = 0; c < 2; c++) {
				if (codes[c].length() > 0)
					upgrades.push_back(ItemDescription::BaseName(codes[c]));
			}
		}

		// A base whose table names no upgrade still allows one, so the recipe
		// says as much without being able to say which.
		if (upgrades.empty())
			recipe.notes.push_back("The exceptional and elite bases also work");
		else
			recipe.notes.push_back("Alt bases: " + Join(upgrades, ", "));
		break;
	}

	// The item level of the result, which is what a recipe making a magic or a
	// rare item is really choosing: it decides which bonuses the item can roll.
	// The game calculates as a flat amount plus a share of the character's level
	// and a share of the level of the input item
	std::vector<std::string> levels;
	int flatLevel = atoi(entry->getString("lvl").c_str());
	int playerShare = atoi(entry->getString("plvl").c_str());
	int itemShare = atoi(entry->getString("ilvl").c_str());
	if (flatLevel > 0)
		levels.push_back(std::to_string(flatLevel));
	if (playerShare > 0)
		levels.push_back(std::to_string(playerShare) + "% clvl");
	if (itemShare > 0)
		levels.push_back(std::to_string(itemShare) + "% ilvl");
	if (!levels.empty())
		recipe.notes.push_back("Craft ilvl: " + Join(levels, " + "));

	// The item level is also what decides how many of the crafted affixes are
	// rolled, which is why this reads under it.
	if (crafted) {
		recipe.notes.push_back("Always 4 affixes at craft ilvl " +
			std::to_string(kCraftAffixLevel));
	}

	if (output.Has("rch"))
		recipe.notes.push_back("Charges are recharged");
	if (output.Has("uns"))
		recipe.notes.push_back("The socketted items are destroyed");

	// A row can make more than one thing. Only the first names the recipe; the
	// rest are said in a note, so nothing is dropped.
	std::vector<std::string> extras;
	const char* extraColumns[] = { "output b", "output c" };
	for (int n = 0; n < Count(extraColumns); n++) {
		CubeToken extra = ParseToken(entry->getString(extraColumns[n]));
		if (!extra.Empty())
			extras.push_back(CodeName(extra.code));
	}
	if (!extras.empty())
		recipe.notes.push_back("Also makes " + Join(extras, " and "));

	if (entry->getString("ladder").compare("1") == 0)
		recipe.notes.push_back("Ladder only");
	int difficulty = atoi(entry->getString("min diff").c_str());
	if (difficulty == 1)
		recipe.notes.push_back("Nightmare or Hell only");
	else if (difficulty >= 2)
		recipe.notes.push_back("Hell only");

	recipe.heading = HeadingFor(output, named, headingType, made, family, socketed);
	recipe.lines = PropertyStats::Lines(recipe.properties, extraLines);
	return true;
}

static std::vector<Catalogue::Source> ReadRows(Table& table, Table& prefixes,
		Table& suffixes, bool requireEnabled) {
	std::vector<Catalogue::Source> read;
	for (int i = 0; i < table.size(); i++) {
		JSONObject* entry = table.entryAt(i);
		if (!entry)
			continue;
		if (requireEnabled && entry->getString("enabled").compare("1") != 0)
			continue;

		Catalogue::Source recipe;
		if (ReadRow(entry, prefixes, suffixes, recipe))
			read.push_back(recipe);
	}
	return read;
}

std::vector<Catalogue::Source> Read(Table& recipes, Table& prefixes,
		Table& suffixes) {
	// Only the rows flagged enabled; the file keeps unfinished and placeholder
	// recipes too. A table with nothing flagged falls back to every row, which
	// is what a realm that has dropped the column leaves behind.
	std::vector<Catalogue::Source> read = ReadRows(recipes, prefixes, suffixes, true);
	if (read.empty())
		read = ReadRows(recipes, prefixes, suffixes, false);

	// CubeMain.txt reaches the same kind of recipe at several points, so the
	// headings are gathered up: each keeps the place the file first reaches it,
	// and the recipes under it keep their own order.
	std::map<std::string, int> place;
	for (unsigned int i = 0; i < read.size(); i++) {
		if (place.find(read[i].heading) == place.end()) {
			int next = (int)place.size();
			place[read[i].heading] = next;
		}
	}
	std::stable_sort(read.begin(), read.end(),
		[&place](const Catalogue::Source& a, const Catalogue::Source& b) {
			return place[a.heading] < place[b.heading];
		});
	return read;
}

// Nothing is kept until both the tables and the string table text are in, so
// that a recipe is never remembered without the words it is read into.
static void Load() {
	if (loaded)
		return;
	if (!StatDescriptions::IsInitialized())
		return;
	// Read as soon as there are rows to read, and once the game says its tables
	// are in whatever they hold, so that a table a realm has emptied leaves the
	// catalogue loaded and empty rather than waiting for rows that never come.
	if (Tables::CubeMain.size() == 0 && !Tables::isInitialized())
		return;

	sources = Read(Tables::CubeMain, Tables::MagicPrefix, Tables::MagicSuffix);

	// Where two recipes go by the same description the first in the list keeps
	// it, that being the one a walk of the catalogue would have stopped at. A
	// row a realm has left undescribed is reachable through the list but not by
	// code, since the empty string names every such row rather than one.
	for (unsigned int i = 0; i < sources.size(); i++) {
		if (sources[i].code.length() > 0)
			byCode.insert(std::make_pair(sources[i].code, &sources[i]));
	}

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
