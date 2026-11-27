#pragma once
#include <mutex>
#include <string>
#include <utility>
#include <vector>
#include "../../Drawing/MapIcon/MapIconStyle.h"

// How a monster is marked on the automap.
//
// Every monster starts from the marker of its rank. The rules are then gone
// through in order, and each one the monster matches changes the marker's icon,
// its line, or both, so where two change the same thing the later one has it.
//
// Pure, so it can be tested without the game. What it is told about a monster is
// read off the unit by the caller.
namespace MonsterMarks {
	// In the order they are offered in.
	enum Rank {
		Normal,
		Minion,
		Champion,
		Boss,
		RankCount
	};
	const unsigned int AllRanks = (1u << RankCount) - 1;

	// The game's three, numbered as the game numbers them.
	enum Difficulty {
		NormalDifficulty,
		Nightmare,
		Hell,
		DifficultyCount
	};
	const unsigned int AllDifficulties = (1u << DifficultyCount) - 1;

	const unsigned int EnchantSlots = 9;

	// The enchantments the Cow King's herd is told by, and the one aura
	// enchantment a monster's aura is only asked about under (MonUMod.txt ids).
	const unsigned int EnchantMagicResistant = 8;
	const unsigned int EnchantLightning = 17;
	const unsigned int EnchantAura = 30;
	const unsigned int HellBovine = 391;		// monstats.txt row

	struct Facts {
		unsigned int txtId;			// monstats.txt row
		bool boss, champion, minion;
		bool superUnique;
		unsigned int superUniqueId;	// superuniques.txt row
		unsigned int level;
		unsigned int difficulty;	// of the game it is in, a Difficulty
		unsigned char enchants[EnchantSlots];	// in the order the unit holds them, 0 for none
		std::vector<unsigned int> auras;	// states.txt rows of the auras it is under

		Facts() : txtId(0), boss(false), champion(false), minion(false),
				superUnique(false), superUniqueId(0), level(0), difficulty(NormalDifficulty) {
			for (unsigned char& enchant : enchants)
				enchant = 0;
		};

		bool HasEnchant(unsigned int id) const;
	};

	// A minion ahead of a champion ahead of a boss, where a unit is flagged as
	// more than one.
	Rank RankOf(const Facts& facts);

	// Whether the monster is one of the Hell Bovines the Cow King leads. Nothing
	// a unit holds says whose minion it is, and the cow level has other bosses
	// with herds of their own, so his are told by what they carry: his two
	// enchantments, in the slots the game copies them to his minions in.
	bool IsCowKingHerd(const Facts& facts);

	enum ModifierKind {
		AnyModifier,
		Enchantment,	// id is a MonUMod.txt id
		Aura			// id is a states.txt row; only an aura enchanted monster's is asked
	};

	enum LineChange {
		LineUnchanged,
		LineOff,
		LineOn,
		LineChangeCount
	};

	struct Rule {
		std::string name;		// empty for one named after what it matches

		// Which monsters, all of which have to hold. Naming no monsters, no super
		// uniques and not the herd is every monster; naming any, a monster has to
		// be one of those named.
		std::vector<unsigned int> monsters;		// monstats.txt rows, ascending
		std::vector<unsigned int> superUniques;	// superuniques.txt rows, ascending
		bool cowKingHerd;						// the Hell Bovines the Cow King leads
		unsigned int ranks;						// a bit for each Rank
		unsigned int difficulties;				// a bit for each Difficulty
		unsigned int modifierKind;
		unsigned int modifierId;
		unsigned int minLevel, maxLevel;		// 0 for no bound

		// What it changes. The style's icon, size, fill and border are the icon;
		// its line colour is the line's. The style's own line flag is not used.
		bool setsIcon;
		unsigned int line;						// a LineChange
		MapIconStyle style;

		Rule();

		bool operator==(const Rule& other) const;
		bool operator!=(const Rule& other) const { return !(*this == other); };

		// Puts the rule in the one form it is matched, compared and kept in: both
		// lists in order with nothing in them twice, and whatever of the style the
		// rule does not change as a new rule has it.
		void Normalize();
	};

	bool Matches(const Rule& rule, const Facts& facts);

	// The style with what the rule changes changed.
	void Apply(const Rule& rule, MapIconStyle& style);

	// The base with every rule the monster matches applied, in order.
	MapIconStyle Resolve(const MapIconStyle& base, const std::vector<Rule>& rules,
		const Facts& facts);

