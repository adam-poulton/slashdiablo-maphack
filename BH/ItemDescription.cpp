#include "ItemDescription.h"
#include <algorithm>
#include <map>
#include "StatDescriptions.h"
#include "StringUtil.h"
#include "TableReader.h"

namespace ItemDescription {

// The three tables the base items are split across, in the order the game reads
// them. Weapons and armour carry damage, defense and durability between them;
// Misc.txt holds the rings, amulets, charms, jewels, gems and runes, which carry
// nothing but a name.
//
// The flag beside each says whether its rows are to be read as weapons, since
// only Weapons.txt carries damage and speed.
static const struct { Table* table; bool weapon; } kBaseTables[] = {
	{ &Tables::Weapons, true },
	{ &Tables::Armor, false },
	{ &Tables::Misc, false }
};

// Read once and kept, since a panel asks for a base a row at a time. The order
// is held alongside the map: a map is keyed by code, and table order is the
// game's own progression through the tiers, which is worth keeping.
static std::map<std::string, Base> bases;
static std::vector<const Base*> baseOrder;
static bool basesLoaded = false;

// A blank cell is absent from the row rather than empty, which reads back as
// the nothing it is.
static int Column(const JSONObject* row, const std::string& name) {
	return atoi(row->getString(name).c_str());
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

Range Defense(const Base& base, const Modifiers& modifiers) {
	if (!base.defense.Any())
		return Range();
	return Apply(base.defense, modifiers.defensePercent, modifiers.defenseFlat);
}

// The numbers the game prints between an item's name and its requirements.
// Damage comes in whichever forms the weapon can be swung in, and a weapon that
// can be held in either hand carries both.
static void ReadNumbers(const JSONObject* row, bool weapon, Base& base) {
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
			RangeText(Defense(base, modifiers)));
	}

	// An item the game will never let wear out is given no durability to watch,
	// only the word.
	if (base.durability > 0 && !modifiers.indestructible) {
		attributes.push_back(Label("ItemStats1d", "Durability:") + " " +
			std::to_string(base.durability));
	}
}

// An item can be given no more sockets than its base allows and no more than
// its type allows at the item level it rolled at, and MaxSock40 is the highest
// of those three bands.
static int MaxSockets(const JSONObject* row, const std::string& type) {
	int sockets = Column(row, "gemsockets");
	JSONObject* entry = Tables::ItemTypes.findEntry("Code", type);
	if (!entry)
		return sockets;
	int cap = atoi(entry->getString("MaxSock40").c_str());
	return (sockets < cap) ? sockets : cap;
}

