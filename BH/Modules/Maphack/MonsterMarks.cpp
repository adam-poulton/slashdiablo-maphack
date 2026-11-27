#include "MonsterMarks.h"
#include <algorithm>
#include <cctype>
#include "../Forge/ForgeCodec.h"

typedef ForgeCodec::Json Json;

namespace MonsterMarks {
	namespace {
		struct RankName {
			const char* key;
			const char* label;
			const char* plural;
		};

		const RankName RankNames[RankCount] = {
			{ "normal", "Normal", "normal monsters" },
			{ "minion", "Minion", "minions" },
			{ "champion", "Champion", "champions" },
			{ "boss", "Boss", "bosses" },
		};

		struct DifficultyName {
			const char* key;
			const char* label;
		};

		const DifficultyName DifficultyNames[DifficultyCount] = {
			{ "normal", "Normal" },
			{ "nightmare", "Nightmare" },
			{ "hell", "Hell" },
		};

		const char* const LineKeys[LineChangeCount] = { "", "none", "drawn" };

		// The size the monster X has always been drawn at.
		const unsigned int CrossSize = 4;

		// What the Cow King's herd has always been marked in, which no setting held.
		const unsigned int CowKingHerdColor = 0xE1;

		// A monster's level never passes this (MonLvl.txt has 110 rows), and a
		// bound is typed in three digits.
		const unsigned int MaxLevel = 999;

		bool Has(const std::vector<unsigned int>& sorted, unsigned int value) {
			return std::binary_search(sorted.begin(), sorted.end(), value);
		}

		void SortUnique(std::vector<unsigned int>& values) {
			std::sort(values.begin(), values.end());
			values.erase(std::unique(values.begin(), values.end()), values.end());
		}

		const ModifierName* FindModifier(unsigned int kind, unsigned int id) {
			for (const ModifierName& name : Modifiers())
				if ((unsigned int)name.kind == kind && name.id == id)
					return &name;
			return NULL;
		}

		const Json* Member(const Json& object, const char* key) {
			Json::const_iterator found = object.find(key);
			return found == object.end() ? NULL : &*found;
		}

		Json EncodeModifier(unsigned int kind, unsigned int id) {
			const ModifierName* name = FindModifier(kind, id);
			return name ? Json(name->key) : Json(id);
		}

		// By name, whatever its case, or by number.
		bool DecodeModifier(const Json& stored, ModifierKind kind, unsigned int& id) {
			if (stored.is_number_unsigned()) {
				id = (unsigned int)stored.get<unsigned long long>();
				return true;
			}
			if (!stored.is_string())
				return false;
			std::string text = ForgeCodec::Lower(ForgeCodec::Trimmed(stored.get<std::string>()));
			for (const ModifierName& name : Modifiers()) {
				if (name.kind == kind && text.compare(name.key) == 0) {
					id = name.id;
					return true;
				}
			}
			return false;
		}

		void DecodeIds(const Json& stored, std::vector<unsigned int>& ids) {
			if (!stored.is_array())
				return;
			for (const Json& entry : stored)
				if (entry.is_number_unsigned())
					ids.push_back((unsigned int)entry.get<unsigned long long>());
		}

		Json Encode(const Rule& rule) {
			Json entry = Json::object();
			if (!rule.name.empty())
				entry["name"] = rule.name;
			if (!rule.monsters.empty())
				entry["monsters"] = rule.monsters;
			if (!rule.superUniques.empty())
				entry["superUniques"] = rule.superUniques;
			if (rule.cowKingHerd)
				entry["cowKingHerd"] = true;
			if ((rule.ranks & AllRanks) != AllRanks) {
				Json ranks = Json::array();
				for (int rank = 0; rank < RankCount; rank++)
					if (rule.ranks & (1u << rank))
						ranks.push_back(RankNames[rank].key);
				entry["ranks"] = ranks;
			}
			if ((rule.difficulties & AllDifficulties) != AllDifficulties) {
				Json difficulties = Json::array();
				for (int difficulty = 0; difficulty < DifficultyCount; difficulty++)
					if (rule.difficulties & (1u << difficulty))
						difficulties.push_back(DifficultyNames[difficulty].key);
				entry["difficulties"] = difficulties;
			}
			if (rule.modifierKind == Enchantment)
				entry["enchantment"] = EncodeModifier(Enchantment, rule.modifierId);
			else if (rule.modifierKind == Aura)
				entry["aura"] = EncodeModifier(Aura, rule.modifierId);
			if (rule.minLevel > 0)
				entry["minLevel"] = rule.minLevel;
			if (rule.maxLevel > 0)
				entry["maxLevel"] = rule.maxLevel;
			if (rule.setsIcon) {
				entry["icon"] = ForgeCodec::EncodeIcon(rule.style.icon);
				entry["size"] = rule.style.size;
				entry["fill"] = ForgeCodec::EncodeColor(rule.style.fillColor);
				entry["border"] = ForgeCodec::EncodeColor(rule.style.borderColor);
				entry["borderWidth"] = rule.style.borderWidth;
			}
			if (rule.line == LineOff || rule.line == LineOn)
				entry["line"] = LineKeys[rule.line];
			if (rule.line == LineOn)
				entry["lineColor"] = ForgeCodec::EncodeColor(rule.style.lineColor);
			return entry;
		}

