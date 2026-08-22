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
static std::string Range(int low, int high) {
	if (high <= low)
		return std::to_string(low);
	return std::to_string(low) + " " + Label("ItemStast1k", "to") + " " +
		std::to_string(high);
}

// The block the game prints between an item's name and its requirements. Damage
// comes in whichever forms the weapon can be swung in, and a weapon that can be
// held in either hand lists both.
static void ReadAttributes(const std::map<std::string, std::string>& row,
		bool weapon, std::vector<std::string>& attributes) {
	if (weapon) {
		if (Column(row, "maxmisdam") > 0) {
			attributes.push_back(Label("ItemStats1n", "Throw Damage:") + " " +
				Range(Column(row, "minmisdam"), Column(row, "maxmisdam")));
		}

		bool twoHanded = (Column(row, "2handed") == 1);
		bool eitherHand = (Column(row, "1or2handed") == 1);
		if ((!twoHanded || eitherHand) && Column(row, "maxdam") > 0) {
			attributes.push_back(Label("ItemStats1l", "One-Hand Damage:") + " " +
				Range(Column(row, "mindam"), Column(row, "maxdam")));
		}
		if (twoHanded && Column(row, "2handmaxdam") > 0) {
			attributes.push_back(Label("ItemStats1m", "Two-Hand Damage:") + " " +
				Range(Column(row, "2handmindam"), Column(row, "2handmaxdam")));
		}
	} else if (Column(row, "maxac") > 0) {
		attributes.push_back(Label("ItemStats1h", "Defense:") + " " +
			Range(Column(row, "minac"), Column(row, "maxac")));
	}

	// A stackable weapon is a throwing weapon, and the game gives its stack over
	// where the durability of anything else goes. Bows carry a durability the
	// game never shows, which is what nodurability marks.
	if (Column(row, "durability") > 0 && Column(row, "nodurability") != 1 &&
		Column(row, "stackable") != 1) {
		attributes.push_back(Label("ItemStats1d", "Durability:") + " " +
			std::to_string(Column(row, "durability")));
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
			base.name = StatDescriptions::GetString(Text(*row, "namestr"));
			if (base.name.length() == 0)
				base.name = Text(*row, "name");
			base.type = Text(*row, "type");
			base.typeName = TypeName(base.type);

			// Armor.txt has no reqdex column at all, and a blank cell is what an
			// item with no requirement of that kind carries.
			base.requirements.level = Column(*row, "levelreq");
			base.requirements.strength = Column(*row, "reqstr");
			base.requirements.dexterity = Column(*row, "reqdex");

			ReadAttributes(*row, weapon, base.attributes);
			bases[code] = base;
		}
	}
	basesLoaded = true;
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

void Description::AddTitle(const std::string& text, TextColor color) {
	titles.push_back(Drawing::TooltipLine(text, color));
}

void Description::AddBase(const std::string& code, TextColor nameColor) {
	const Base* base = FindBase(code);
	if (!base)
		return;

	AddTitle(base->name, nameColor);
	attributes.insert(attributes.end(), base->attributes.begin(),
		base->attributes.end());
	requirements = base->requirements;
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
		int value;
		const char* key;
		const char* fallback;
	};
	const Line order[] = {
		{ requirements.dexterity, "ItemStats1f", "Required Dexterity:" },
		{ requirements.strength, "ItemStats1e", "Required Strength:" },
		{ requirements.level, "ItemStats1p", "Required Level:" },
	};

	for (int i = 0; i < (int)(sizeof(order) / sizeof(order[0])); i++) {
		if (order[i].value <= 0)
			continue;
		std::string label = Label(order[i].key, order[i].fallback);
		lines.push_back(Drawing::TooltipLine(
			label + " " + std::to_string(order[i].value), White));
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
