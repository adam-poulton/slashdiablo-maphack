#include "RecipeTab.h"
#include <algorithm>
#include <map>
#include <set>
#include "../../BH.h"
#include "../../Common.h"
#include "../../ItemDescription.h"
#include "../../ItemRarity.h"
#include "../../StatDescriptions.h"
#include "../../TableReader.h"
#include "InfoText.h"

using namespace Drawing;
using namespace InfoText;

// Margins and the gaps between the three bands. Widths and the list height are
// measured from the tab by ApplyLayout().
#define RC_MARGIN			6	// down either side, and below the status line
#define RC_SEARCH_Y			3
#define RC_SEARCH_GAP		7	// between the search box and the list
#define RC_FOOTER_GAP		6	// between the list and the status line
#define RC_FOOTER_HEIGHT	8	// the status line itself

// What a recipe makes and what it is made from read as one line, parted by this
// rather than by a column of clear space, so a short result leaves its
// ingredients room instead of leaving a gap. The clear space in front of it is
// the ingredients column's own gap.
#define RC_SEPARATOR		"= "
#define RC_COL_GAP			5

// How many inputs and how many output bonuses CubeMain.txt gives each row.
#define RC_INPUT_COUNT		7
#define RC_MOD_COUNT		5

// The cube's own wildcard, standing for any item at all rather than for a base
// item or an item type. Its qualifiers are what narrow it, so it reads as
// "Socketed Item" or "Rare Item". The game never writes it out anywhere, so
// there is no string of its own to read the word from.
#define RC_ANY_ITEM			"any"

// Outputs that name the first input rather than an item of their own: one
// remade as a new item of the same kind, one kept and altered. Either way the
// result is named after what went in.
#define RC_OUTPUT_TYPE		"usetype"
#define RC_OUTPUT_ITEM		"useitem"

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
	for (int i = 0; i < (int)(sizeof(kCraftFamilies) / sizeof(kCraftFamilies[0])); i++) {
		if (text.find(kCraftFamilies[i].marker) != std::string::npos)
			return kCraftFamilies[i].name;
	}
	return "";
}

// The item types worth gathering recipes under, nearest first. Each is a real
// ItemTypes.txt row, so a heading is whatever the realm calls that type and a
// type the realm adds is gathered under whichever of these it descends from.
static const char* kGroupTypes[] = {
	"gem", "rune", "jewl", "poti", "misl", "ring", "amul", "char", "scro",
	"book", "ques", "weap", "armo",
};

// The two grouping types the tab has something of its own to say about: a rune
// is drawn in the colour the game gives a rune rather than as a plain item, and
// a quest item is gathered under the quest recipes rather than under its type.
#define RC_TYPE_RUNE		"rune"
#define RC_TYPE_QUEST		"ques"

// Headings for what a recipe does, where that says more than what it makes
// does: every socket recipe makes a different item, but they are all one recipe
// to anyone looking for one. The game writes none of these anywhere, so there
// is no string of its own to read them from.
#define RC_GROUP_CRAFTING	"Crafting"
#define RC_GROUP_SOCKETS	"Sockets"
#define RC_GROUP_REPAIRING	"Repairing"
#define RC_GROUP_UPGRADING	"Upgrading"
#define RC_GROUP_REROLLING	"Rerolling"
#define RC_GROUP_QUEST		"Quest"