		Rule Decode(const Json& entry) {
			Rule rule;
			if (const Json* name = Member(entry, "name"))
				if (name->is_string())
					rule.name = ForgeCodec::Trimmed(name->get<std::string>());
			if (const Json* monsters = Member(entry, "monsters"))
				DecodeIds(*monsters, rule.monsters);
			if (const Json* superUniques = Member(entry, "superUniques"))
				DecodeIds(*superUniques, rule.superUniques);
			if (const Json* herd = Member(entry, "cowKingHerd"))
				rule.cowKingHerd = herd->is_boolean() && herd->get<bool>();
			if (const Json* ranks = Member(entry, "ranks")) {
				if (ranks->is_array()) {
					rule.ranks = 0;
					for (const Json& named : *ranks) {
						if (!named.is_string())
							continue;
						std::string text = ForgeCodec::Lower(
							ForgeCodec::Trimmed(named.get<std::string>()));
						for (int rank = 0; rank < RankCount; rank++)
							if (text.compare(RankNames[rank].key) == 0)
								rule.ranks |= 1u << rank;
					}
				}
			}
			if (const Json* difficulties = Member(entry, "difficulties")) {
				if (difficulties->is_array()) {
					rule.difficulties = 0;
					for (const Json& named : *difficulties) {
						if (!named.is_string())
							continue;
						std::string text = ForgeCodec::Lower(
							ForgeCodec::Trimmed(named.get<std::string>()));
						for (int difficulty = 0; difficulty < DifficultyCount; difficulty++)
							if (text.compare(DifficultyNames[difficulty].key) == 0)
								rule.difficulties |= 1u << difficulty;
					}
				}
			}
			unsigned int id = 0;
			const Json* enchantment = Member(entry, "enchantment");
			const Json* aura = Member(entry, "aura");
			if (enchantment && DecodeModifier(*enchantment, Enchantment, id)) {
				rule.modifierKind = Enchantment;
				rule.modifierId = id;
			} else if (aura && DecodeModifier(*aura, Aura, id)) {
				rule.modifierKind = Aura;
				rule.modifierId = id;
			}
			if (const Json* minLevel = Member(entry, "minLevel"))
				ForgeCodec::DecodeCount(*minLevel, 0, MaxLevel, rule.minLevel);
			if (const Json* maxLevel = Member(entry, "maxLevel"))
				ForgeCodec::DecodeCount(*maxLevel, 0, MaxLevel, rule.maxLevel);

			// The icon is what says the rule has one. The rest of it is read where
			// it is given and left at the X's where it is not.
			const Json* icon = Member(entry, "icon");
			if (icon && ForgeCodec::DecodeIcon(*icon, rule.style.icon)) {
				rule.setsIcon = true;
				if (const Json* size = Member(entry, "size"))
					ForgeCodec::DecodeCount(*size, MapIcon::MinSize, MapIcon::MaxSize,
						rule.style.size);
				if (const Json* fill = Member(entry, "fill"))
					ForgeCodec::DecodeColor(*fill, true, rule.style.fillColor);
				if (const Json* border = Member(entry, "border"))
					ForgeCodec::DecodeColor(*border, true, rule.style.borderColor);
				if (const Json* width = Member(entry, "borderWidth"))
					ForgeCodec::DecodeCount(*width, MapIcon::MinBorder, MapIcon::MaxBorder,
						rule.style.borderWidth);
			}
			if (const Json* line = Member(entry, "line")) {
				if (line->is_string()) {
					std::string text = ForgeCodec::Lower(
						ForgeCodec::Trimmed(line->get<std::string>()));
					for (unsigned int change = LineOff; change < LineChangeCount; change++)
						if (text.compare(LineKeys[change]) == 0)
							rule.line = change;
				}
			}
			if (const Json* lineColor = Member(entry, "lineColor"))
				ForgeCodec::DecodeColor(*lineColor, false, rule.style.lineColor);
			rule.Normalize();
			return rule;
		}