// Nothing is kept until every table a base is read out of has been read, so
// that a base is never remembered half resolved. ItemTypes is one of them: a
// base takes the name of its type and the cap on its sockets from there, and
// both are settled here rather than on the way out.
static void LoadBases() {
	if (basesLoaded)
		return;

	const int tableCount = (int)(sizeof(kBaseTables) / sizeof(kBaseTables[0]));
	if (Tables::ItemTypes.size() == 0)
		return;
	for (int i = 0; i < tableCount; i++) {
		if (kBaseTables[i].table->size() == 0)
			return;
	}

	for (int i = 0; i < tableCount; i++) {
		Table& table = *kBaseTables[i].table;
		bool weapon = kBaseTables[i].weapon;
		for (int n = 0; n < table.size(); n++) {
			const JSONObject* row = table.entryAt(n);
			std::string code = row->getString("code");
			if (code.length() == 0)
				continue;

			Base base;
			base.code = code;
			base.name = NameLine(
				StatDescriptions::GetString(row->getString("namestr")));
			if (base.name.length() == 0)
				base.name = row->getString("name");
			base.type = row->getString("type");
			base.typeName = TypeName(base.type);
			base.weapon = weapon;
			base.spawnable = (Column(row, "spawnable") == 1);
			base.level = Column(row, "level");
			base.quest = Column(row, "quest");
			base.speed = weapon ? Column(row, "speed") : 0;
			base.maxSockets = MaxSockets(row, base.type);

			// A base pointing an upgrade column at itself has no upgrade there, and
			// is itself the tier that column stands for.
			std::string exceptional = row->getString("ubercode");
			std::string elite = row->getString("ultracode");
			if (elite.compare(code) == 0)
				base.tier = TierElite;
			else if (exceptional.compare(code) == 0)
				base.tier = TierExceptional;
			if (exceptional.compare(code) != 0)
				base.exceptional = exceptional;
			if (elite.compare(code) != 0)
				base.elite = elite;

			// Armor.txt has no reqdex column at all, and a blank cell is what an
			// item with no requirement of that kind carries.
			base.requirements.level = Column(row, "levelreq");
			base.requirements.strength = Range(Column(row, "reqstr"),
				Column(row, "reqstr"));
			base.requirements.dexterity = Range(Column(row, "reqdex"),
				Column(row, "reqdex"));

			ReadNumbers(row, weapon, base);

			// A map keeps its nodes put, so a pointer taken here survives the rest of
			// the load. Listed on the way in rather than afterwards, so that a code
			// two tables both carry is one base, in the place it first appeared.
			std::pair<std::map<std::string, Base>::iterator, bool> stored =
				bases.insert(std::make_pair(code, base));
			if (stored.second)
				baseOrder.push_back(&stored.first->second);
			else
				stored.first->second = base;
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

const std::vector<const Base*>& AllBases() {
	LoadBases();
	return baseOrder;
}

std::string BaseName(const std::string& code) {
	const Base* base = FindBase(code);
	return (base && base->name.length() > 0) ? base->name : code;
}

std::string TierName(Tier tier) {
	switch (tier) {
		case TierNormal:		return "Normal";
		case TierExceptional:	return "Exceptional";
		case TierElite:			return "Elite";
		default:				return "";
	}
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

// The kinds of base one of the two columns of them names, in table order and
// without a repeat of one already named. The codes are kept alongside the names
// for a caller that needs the type itself and not only what it is called.
static void AppendTypes(JSONObject& entry, const char* column, int count,
		std::vector<std::string>& names, std::vector<std::string>* codes) {
	for (int n = 1; n <= count; n++) {
		std::string code = Trim(entry.getString(column + std::to_string(n)));
		if (code.length() == 0)
			continue;
		std::string name = TypeName(code);
		if (std::find(names.begin(), names.end(), name) != names.end())
			continue;
		names.push_back(name);
		if (codes)
			codes->push_back(code);
	}
}

ItemTypes ReadTypes(JSONObject& entry, int typeCount, int excludedTypeCount) {
	ItemTypes types;
	AppendTypes(entry, "itype", typeCount, types.names, &types.codes);

	std::vector<std::string> excluded;
	AppendTypes(entry, "etype", excludedTypeCount, excluded, NULL);

	types.text = Join(types.names, ", ");
	if (types.text.length() > 0 && !excluded.empty())
		types.text += " (not " + Join(excluded, ", ") + ")";
	return types;
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

void Description::AddBaseLimits(const std::string& code) {
	const Base* base = FindBase(code);
	if (!base)
		return;

	// In brackets, since the number counts down rather than up and is quoted that
	// way wherever weapons are compared. The label comes with a trailing space of
	// its own, so it is trimmed and the gap put back.
	if (base->weapon) {
		attributes.push_back(Trim(Label("StrSkill106", "Attack Speed:")) + " [" +
			std::to_string(base->speed) + "]");
	}

	// As a range, because it is one: a base that can take six can also come with
	// one, and the number an item ends up with is rolled.
	if (base->maxSockets > 0) {
		attributes.push_back(Trim(Label("ModStre8c", "Sockets")) + ": " +
			RangeText(Range(1, base->maxSockets)));
	}
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