	// The aura states the rules ask about, each once, which are the ones a caller
	// has to look for on a monster.
	std::vector<unsigned int> AurasAsked(const std::vector<Rule>& rules);

	// The rank a rule's marker is shown on away from the map: the first it
	// applies to.
	Rank ShownRank(const Rule& rule);

	const char* RankKey(Rank rank);		// as the file writes it
	const char* RankLabel(Rank rank);	// as it is offered
	const char* DifficultyKey(Difficulty difficulty);
	const char* DifficultyLabel(Difficulty difficulty);

	// The enchantments and auras offered by name, in the order they are offered:
	// the bonuses a boss is named with, then the auras one can carry. Any other
	// id can still be matched; it has no name.
	struct ModifierName {
		ModifierKind kind;
		unsigned int id;
		const char* key;		// as the file writes it
		const char* label;		// as it is offered
	};
	const std::vector<ModifierName>& Modifiers();

	// What a modifier is called, "Enchantment 23" for one with no name.
	std::string ModifierLabel(unsigned int kind, unsigned int id);

	// What a rule is called where it has not been given a name: what it matches,
	// in a few words. targets is what its monsters and super uniques are called,
	// which only the game's tables say. No more than most of them are named, and
	// the rest counted; 0 names them all, and says the rule's ranks as well,
	// which is the whole of what it matches.
	std::string Summary(const Rule& rule, const std::vector<std::string>& targets,
		unsigned int most = 2);

	// The rules as the text of a JSON array, and back. A rule holds only what it
	// asks or changes:
	//
	//   [ { "name": "Souls", "monsters": [118, 119], "ranks": ["normal", "minion"],
	//       "difficulties": ["hell"], "enchantment": "cursed", "minLevel": 85,
	//       "icon": "cross", "size": 4, "fill": "none", "border": "0x9B",
	//       "borderWidth": 1, "line": "drawn", "lineColor": "0x9B" } ]
	//
	// Reading answers false for text that is not an array, leaving the rules
	// empty. An entry that is not an object is passed over, and a field that
	// cannot be read is as though it were not there.
	std::string ToJson(const std::vector<Rule>& rules);
	bool FromJson(const std::string& text, std::vector<Rule>& rules);

	// What BH_settings.cfg has always been able to say about monsters, each as
	// (id, colour) in the order the file gives them.
	struct Legacy {
		std::vector<std::pair<unsigned int, unsigned int>> monsterColors;		// Monster Color[row]
		std::vector<std::pair<unsigned int, unsigned int>> monsterLines;		// Monster Line[row]
		std::vector<unsigned int> hidden;										// Monster Hide[row]
		std::vector<std::pair<unsigned int, unsigned int>> enhancementColors;	// Enhancement Color[id]
		std::vector<std::pair<unsigned int, unsigned int>> superUniqueColors;	// Super Unique Color[row]
		std::vector<std::pair<unsigned int, unsigned int>> auraColors;			// Aura Color[state]
	};

	// The rules that mark monsters as BH did before it had rules: the Cow King's
	// herd in a colour of its own, which it had whatever the settings said, then
	// what the settings say, entries that say the same thing gathered into one
	// rule. They are ordered as the settings took precedence: a monster's colour,
	// then an enchantment's, a super unique's and an aura's, with hiding over all
	// of them.
	std::vector<Rule> FromLegacy(const Legacy& legacy);

	// The monster X as it has always been drawn: an outline, in one colour.
	MapIconStyle CrossStyle(unsigned int color);

	// Whether the style is a shape drawn as nothing but its one pixel outline,
	// which can be drawn as lines.
	bool IsOutlineOnly(const MapIconStyle& style);

	// The rules, held for the map that draws by them and the window that
	// edits them, which need not be on one thread: each takes a copy and gives a
	// whole list back.
	class RuleList {
		private:
			mutable std::mutex guard;
			std::vector<Rule> rules;
			std::string json;		// the rules as ToJson() writes them, kept in step
			unsigned int revision;
		public:
			RuleList();

			std::vector<Rule> Get() const;

			// Takes the rules, each put in its one form. Nothing changes where
			// they are the rules already held.
			void Set(std::vector<Rule> newRules);

			// The rules as JSON text, which costs a copy and no more: it is asked
			// every frame by whatever watches for a change.
			std::string Json() const;

			// Takes the rules FromJson() reads, and answers whether it could.
			bool SetJson(const std::string& text);

			// Moves on each time the rules change, so a copy can be told from
			// what is held without comparing the two.
			unsigned int Revision() const;
	};
}