		// The ids that share a colour, each colour where it is first given.
		std::vector<std::pair<unsigned int, std::vector<unsigned int>>> ByColor(
				const std::vector<std::pair<unsigned int, unsigned int>>& entries) {
			std::vector<std::pair<unsigned int, std::vector<unsigned int>>> groups;
			for (const std::pair<unsigned int, unsigned int>& entry : entries) {
				bool placed = false;
				for (std::pair<unsigned int, std::vector<unsigned int>>& group : groups) {
					if (group.first == entry.second) {
						group.second.push_back(entry.first);
						placed = true;
						break;
					}
				}
				if (!placed)
					groups.push_back({ entry.second, { entry.first } });
			}
			return groups;
		}

		const unsigned int NotBoss = AllRanks & ~(1u << Boss);

		Rule IconRule(unsigned int color) {
			Rule rule;
			rule.setsIcon = true;
			rule.style = CrossStyle(color % MapIcon::PaletteSize);
			return rule;
		}

		std::string Capitalised(std::string text) {
			if (!text.empty())
				text[0] = (char)std::toupper((unsigned char)text[0]);
			return text;
		}

		std::string RankPhrase(unsigned int ranks) {
			ranks &= AllRanks;
			if (ranks == 0)
				return "no rank";
			std::vector<std::string> in, out;
			for (int rank = 0; rank < RankCount; rank++)
				((ranks & (1u << rank)) ? in : out).push_back(RankNames[rank].plural);
			if (out.size() == 1)
				return "all but " + out[0];
			std::string phrase;
			for (size_t i = 0; i < in.size(); i++) {
				if (i > 0)
					phrase += (i + 1 == in.size()) ? " and " : ", ";
				phrase += in[i];
			}
			return phrase;
		}

		// "in Hell", "in Nightmare and Hell".
		std::string DifficultyPhrase(unsigned int difficulties) {
			difficulties &= AllDifficulties;
			if (difficulties == 0)
				return "in no difficulty";
			std::vector<std::string> in;
			for (int difficulty = 0; difficulty < DifficultyCount; difficulty++)
				if (difficulties & (1u << difficulty))
					in.push_back(DifficultyNames[difficulty].label);
			std::string phrase = "in ";
			for (size_t i = 0; i < in.size(); i++) {
				if (i > 0)
					phrase += (i + 1 == in.size()) ? " and " : ", ";
				phrase += in[i];
			}
			return phrase;
		}

		std::string LevelPhrase(unsigned int min, unsigned int max) {
			if (min > 0 && max > 0) {
				if (min == max)
					return "level " + std::to_string(min);
				return "level " + std::to_string(min) + "-" + std::to_string(max);
			}
			if (min > 0)
				return "level " + std::to_string(min) + "+";
			if (max > 0)
				return "up to level " + std::to_string(max);
			return std::string();
		}
	}

	bool Facts::HasEnchant(unsigned int id) const {
		for (unsigned char enchant : enchants)
			if (enchant == id && id != 0)
				return true;
		return false;
	}

	Rank RankOf(const Facts& facts) {
		if (facts.minion)
			return Minion;
		if (facts.champion)
			return Champion;
		if (facts.boss)
			return Boss;
		return Normal;
	}

	bool IsCowKingHerd(const Facts& facts) {
		return facts.txtId == HellBovine &&
			facts.enchants[0] == EnchantMagicResistant &&
			facts.enchants[1] == EnchantLightning &&
			facts.enchants[3] != 0;
	}

