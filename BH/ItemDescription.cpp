#include "ItemDescription.h"
#include <map>
#include "Common.h"
#include "MPQReader.h"
#include "StatDescriptions.h"
#include "TableReader.h"

namespace ItemDescription {

// The three tables the base items are split across, in the order the game reads
// them. Weapons and armour carry damage, defense and durability between them;
// Misc.txt holds the rings, amulets, charms, jewels, gems and runes, which carry
// nothing but a name.
static const char* kBaseTables[] = { "weapons", "armor", "misc" };

// Read once and kept, since a panel asks for a base a row at a time.
static std::map<std::string, Base> bases;
static bool basesLoaded = false;

static int Column(const std::map<std::string, std::string>& row,
		const std::string& name) {
	std::map<std::string, std::string>::const_iterator found = row.find(name);
	return (found != row.end()) ? atoi(found->second.c_str()) : 0;
}

static std::string Text(const std::map<std::string, std::string>& row,
		const std::string& name) {
	std::map<std::string, std::string>::const_iterator found = row.find(name);
	return (found != row.end()) ? found->second : "";
}

// The game's own wording, so a localised client reads correctly. The fallbacks
// are what the English tables say, for a client whose strings we could not read.
static std::string Label(const std::string& key, const char* fallback) {
	std::string label = StatDescriptions::GetString(key);
	return (label.length() > 0) ? label : fallback;
}

// "98 to 141", or just the one number where the range does not vary. The key is
// misspelled in the game's own string table, which is why it reads as it does.
static std::string RangeText(const Range& range) {
	if (range.high <= range.low)
		return std::to_string(range.low);
	return std::to_string(range.low) + " " + Label("ItemStast1k", "to") + " " +
		std::to_string(range.high);
}

// The game adds a percentage on as a share of the base rather than scaling the
// whole, and truncates that share toward zero. Rounding the other way costs
// Shadow Dancer a point of strength: 208 less a fifth is 167 to the game, and
// 166 to anything that works out 208 * 80 / 100 instead.
static int Boost(int value, int percent) {
	if (percent < -100)
		percent = -100;
	int boosted = value + (value * percent / 100);
	return (boosted > 0) ? boosted : 0;
}

// A base range under a percentage and a flat amount, each of which is itself a
// range. The ends are sorted afterwards rather than assumed: a modifier that
// lowers what it touches, as a requirement modifier does, pairs the low end of
// the range with the high end of the result.
static Range Apply(const Range& base, const Range& percent, const Range& flat) {
	int first = Boost(base.low, percent.low) + flat.low;
	int second = Boost(base.high, percent.high) + flat.high;
	if (first < 0)
		first = 0;
	if (second < 0)
		second = 0;
	return (first <= second) ? Range(first, second) : Range(second, first);
}

// The numbers the game prints between an item's name and its requirements.
// Damage comes in whichever forms the weapon can be swung in, and a weapon that
// can be held in either hand carries both.
static void ReadNumbers(const std::map<std::string, std::string>& row,
		bool weapon, Base& base) {
	if (weapon) {
		if (Column(row, "maxmisdam") > 0) {
			base.throwDamage = Range(Column(row, "minmisdam"),
				Column(row, "maxmisdam"));
		}

		bool twoHanded = (Column(row, "2handed") == 1);
		bool eitherHand = (Column(row, "1or2handed") == 1);
		if ((!twoHanded || eitherHand) && Column(row, "maxdam") > 0)
			base.oneHandDamage = Range(Column(row, "mindam"), Column(row, "maxdam"));
		if (twoHanded && Column(row, "2handmaxdam") > 0) {
			base.twoHandDamage = Range(Column(row, "2handmindam"),
				Column(row, "2handmaxdam"));
		}
	} else if (Column(row, "maxac") > 0) {
		base.defense = Range(Column(row, "minac"), Column(row, "maxac"));
	}

	// A stackable weapon is a throwing weapon, and the game gives its stack over
	// where the durability of anything else goes. Bows carry a durability the
	// game never shows, which is what nodurability marks.
	if (Column(row, "durability") > 0 && Column(row, "nodurability") != 1 &&
		Column(row, "stackable") != 1) {
		base.durability = Column(row, "durability");
	}
}

// Worded only once the item's own bonuses have been folded in, and only for the
// numbers the base actually carries. That last part is what keeps the flat
// defense on an amulet out of a Defense line the game never gives it: an amulet
// has no defense of its own, so there is no line to fold into and the bonus is
// left to describe itself.
static void RenderAttributes(const Base& base, const Modifiers& modifiers,
		std::vector<std::string>& attributes) {
	if (base.throwDamage.Any()) {
		attributes.push_back(Label("ItemStats1n", "Throw Damage:") + " " +
			RangeText(Apply(base.throwDamage, modifiers.damagePercent,
				Range(modifiers.damageMinFlat.low + modifiers.throwMinFlat.low,
					modifiers.damageMaxFlat.high + modifiers.throwMaxFlat.high))));
	}
	if (base.oneHandDamage.Any()) {
		attributes.push_back(Label("ItemStats1l", "One-Hand Damage:") + " " +
			RangeText(Apply(base.oneHandDamage, modifiers.damagePercent,
				Range(modifiers.damageMinFlat.low, modifiers.damageMaxFlat.high))));
	}
	if (base.twoHandDamage.Any()) {
		attributes.push_back(Label("ItemStats1m", "Two-Hand Damage:") + " " +
			RangeText(Apply(base.twoHandDamage, modifiers.damagePercent,
				Range(modifiers.damageMinFlat.low + modifiers.twoHandMinFlat.low,
					modifiers.damageMaxFlat.high + modifiers.twoHandMaxFlat.high))));
	}
	if (base.defense.Any()) {
		attributes.push_back(Label("ItemStats1h", "Defense:") + " " +
			RangeText(Apply(base.defense, modifiers.defensePercent,
				modifiers.defenseFlat)));
	}

	// An item the game will never let wear out is given no durability to watch,
	// only the word.
	if (base.durability > 0 && !modifiers.indestructible) {
		attributes.push_back(Label("ItemStats1d", "Durability:") + " " +
			std::to_string(base.durability));
	}
}

// Nothing is cached until the tables the names and types come out of are all
// readable, so that a base is never remembered half resolved.
static void LoadBases() {
	if (basesLoaded || !Tables::isInitialized())
		return;

	for (int i = 0; i < (int)(sizeof(kBaseTables) / sizeof(kBaseTables[0])); i++) {
		std::map<std::string, MPQData*>::iterator data =
			MpqDataMap.find(kBaseTables[i]);
		if (data == MpqDataMap.end() || !data->second)
			return;

		bool weapon = (i == 0);
		for (auto row = data->second->data.begin(); row != data->second->data.end(); row++) {
			std::string code = Text(*row, "code");
			if (code.length() == 0)
				continue;

			Base base;
			base.code = code;
			base.name = NameLine(
				StatDescriptions::GetString(Text(*row, "namestr")));
			if (base.name.length() == 0)
				base.name = Text(*row, "name");
			base.type = Text(*row, "type");
			base.typeName = TypeName(base.type);
			base.quest = Column(*row, "quest");

			// A base pointing an upgrade column at itself has no upgrade there.
			std::string exceptional = Text(*row, "ubercode");
			std::string elite = Text(*row, "ultracode");
			if (exceptional.compare(code) != 0)
				base.exceptional = exceptional;
			if (elite.compare(code) != 0)
				base.elite = elite;

			// Armor.txt has no reqdex column at all, and a blank cell is what an
			// item with no requirement of that kind carries.
			base.requirements.level = Column(*row, "levelreq");
			base.requirements.strength = Range(Column(*row, "reqstr"),
				Column(*row, "reqstr"));
			base.requirements.dexterity = Range(Column(*row, "reqdex"),
				Column(*row, "reqdex"));

			ReadNumbers(*row, weapon, base);
			bases[code] = base;
		}
	}
	basesLoaded = true;
}

std::string NameLine(const std::string& text) {
	size_t breakAt = text.find_last_of("\r\n");
	return (breakAt == std::string::npos) ? text : text.substr(breakAt + 1);
}

const Base* FindBase(const std::string& code) {
	LoadBases();
	std::map<std::string, Base>::iterator found = bases.find(code);
	return (found != bases.end()) ? &found->second : NULL;
}

std::string BaseName(const std::string& code) {
	const Base* base = FindBase(code);
	return (base && base->name.length() > 0) ? base->name : code;
}

std::string TypeName(const std::string& code) {
	JSONObject* entry = Tables::ItemTypes.findEntry("Code", code);
	if (entry) {
		std::string name = Trim(entry->getString("ItemType"));
		if (name.length() > 0)
			return name;
	}
	return code;
}

// Which stat moves which number. Going by the stat a property writes rather than
// by the property's own code is what keeps the exclusions honest: the defense a
// shield grants against missiles is armorclass_vs_missile, defense granted per
// level is item_armor_perlevel, and fire damage is firemindam, so none of them
// are named here and none of them can be folded in by mistake. A realm that adds
// a property of its own writing one of these stats is taken in without an edit.
struct FoldedStat {
	const char* stat;
	Range Modifiers::* into;
};

static const FoldedStat kFoldedStats[] = {
	{ "armorclass",              &Modifiers::defenseFlat },
	{ "item_armor_percent",      &Modifiers::defensePercent },

	// The game holds enhanced damage as a minimum and a maximum percentage, and
	// the property that grants it always sets the pair to one number. Reading
	// only the maximum keeps them from adding up to twice what they are.
	{ "item_maxdamage_percent",  &Modifiers::damagePercent },

	{ "mindamage",               &Modifiers::damageMinFlat },
	{ "maxdamage",               &Modifiers::damageMaxFlat },
	{ "secondary_mindamage",     &Modifiers::twoHandMinFlat },
	{ "secondary_maxdamage",     &Modifiers::twoHandMaxFlat },
	{ "item_throw_mindamage",    &Modifiers::throwMinFlat },
	{ "item_throw_maxdamage",    &Modifiers::throwMaxFlat },

	{ "item_req_percent",        &Modifiers::requirementPercent },
};

Modifiers ReadModifiers(const std::vector<StatDescriptions::StatTotal>& totals) {
	Modifiers modifiers;
	for (int i = 0; i < (int)(sizeof(kFoldedStats) / sizeof(kFoldedStats[0])); i++) {
		int low = 0, high = 0;
		StatDescriptions::TotalFor(totals, kFoldedStats[i].stat, low, high);
		modifiers.*(kFoldedStats[i].into) = Range(low, high);
	}

	// Damage that raises both ends at once, which the tables keep apart from the
	// minimum and the maximum.
	int low = 0, high = 0;
	StatDescriptions::TotalFor(totals, "item_normaldamage", low, high);
	modifiers.damageMinFlat.low += low;
	modifiers.damageMinFlat.high += high;
	modifiers.damageMaxFlat.low += low;
	modifiers.damageMaxFlat.high += high;

	StatDescriptions::TotalFor(totals, "item_indesctructible", low, high);
	modifiers.indestructible = (high > 0);
	return modifiers;
}

void Description::AddTitle(const std::string& text, TextColor color) {
	titles.push_back(Drawing::TooltipLine(text, color));
}

void Description::AddBase(const std::string& code, TextColor nameColor,
		const Modifiers& modifiers) {
	const Base* base = FindBase(code);
	if (!base)
		return;

	AddTitle(base->name, nameColor);
	RenderAttributes(*base, modifiers, attributes);

	// Only strength and dexterity move. A requirement modifier leaves the level
	// alone, and the level is the one a caller may still raise.
	requirements = base->requirements;
	requirements.strength = Apply(requirements.strength,
		modifiers.requirementPercent, Range());
	requirements.dexterity = Apply(requirements.dexterity,
		modifiers.requirementPercent, Range());
}

void Description::AddStats(const std::vector<std::string>& lines, TextColor color,
		bool spaced) {
	AddSection("", color, lines, color, spaced);
}

void Description::AddSection(const std::string& heading, TextColor headingColor,
		const std::vector<std::string>& lines, TextColor color, bool spaced) {
	if (heading.length() == 0 && lines.empty())
		return;

	Section section;
	section.heading = heading;
	section.headingColor = headingColor;
	section.lines = lines;
	section.color = color;
	section.spaced = spaced;
	sections.push_back(section);
}

void Recipe::AddStats(const std::vector<std::string>& lines, TextColor color,
		bool spaced) {
	if (lines.empty())
		return;

	Section section;
	section.lines = lines;
	section.color = color;
	section.spaced = spaced;
	sections.push_back(section);
}

// Dexterity before strength before level, which is the order the game lists
// them in on an item.
static void AppendRequirements(const Requirements& requirements,
		std::vector<Drawing::TooltipLine>& lines) {
	struct Line {
		Range value;
		const char* key;
		const char* fallback;
	};
	const Line order[] = {
		{ requirements.dexterity, "ItemStats1f", "Required Dexterity:" },
		{ requirements.strength, "ItemStats1e", "Required Strength:" },
		{ Range(requirements.level, requirements.level), "ItemStats1p",
			"Required Level:" },
	};

	for (int i = 0; i < (int)(sizeof(order) / sizeof(order[0])); i++) {
		if (!order[i].value.Any())
			continue;
		std::string label = Label(order[i].key, order[i].fallback);
		lines.push_back(Drawing::TooltipLine(
			label + " " + RangeText(order[i].value), White));
	}
}

std::vector<Drawing::TooltipLine> Build(const Description& item) {
	std::vector<Drawing::TooltipLine> lines = item.titles;
	for (unsigned int i = 0; i < item.attributes.size(); i++)
		lines.push_back(Drawing::TooltipLine(item.attributes[i], White));
	AppendRequirements(item.requirements, lines);

	for (unsigned int i = 0; i < item.sections.size(); i++) {
		const Section& section = item.sections[i];

		// The first block is always parted from the names above it; after that
		// only the blocks that asked to be.
		if (i == 0 || section.spaced)
			lines.push_back(Drawing::TooltipLine("", White));

		if (section.heading.length() > 0)
			lines.push_back(Drawing::TooltipLine(section.heading, section.headingColor));
		for (unsigned int n = 0; n < section.lines.size(); n++)
			lines.push_back(Drawing::TooltipLine(section.lines[n], section.color));
	}
	return lines;
}

std::vector<Drawing::TooltipLine> Build(const Recipe& recipe) {
	Description item;
	item.AddTitle(recipe.name, recipe.nameColor);
	if (recipe.appliesTo.length() > 0)
		item.AddTitle(recipe.appliesTo, Grey);
	if (recipe.ingredients.length() > 0)
		item.AddTitle(recipe.ingredients, recipe.ingredientColor);
	item.requirements = recipe.requirements;
	item.sections = recipe.sections;
	return Build(item);
}

};