// The nearest grouping type at or above a code's own item type. Both Equiv
// columns are followed, since a type can descend from two - a shield is armour
// and a second hand - and the nearer of them wins.
static std::string GroupTypeFor(const std::string& code) {
	std::vector<std::string> level;
	const ItemDescription::Base* base = ItemDescription::FindBase(code);
	if (base)
		level.push_back(base->type);
	else if (Tables::ItemTypes.findEntry("Code", code))
		level.push_back(code);

	std::set<std::string> seen(level.begin(), level.end());
	while (!level.empty()) {
		for (unsigned int i = 0; i < level.size(); i++) {
			for (int n = 0; n < (int)(sizeof(kGroupTypes) / sizeof(kGroupTypes[0])); n++) {
				if (level[i].compare(kGroupTypes[n]) == 0)
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
	if (code.compare(RC_ANY_ITEM) == 0)
		return "Item";

	const ItemDescription::Base* base = ItemDescription::FindBase(code);
	if (base && base->name.length() > 0)
		return base->name;
	if (Tables::ItemTypes.findEntry("Code", code))
		return ItemDescription::TypeName(code);

	// A quest item is named outright rather than by its code, and the name it is
	// given is its own string table key. So are the portals, once the two the
	// game has a name for are asked for under that name.
	for (int i = 0; i < (int)(sizeof(kPortals) / sizeof(kPortals[0])); i++) {
		if (code.compare(kPortals[i].output) == 0)
			return Label(kPortals[i].key, kPortals[i].fallback);
	}
	std::string localized = ItemDescription::NameLine(
		StatDescriptions::GetString(code));
	return (localized.length() > 0) ? localized : code;
}

// The colour an item's name is drawn in, with gold wherever the game gives it no
// colour of its own. A plain name is white in game, but so is everything else in
// the panel around it, and gold is what keeps a name reading as one.
static TextColor NameColor(ItemRarity rarity) {
	TextColor color = RarityColor(rarity);
	return (color == White) ? Gold : color;
}

// A forced prefix or suffix, by the row it sits on in its table. CubeMain.txt
// names it by row rather than by name, so a realm that reorders its affixes
// moves what a recipe makes with it.
static std::string AffixName(Table& table, int row) {
	JSONObject* entry = table.entryAt(row);
	if (!entry)
		return "";
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
	for (int i = 0; i < (int)(sizeof(kQualities) / sizeof(kQualities[0])); i++) {
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
	AppendWords(token, kFlags, (int)(sizeof(kFlags) / sizeof(kFlags[0])), words);
	AppendWords(token, kTiers, (int)(sizeof(kTiers) / sizeof(kTiers[0])), words);
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
// what the recipes anyone hunts for as a group have in common, and failing that
// what kind of item it makes, which gathers the gem and rune chains up.
static std::string RecipeGroup(const CubeToken& output, const CubeToken& named,
		const std::string& madeType, const CubeQuality* made,
		const std::string& family, bool socketed) {
	if (made && made->rarity == RarityCrafted)
		return (family.length() > 0) ? family : RC_GROUP_CRAFTING;
	if (socketed || output.Has("uns"))
		return RC_GROUP_SOCKETS;
	if (output.Has("rep") || output.Has("rch"))
		return RC_GROUP_REPAIRING;
	if (output.Has("mod") && (output.Has("exc") || output.Has("eli")))
		return RC_GROUP_UPGRADING;

	const ItemDescription::Base* base = ItemDescription::FindBase(named.code);
	if (base && base->quest > 0)
		return RC_GROUP_QUEST;

	if (madeType.length() > 0) {
		return (madeType.compare(RC_TYPE_QUEST) == 0) ? RC_GROUP_QUEST :
			ItemDescription::TypeName(madeType);
	}
	// A recipe that names no item at all makes none: it opens a portal, which
	// the quest recipes are what do.
	return (named.code.compare(RC_ANY_ITEM) == 0) ?
		RC_GROUP_REROLLING : RC_GROUP_QUEST;
}

RecipeTab::RecipeTab(UI* ui) : InfoTab("Recipes", ui),
	shownGroups(0),
	foldOnPush(true),
	shownSummary(-1),
	recipesLoaded(false),
	needsRefresh(true) {

	searchBox = new Inputhook(tab, RC_MARGIN, RC_SEARCH_Y, 0, "");
	searchBox->SetPlaceholder("Search by what a recipe makes or what it takes");
	searchBox->SetClearOnFocus(true);

	list = new Listhook(tab, RC_MARGIN, 0, 0, 0);
	// One line in two colours: what the recipe makes, in the colour of the item
	// it makes, and then what it is made from flowing straight on from it. The
	// ingredients are the end of the line and so the end that is cut, which
	// leaves the result whole however narrow the window is drawn.
	//
	// A recipe makes whatever quality of item it makes, so the result's colour
	// comes from the row rather than from the column. Both columns still lift to
	// white under the mouse, as every other list's do.
	std::vector<ListColumn> columns;
	columns.push_back(ListColumn("", 0, 1, 0, Gold, White));
	columns.push_back(ListColumn("", 0, 0, RC_COL_GAP, Grey, White, true));
	list->SetColumns(columns);
	list->SetGroupColor(Gold);

	statusText = new Texthook(tab, RC_MARGIN, 0, "");
	statusText->SetColor(Grey);

	// Placed and switched on by UpdateSummary().
	summary = new Tooltiphook(InGame, 0, 0);
	summary->SetActive(false);

	ApplyLayout();
}

// The list takes whatever height is left between the search box and the status
// line, so a resize needs nothing but this.
void RecipeTab::ApplyLayout() {
	laidOutWidth = tab->GetXSize();
	laidOutHeight = tab->GetYSize();

	unsigned int contentWidth = (laidOutWidth > 2 * RC_MARGIN) ?
		(laidOutWidth - (2 * RC_MARGIN)) : 0;

	// Measured off the box rather than guessed, since its height follows its font.
	unsigned int listY = RC_SEARCH_Y + searchBox->GetYSize() + RC_SEARCH_GAP;
	unsigned int footerBand = RC_FOOTER_GAP + RC_FOOTER_HEIGHT + RC_MARGIN;
	unsigned int listHeight = (laidOutHeight > listY + footerBand) ?
		(laidOutHeight - listY - footerBand) : 0;

	searchBox->SetXSize(contentWidth);
	list->SetBaseY(listY);
	list->SetSize(contentWidth, listHeight);
	statusText->SetBaseY(listY + listHeight + RC_FOOTER_GAP);
	summary->SetMaxWidth(contentWidth);
}

void RecipeTab::MpqLoaded() {
	StatDescriptions::Initialize();
	BuildRecipes();
}

bool RecipeTab::HandlesCommand(const std::string& command) {
	return command.compare("cube") == 0 || command.compare("recipes") == 0;
}

// The sockets a recipe adds and the levels it costs are held as properties, but
// the game gives neither a stat line of its own: it shows them in the shape of
// the item instead. So they are read out here and said in words rather than
// left to StatDescriptions, which would drop them.
static bool ReadStructuralMod(const CubeProperty& property, CubeRecipe& recipe,
		bool& socketed) {
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

void RecipeTab::BuildRecipes() {
	recipes.clear();
	matches.clear();

	// Only the rows flagged enabled; the file keeps unfinished and placeholder
	// recipes too. A modified file with nothing flagged falls back to every row
	// that names an output.
	for (int pass = 0; pass < 2 && recipes.empty(); pass++) {
		bool requireEnabled = (pass == 0);
		for (int i = 0; i < Tables::CubeMain.size(); i++) {
			JSONObject* entry = Tables::CubeMain.entryAt(i);
			if (!entry)
				continue;
			if (requireEnabled && entry->getString("enabled").compare("1") != 0)
				continue;

			CubeToken output = ParseToken(entry->getString("output"));
			if (output.Empty())
				continue;

			std::vector<CubeToken> inputs;
			for (int n = 1; n <= RC_INPUT_COUNT; n++) {
				CubeToken input = ParseToken(
					entry->getString("input " + std::to_string(n)));
				if (!input.Empty())
					inputs.push_back(input);
			}
			if (inputs.empty())
				continue;

			CubeRecipe recipe;
			bool socketed = false;		// whether the result comes out socketed

			std::vector<std::string> ingredients;
			for (unsigned int n = 0; n < inputs.size(); n++)
				ingredients.push_back(InputPhrase(inputs[n]));
			recipe.ingredients = Join(ingredients, " + ");

			for (int n = 1; n <= RC_MOD_COUNT; n++) {
				std::string index = std::to_string(n);
				CubeProperty property;
				property.code = ToLower(Trim(entry->getString("mod " + index)));
				if (property.code.length() == 0)
					continue;
				property.param = Trim(entry->getString("mod " + index + " param"));
				property.min = atoi(entry->getString("mod " + index + " min").c_str());
				property.max = atoi(entry->getString("mod " + index + " max").c_str());
				if (!ReadStructuralMod(property, recipe, socketed))
					recipe.properties.push_back(property);
			}

			// An output that names the first input takes its tier and its quality
			// from it as well, since it is that item that comes back out. Nothing
			// else carries over: what state the input had to be in is a condition
			// on the recipe, not a promise about the result.
			bool fromInput = (output.code.compare(RC_OUTPUT_TYPE) == 0 ||
				output.code.compare(RC_OUTPUT_ITEM) == 0);
			const CubeToken& named = fromInput ? inputs[0] : output;

			// A recipe can also add sockets through a qualifier on the output
			// rather than through a bonus, which comes to the same thing.
			int forcedSockets = output.Number("sock", 0);
			if (forcedSockets > 0) {
				CubeProperty sockets = { "sock", "", forcedSockets, forcedSockets };
				ReadStructuralMod(sockets, recipe, socketed);
			}

			std::vector<std::string> words;
			AppendWords(output, kOutputFlags, (int)(sizeof(kOutputFlags) / sizeof(kOutputFlags[0])), words);
			if (socketed)
				words.push_back(Label("Socketable", "Socketed"));

			AppendWords(output.Has("exc") || output.Has("eli") ? output : named,
				kTiers, (int)(sizeof(kTiers) / sizeof(kTiers[0])), words);

			// A crafted result is one the output itself says is crafted; a quality
			// the result only inherits was the input's already, and inheriting it
			// does not make the recipe a crafting recipe.
			const CubeQuality* made = FindQuality(output);
			const CubeQuality* quality = made ? made : FindQuality(named);

			// An affix or a craft family names the result outright, so the quality
			// word would only repeat what the name already says; the colour still
			// carries it.
			std::string prefix = AffixName(Tables::MagicPrefix, output.Number("pre", -1));
			std::string suffix = AffixName(Tables::MagicSuffix, output.Number("suf", -1));
			std::string family;
			if (made && made->rarity == RarityCrafted)
				family = CraftFamily(entry->getString("description"));
			if (quality && prefix.length() == 0 && suffix.length() == 0 &&
					family.length() == 0) {
				words.push_back(Label(quality->word.key, quality->word.fallback));
			}
			// A rune carries no quality, but the game still gives its name a
			// colour of its own, which is the one BH draws a rune in everywhere.
			std::string madeType = GroupTypeFor(named.code);
			ItemRarity rarity = RarityNone;
			if (quality)
				rarity = quality->rarity;
			else if (madeType.compare(RC_TYPE_RUNE) == 0)
				rarity = RarityRune;
			recipe.resultColor = NameColor(rarity);

			if (family.length() > 0)
				words.push_back(family);
			if (prefix.length() > 0)
				words.push_back(prefix);
			words.push_back(CodeName(named.code));
			if (suffix.length() > 0)
				words.push_back(suffix);
			recipe.result = Join(words, " ");

			// The item level of the result, which is what a recipe making a magic
			// or a rare item is really choosing: it decides which bonuses the
			// item can roll. The game works it out as a flat amount plus a share
			// of the character's level and a share of the level of the item that
			// went in, so it is said here as the sum it is.
			std::vector<std::string> levels;
			int flatLevel = atoi(entry->getString("lvl").c_str());
			int playerShare = atoi(entry->getString("plvl").c_str());
			int itemShare = atoi(entry->getString("ilvl").c_str());
			if (flatLevel > 0)
				levels.push_back(std::to_string(flatLevel));
			if (playerShare > 0)
				levels.push_back(std::to_string(playerShare) + "% of your level");
			if (itemShare > 0)
				levels.push_back(std::to_string(itemShare) + "% of the item's");
			if (!levels.empty())
				recipe.notes.push_back("Item level: " + Join(levels, " + "));

			if (output.Has("rch"))
				recipe.notes.push_back("Charges are recharged");
			if (output.Has("uns"))
				recipe.notes.push_back("The gems and runes are destroyed");
			// An input allowed at any tier is allowed as its exceptional and its
			// elite version too, which is not something its name says.
			for (unsigned int n = 0; n < inputs.size(); n++) {
				if (inputs[n].Has("upg")) {
					recipe.notes.push_back(
						"The exceptional and elite versions also work");
					break;
				}
			}

			// A row can make more than one thing. Only the first names the
			// recipe; the rest are said in a note, so nothing is dropped.
			std::vector<std::string> extras;
			const char* extraColumns[] = { "output b", "output c" };
			for (int n = 0; n < (int)(sizeof(extraColumns) / sizeof(extraColumns[0])); n++) {
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

			recipe.group = RecipeGroup(output, named, madeType, made, family,
				socketed);
			recipe.searchKey = ToLower(recipe.group + " " + recipe.result + " " +
				recipe.ingredients + " " + Join(recipe.notes, " "));
			recipes.push_back(recipe);
		}
	}

	// Recipes keep the order CubeMain.txt gives them, which walks the cube from
	// the quest recipes through the potions, the gems and the runes to the
	// crafting and the upgrades; sorting them would break the chains apart. The
	// file reaches the same kind of recipe at several points, though, so the
	// groups are gathered up: each keeps the place the file first reaches it,
	// and the recipes inside it keep their own order.
	std::map<std::string, int> place;
	for (unsigned int i = 0; i < recipes.size(); i++) {
		if (place.find(recipes[i].group) == place.end()) {
			int next = (int)place.size();
			place[recipes[i].group] = next;
		}
	}
	std::stable_sort(recipes.begin(), recipes.end(),
		[&place](const CubeRecipe& a, const CubeRecipe& b) {
			return place[a.group] < place[b.group];
		});

	recipesLoaded = true;
	needsRefresh = true;
}

void RecipeTab::LoadStats(CubeRecipe* recipe) {
	if (recipe->statsLoaded)
		return;
	recipe->statsLoaded = true;
	StatDescriptions::Initialize();

	std::vector<StatDescriptions::Stat> stats;
	for (unsigned int i = 0; i < recipe->properties.size(); i++) {
		const CubeProperty& property = recipe->properties[i];
		StatDescriptions::CollectProperty(property.code, property.param,
			property.min, property.max, stats);
	}
	recipe->stats = StatDescriptions::BuildLines(stats);
}

void RecipeTab::ApplyFilter() {
	matches.clear();
	for (unsigned int i = 0; i < recipes.size(); i++) {
		if (query.empty() || recipes[i].searchKey.find(query) != std::string::npos)
			matches.push_back(&recipes[i]);
	}
}

void RecipeTab::PushRows() {
	std::vector<ListRow> rows;
	rows.reserve(matches.size() * 2);
	rowRecipes.clear();
	rowRecipes.reserve(matches.size() * 2);
	shownGroups = 0;

	for (unsigned int i = 0; i < matches.size(); i++) {
		if (i == 0 || matches[i]->group != matches[i - 1]->group) {
			rows.push_back(ListRow({ matches[i]->group }, true));
			rowRecipes.push_back(NULL);
			shownGroups++;
		}

		// The result carries its own colour; the ingredients keep the column's.
		ListRow row({ matches[i]->result,
			RC_SEPARATOR + matches[i]->ingredients });
		row.colors.push_back(matches[i]->resultColor);
		rows.push_back(row);
		rowRecipes.push_back(matches[i]);
	}

	list->SetRows(rows);	// also clears the selection

	// Not on open, which is usually before the game data has loaded, and not on
	// a filtered list, which would leave every group it did not match unfolded.
	if (foldOnPush && query.empty() && !rows.empty()) {
		list->FoldAllGroups();
		foldOnPush = false;
	}
	shownSummary = -1;
	UpdateStatus();
}

void RecipeTab::UpdateStatus() {
	if (!recipesLoaded) {
		statusText->SetText("Waiting for game data to finish loading...");
	} else if (matches.empty()) {
		statusText->SetText("No recipes match \"%s\"", query.c_str());
	} else {
		statusText->SetText("%u recipes in %u group%s",
			(unsigned int)matches.size(), shownGroups,
			(shownGroups == 1) ? "" : "s");
	}
}

// ItemDescription orders and spaces the panel the way the game describes a
// recipe; the tab only says what goes in it.
std::vector<TooltipLine> RecipeTab::BuildSummaryLines(CubeRecipe* recipe) {
	ItemDescription::Recipe cube;
	cube.name = recipe->result;
	cube.nameColor = recipe->resultColor;
	cube.ingredients = recipe->ingredients;
	cube.AddStats(recipe->stats, Blue);
	cube.AddStats(recipe->notes, Grey, true);
	return ItemDescription::Build(cube);
}

// Follows the mouse, falling back to the selection. Rebuilt only when the row
// changes, since the mouse sits on one row for many frames.
void RecipeTab::UpdateSummary() {
	int row = list->GetHoveredRow();
	if (row < 0)
		row = list->GetSelectedRow();

	// A heading describes nothing of its own; it only gathers its recipes up.
	bool describable = IsActive() && row >= 0 && row < (int)rowRecipes.size() &&
		rowRecipes[row] != NULL;
	if (!describable) {
		summary->SetActive(false);
		shownSummary = -1;
		return;
	}

	if (row != shownSummary) {
		// recipes owns the records; rowRecipes only points into it.
		CubeRecipe* recipe = const_cast<CubeRecipe*>(rowRecipes[row]);
		LoadStats(recipe);

		summary->SetLines(BuildSummaryLines(recipe));
		shownSummary = row;
	}

	// Must follow SetLines(): where it fits depends on how big it turned out.
	summary->PlaceBeside(tab->GetX(), tab->GetY(), tab->GetXSize(), tab->GetYSize());
	summary->SetActive(true);
}

void RecipeTab::Search(const std::string& text) {
	std::string trimmed = Trim(text);
	query = ToLower(trimmed);
	searchBox->SetText("%s", trimmed.c_str());
	searchBox->SetTextPos(0);
	searchBox->ResetSelection();
	searchBox->SetCursorPosition(searchBox->GetText().length());
	lastBoxText = searchBox->GetText();
	list->SetScrollTop(0);
	needsRefresh = true;
}

// The caret goes straight in the search box. A search that arrived with the
// window, from the chat command, is left alone: the box only clears on a click.
void RecipeTab::OnOpen() {
	searchBox->SetCursorPosition(searchBox->GetText().length());
	searchBox->SetFocused(true);
}

void RecipeTab::OnClose() {
	searchBox->SetFocused(false);
	summary->SetActive(false);
	shownSummary = -1;
	foldOnPush = true;
	Search("");
}

void RecipeTab::OnDraw() {
	if (tab->GetXSize() != laidOutWidth || tab->GetYSize() != laidOutHeight)
		ApplyLayout();

	// MpqLoaded can fire before this tab exists.
	if (!recipesLoaded && Tables::isInitialized()) {
		StatDescriptions::Initialize();
		BuildRecipes();
	}

	if (searchBox->GetText() != lastBoxText) {
		lastBoxText = searchBox->GetText();
		query = ToLower(Trim(lastBoxText));
		list->SetScrollTop(0);
		needsRefresh = true;
	}

	if (needsRefresh) {
		ApplyFilter();
		// Suspended rather than cleared, so clearing the search restores the
		// user's folds.
		list->SetFoldingSuspended(!query.empty());
		PushRows();
		needsRefresh = false;
	}

	// Row 0 is a heading, so enter takes the first row holding a recipe.
	if (searchBox->TakeSubmitted()) {
		for (unsigned int i = 0; i < rowRecipes.size(); i++) {
			if (rowRecipes[i]) {
				list->SetSelectedRow((int)i);
				break;
			}
		}
	}

	// The mouse and the selection move on the input thread, so catch up here.
	UpdateSummary();
}

bool RecipeTab::OnKey(bool up, BYTE key) {
	switch (key) {
		// The selection ends on the heading rather than being let go of, so
		// folding and unfolding are both reachable from wherever the last press
		// left it.
		case VK_LEFT:
		case VK_RIGHT: {
			if (up)
				return true;

			int row = list->GetSelectedRow();
			int group = list->GetGroupRowFor(row);
			if (group < 0)
				return true;

			if (key == VK_LEFT) {
				list->SetSelectedRow(group);
				list->SetGroupFolded(group, true);
			} else if (row != group) {
				// Already inside the group, so there is nowhere further in to go.
			} else if (list->IsGroupFolded(group)) {
				list->SetGroupFolded(group, false);
			} else {
				list->MoveSelection(1);		// open already, so step into it
			}
			return true;
		}

		case VK_UP:
		case VK_DOWN:
		case VK_PRIOR:
		case VK_NEXT:
		case VK_HOME:
		case VK_END: {
			if (up)
				return true;

			// A screenful less one row, so the row being read stays on screen.
			int visible = (int)list->GetVisibleRows();
			int step = (visible > 1) ? (visible - 1) : 1;
			int count = (int)list->GetRowCount();
			switch (key) {
				case VK_UP:		list->MoveSelection(-1); break;
				case VK_DOWN:	list->MoveSelection(1); break;
				case VK_PRIOR:	list->MoveSelection(-step); break;
				case VK_NEXT:	list->MoveSelection(step); break;
				case VK_HOME:	list->SetSelectedRow(0); break;
				case VK_END:	list->SetSelectedRow(count - 1); break;
			}
			return true;
		}
	}
	return false;
}