	Rule::Rule() : cowKingHerd(false), ranks(AllRanks), difficulties(AllDifficulties),
			modifierKind(AnyModifier), modifierId(0),
			minLevel(0), maxLevel(0), setsIcon(false), line(LineUnchanged),
			style(CrossStyle(0xFF)) {
	}

	bool Rule::operator==(const Rule& other) const {
		return name == other.name && monsters == other.monsters &&
			superUniques == other.superUniques && cowKingHerd == other.cowKingHerd &&
			ranks == other.ranks &&
			difficulties == other.difficulties &&
			modifierKind == other.modifierKind && modifierId == other.modifierId &&
			minLevel == other.minLevel && maxLevel == other.maxLevel &&
			setsIcon == other.setsIcon && line == other.line && style == other.style;
	}

	void Rule::Normalize() {
		SortUnique(monsters);
		SortUnique(superUniques);
		ranks &= AllRanks;
		difficulties &= AllDifficulties;
		if (modifierKind > Aura)
			modifierKind = AnyModifier;
		if (modifierKind == AnyModifier)
			modifierId = 0;
		if (line >= LineChangeCount)
			line = LineUnchanged;
		minLevel = (std::min)(minLevel, MaxLevel);
		maxLevel = (std::min)(maxLevel, MaxLevel);
		// What the rule does not change says nothing, so it is held at what a new
		// rule has: two rules that do the same thing are then the same rule.
		const MapIconStyle fresh = Rule().style;
		if (!setsIcon) {
			style.icon = fresh.icon;
			style.size = fresh.size;
			style.fillColor = fresh.fillColor;
			style.borderColor = fresh.borderColor;
			style.borderWidth = fresh.borderWidth;
		}
		if (line != LineOn)
			style.lineColor = fresh.lineColor;
		style.line = false;
	}

	bool Matches(const Rule& rule, const Facts& facts) {
		if (!(rule.ranks & (1u << RankOf(facts))))
			return false;
		if (facts.difficulty >= DifficultyCount ||
				!(rule.difficulties & (1u << facts.difficulty)))
			return false;
		if (!rule.monsters.empty() || !rule.superUniques.empty() || rule.cowKingHerd) {
			bool named = Has(rule.monsters, facts.txtId) ||
				(facts.superUnique && Has(rule.superUniques, facts.superUniqueId)) ||
				(rule.cowKingHerd && IsCowKingHerd(facts));
			if (!named)
				return false;
		}
		if (rule.modifierKind == Enchantment && !facts.HasEnchant(rule.modifierId))
			return false;
		if (rule.modifierKind == Aura) {
			if (!facts.HasEnchant(EnchantAura))
				return false;
			if (std::find(facts.auras.begin(), facts.auras.end(), rule.modifierId) ==
					facts.auras.end())
				return false;
		}
		if (rule.minLevel > 0 && facts.level < rule.minLevel)
			return false;
		if (rule.maxLevel > 0 && facts.level > rule.maxLevel)
			return false;
		return true;
	}

	void Apply(const Rule& rule, MapIconStyle& style) {
		if (rule.setsIcon) {
			style.icon = rule.style.icon;
			style.size = rule.style.size;
			style.fillColor = rule.style.fillColor;
			style.borderColor = rule.style.borderColor;
			style.borderWidth = rule.style.borderWidth;
		}
		if (rule.line == LineOff) {
			style.line = false;
		} else if (rule.line == LineOn) {
			style.line = true;
			style.lineColor = rule.style.lineColor;
		}
	}

	MapIconStyle Resolve(const MapIconStyle& base, const std::vector<Rule>& rules,
			const Facts& facts) {
		MapIconStyle style = base;
		for (const Rule& rule : rules)
			if (Matches(rule, facts))
				Apply(rule, style);
		return style;
	}

	std::vector<unsigned int> AurasAsked(const std::vector<Rule>& rules) {
		std::vector<unsigned int> auras;
		for (const Rule& rule : rules)
			if (rule.modifierKind == Aura)
				auras.push_back(rule.modifierId);
		SortUnique(auras);
		return auras;
	}

	Rank ShownRank(const Rule& rule) {
		for (int rank = 0; rank < RankCount; rank++)
			if (rule.ranks & (1u << rank))
				return (Rank)rank;
		return Normal;
	}

	const char* RankKey(Rank rank) {
		return RankNames[rank < RankCount ? rank : Normal].key;
	}

	const char* RankLabel(Rank rank) {
		return RankNames[rank < RankCount ? rank : Normal].label;
	}

	const char* DifficultyKey(Difficulty difficulty) {
		return DifficultyNames[difficulty < DifficultyCount ? difficulty : NormalDifficulty].key;
	}

	const char* DifficultyLabel(Difficulty difficulty) {
		return DifficultyNames[difficulty < DifficultyCount ? difficulty : NormalDifficulty].label;
	}

	const std::vector<ModifierName>& Modifiers() {
		static const std::vector<ModifierName> names = {
			{ Enchantment, 5, "extra strong", "Extra strong" },
			{ Enchantment, 6, "extra fast", "Extra fast" },
			{ Enchantment, 7, "cursed", "Cursed" },
			{ Enchantment, 8, "magic resistant", "Magic resistant" },
			{ Enchantment, 9, "fire enchanted", "Fire enchanted" },
			{ Enchantment, 17, "lightning enchanted", "Lightning enchanted" },
			{ Enchantment, 18, "cold enchanted", "Cold enchanted" },
			{ Enchantment, 25, "mana burn", "Mana burn" },
			{ Enchantment, 26, "teleportation", "Teleportation" },
			{ Enchantment, 27, "spectral hit", "Spectral hit" },
			{ Enchantment, 28, "stone skin", "Stone skin" },
			{ Enchantment, 29, "multiple shots", "Multiple shots" },
			{ Enchantment, 30, "aura enchanted", "Aura enchanted" },
			{ Aura, 33, "might", "Might aura" },
			{ Aura, 35, "holy fire", "Holy fire aura" },
			{ Aura, 40, "blessed aim", "Blessed aim aura" },
			{ Aura, 43, "holy freeze", "Holy freeze aura" },
			{ Aura, 46, "holy shock", "Holy shock aura" },
			{ Aura, 28, "conviction", "Conviction aura" },
			{ Aura, 49, "fanaticism", "Fanaticism aura" },
			// What Conviction puts on those it reaches, which is the state
			// BH_settings.cfg has always shipped a colour for.
			{ Aura, 29, "convicted", "Convicted" },
		};
		return names;
	}

	std::string ModifierLabel(unsigned int kind, unsigned int id) {
		if (kind == AnyModifier)
			return "Any";
		const ModifierName* name = FindModifier(kind, id);
		if (name)
			return name->label;
		return std::string(kind == Aura ? "Aura " : "Enchantment ") + std::to_string(id);
	}

	std::string Summary(const Rule& rule, const std::vector<std::string>& targets,
			unsigned int most) {
		std::vector<std::string> parts;
		bool ranked = (rule.ranks & AllRanks) != AllRanks;
		if (!targets.empty()) {
			size_t shown = (most == 0 || targets.size() < most) ? targets.size() : most;
			std::string named;
			for (size_t i = 0; i < shown; i++)
				named += (i > 0 ? ", " : "") + targets[i];
			if (targets.size() > shown)
				named += " +" + std::to_string(targets.size() - shown);
			parts.push_back(named);
			if (most == 0 && ranked)
				parts.push_back(RankPhrase(rule.ranks));
		} else if (ranked) {
			parts.push_back(RankPhrase(rule.ranks));
		}
		if (rule.modifierKind != AnyModifier) {
			std::string label = ModifierLabel(rule.modifierKind, rule.modifierId);
			label[0] = (char)std::tolower((unsigned char)label[0]);
			parts.push_back(label);
		}
		if ((rule.difficulties & AllDifficulties) != AllDifficulties)
			parts.push_back(DifficultyPhrase(rule.difficulties));
		std::string level = LevelPhrase(rule.minLevel, rule.maxLevel);
		if (!level.empty())
			parts.push_back(level);
		if (parts.empty())
			return "Every monster";
		std::string summary;
		for (size_t i = 0; i < parts.size(); i++)
			summary += (i > 0 ? ", " : "") + parts[i];
		return Capitalised(summary);
	}

	std::string ToJson(const std::vector<Rule>& rules) {
		Json list = Json::array();
		for (const Rule& rule : rules)
			list.push_back(Encode(rule));
		// A name is whatever was typed, which need not be UTF-8.
		return list.dump(-1, ' ', false, Json::error_handler_t::replace);
	}

	bool FromJson(const std::string& text, std::vector<Rule>& rules) {
		rules.clear();
		Json list = Json::parse(text, NULL, false);
		if (!list.is_array())
			return false;
		for (const Json& entry : list)
			if (entry.is_object())
				rules.push_back(Decode(entry));
		return true;
	}

	std::vector<Rule> FromLegacy(const Legacy& legacy) {
		std::vector<Rule> rules;
		// Ahead of the settings, and so under all of them: the herd had its colour
		// before any of them was looked at.
		Rule herd = IconRule(CowKingHerdColor);
		herd.cowKingHerd = true;
		rules.push_back(herd);
		for (const auto& group : ByColor(legacy.monsterColors)) {
			Rule rule = IconRule(group.first);
			rule.monsters = group.second;
			rule.ranks = NotBoss;
			rules.push_back(rule);
		}
		for (const auto& group : ByColor(legacy.monsterLines)) {
			Rule rule;
			rule.monsters = group.second;
			rule.line = LineOn;
			rule.style.lineColor = group.first % MapIcon::PaletteSize;
			rules.push_back(rule);
		}
		// The first of these a monster carried was the one it was coloured by, so
		// they go in backwards: the last rule to match is the one that holds. A
		// colour of 0 was the setting switched off.
		for (auto it = legacy.enhancementColors.rbegin();
				it != legacy.enhancementColors.rend(); ++it) {
			if (it->second == 0)
				continue;
			Rule rule = IconRule(it->second);
			rule.modifierKind = Enchantment;
			rule.modifierId = it->first;
			rule.ranks = NotBoss;
			rules.push_back(rule);
		}
		for (const auto& group : ByColor(legacy.superUniqueColors)) {
			Rule rule = IconRule(group.first);
			rule.superUniques = group.second;
			rule.line = LineOn;
			rule.style.lineColor = group.first % MapIcon::PaletteSize;
			rules.push_back(rule);
		}
		for (auto it = legacy.auraColors.rbegin(); it != legacy.auraColors.rend(); ++it) {
			if (it->second == 0)
				continue;
			Rule rule = IconRule(it->second);
			rule.modifierKind = Aura;
			rule.modifierId = it->first;
			rules.push_back(rule);
		}
		if (!legacy.hidden.empty()) {
			Rule rule;
			rule.monsters = legacy.hidden;
			rule.setsIcon = true;
			rule.style.icon = MapIcon::None;
			rule.line = LineOff;
			rules.push_back(rule);
		}
		for (Rule& rule : rules)
			rule.Normalize();
		return rules;
	}

	RuleList::RuleList() : json("[]"), revision(0) {
	}

	std::vector<Rule> RuleList::Get() const {
		std::lock_guard<std::mutex> lock(guard);
		return rules;
	}

	void RuleList::Set(std::vector<Rule> newRules) {
		for (Rule& rule : newRules)
			rule.Normalize();
		std::string text = ToJson(newRules);
		std::lock_guard<std::mutex> lock(guard);
		if (newRules == rules)
			return;
		rules.swap(newRules);
		json.swap(text);
		revision++;
	}

	std::string RuleList::Json() const {
		std::lock_guard<std::mutex> lock(guard);
		return json;
	}

	bool RuleList::SetJson(const std::string& text) {
		std::vector<Rule> read;
		if (!FromJson(text, read))
			return false;
		Set(read);
		return true;
	}

	unsigned int RuleList::Revision() const {
		std::lock_guard<std::mutex> lock(guard);
		return revision;
	}

	MapIconStyle CrossStyle(unsigned int color) {
		MapIconStyle style;
		style.icon = MapIcon::Cross;
		style.size = CrossSize;
		style.fillColor = MapIcon::NoColor;
		style.borderColor = color;
		style.borderWidth = MapIcon::MinBorder;
		style.line = false;
		style.lineColor = color;
		return style;
	}

	bool IsOutlineOnly(const MapIconStyle& style) {
		return MapIcon::IsShape(style.Icon()) && style.Icon() != MapIcon::Circle &&
			style.fillColor == MapIcon::NoColor && style.borderColor != MapIcon::NoColor &&
			style.borderWidth == MapIcon::MinBorder;
	}
}
