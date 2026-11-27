#include "doctest.h"
#include <string>
#include <vector>
#include "Modules/Maphack/MonsterMarks.h"
#include "Modules/Maphack/MonsterNames.h"

/*
 * How a monster is marked on the map: the marker it starts from, the rules
 * it matches, and how those are kept in the Forge's file.
 */

using namespace MonsterMarks;

namespace {

Facts Monster(unsigned int txtId, unsigned int level = 1) {
	Facts facts;
	facts.txtId = txtId;
	facts.level = level;
	return facts;
}

Facts BossOf(unsigned int txtId) {
	Facts facts = Monster(txtId);
	facts.boss = true;
	return facts;
}

Rule Colour(unsigned int color) {
	Rule rule;
	rule.setsIcon = true;
	rule.style = CrossStyle(color);
	return rule;
}

const unsigned int Gloam = 118;
const unsigned int CowKing = 39;

}  // namespace

TEST_CASE("A monster's rank is its flags, a minion ahead of a champion ahead of a boss") {
	Facts facts = Monster(1);
	CHECK(RankOf(facts) == Normal);
	facts.boss = true;
	CHECK(RankOf(facts) == Boss);
	facts.champion = true;
	CHECK(RankOf(facts) == Champion);
	facts.minion = true;
	CHECK(RankOf(facts) == Minion);
}

TEST_CASE("The Cow King's herd is the hell bovines carrying his enchantments") {
	Facts cow = Monster(HellBovine);
	cow.minion = true;
	CHECK_FALSE(IsCowKingHerd(cow));
	cow.enchants[0] = EnchantMagicResistant;
	cow.enchants[1] = EnchantLightning;
	CHECK_FALSE(IsCowKingHerd(cow));
	cow.enchants[3] = 5;
	CHECK(IsCowKingHerd(cow));
	CHECK(RankOf(cow) == Minion);

	// Another boss's herd carries that boss's enchantments, in its order.
	Facts stray = cow;
	stray.enchants[0] = EnchantLightning;
	stray.enchants[1] = EnchantMagicResistant;
	CHECK_FALSE(IsCowKingHerd(stray));

	Facts other = cow;
	other.txtId = Gloam;
	CHECK_FALSE(IsCowKingHerd(other));
}

TEST_CASE("A rule that names the herd matches it, and no other hell bovine") {
	Rule rule;
	rule.cowKingHerd = true;
	Facts cow = Monster(HellBovine);
	cow.minion = true;
	CHECK_FALSE(Matches(rule, cow));
	cow.enchants[0] = EnchantMagicResistant;
	cow.enchants[1] = EnchantLightning;
	cow.enchants[3] = 5;
	CHECK(Matches(rule, cow));

	// Named beside other monsters, it is one more of those the rule is for.
	rule.monsters = { Gloam };
	CHECK(Matches(rule, cow));
	CHECK(Matches(rule, Monster(Gloam)));
	CHECK_FALSE(Matches(rule, Monster(HellBovine)));

	std::vector<Rule> read;
	REQUIRE(FromJson(ToJson({ rule }), read));
	REQUIRE(read.size() == 1);
	CHECK(read[0].cowKingHerd);
	CHECK(ToJson({ rule }) == R"([{"monsters":[118],"cowKingHerd":true}])");
}

TEST_CASE("A rule that asks nothing matches every monster, at any level") {
	Rule rule;
	CHECK(Matches(rule, Monster(Gloam, 1)));
	CHECK(Matches(rule, Monster(7, 99)));
	CHECK(Matches(rule, BossOf(7)));
}

TEST_CASE("A rule matches only the ranks it names") {
	Rule rule;
	rule.ranks = (1u << Champion) | (1u << Boss);
	Facts champion = Monster(1);
	champion.champion = true;
	CHECK_FALSE(Matches(rule, Monster(1)));
	CHECK(Matches(rule, champion));
	CHECK(Matches(rule, BossOf(1)));
	rule.ranks = 0;
	CHECK_FALSE(Matches(rule, BossOf(1)));
}

TEST_CASE("A rule that names monsters matches those, and a super unique it names") {
	Rule rule;
	rule.monsters = { 118, 120 };
	rule.superUniques = { CowKing };
	CHECK(Matches(rule, Monster(118)));
	CHECK(Matches(rule, Monster(120)));
	CHECK_FALSE(Matches(rule, Monster(119)));

	Facts king = BossOf(HellBovine);
	CHECK_FALSE(Matches(rule, king));
	king.superUnique = true;
	king.superUniqueId = CowKing;
	CHECK(Matches(rule, king));

	// A row number means nothing as a super unique unless the monster is one.
	Facts plain = Monster(7);
	plain.superUniqueId = CowKing;
	CHECK_FALSE(Matches(rule, plain));
}

TEST_CASE("A rule matches a level inside its bounds, and any level without them") {
	Rule rule;
	rule.minLevel = 85;
	CHECK_FALSE(Matches(rule, Monster(1, 84)));
	CHECK(Matches(rule, Monster(1, 85)));
	CHECK(Matches(rule, Monster(1, 110)));
	rule.maxLevel = 90;
	CHECK(Matches(rule, Monster(1, 90)));
	CHECK_FALSE(Matches(rule, Monster(1, 91)));
	rule.minLevel = 0;
	CHECK(Matches(rule, Monster(1, 1)));
}

TEST_CASE("A rule matches in the difficulties it names, and in every one until it names any") {
	Rule rule;
	Facts facts = Monster(1);
	for (unsigned int difficulty = 0; difficulty < DifficultyCount; difficulty++) {
		facts.difficulty = difficulty;
		CHECK(Matches(rule, facts));
	}

	rule.difficulties = (1u << Nightmare) | (1u << Hell);
	facts.difficulty = NormalDifficulty;
	CHECK_FALSE(Matches(rule, facts));
	facts.difficulty = Nightmare;
	CHECK(Matches(rule, facts));
	facts.difficulty = Hell;
	CHECK(Matches(rule, facts));

	rule.difficulties = 0;
	CHECK_FALSE(Matches(rule, facts));
}

TEST_CASE("An enchantment is matched in whichever slot the monster carries it") {
	Rule rule;
	rule.modifierKind = Enchantment;
	rule.modifierId = 7;
	Facts facts = BossOf(1);
	CHECK_FALSE(Matches(rule, facts));
	facts.enchants[4] = 7;
	CHECK(Matches(rule, facts));
}

TEST_CASE("An aura is matched only on a monster that is aura enchanted") {
	Rule rule;
	rule.modifierKind = Aura;
	rule.modifierId = 28;
	Facts facts = BossOf(1);
	facts.auras = { 28 };
	CHECK_FALSE(Matches(rule, facts));
	facts.enchants[2] = EnchantAura;
	CHECK(Matches(rule, facts));
	facts.auras = { 33 };
	CHECK_FALSE(Matches(rule, facts));
}

TEST_CASE("The auras asked about are each rule's, once") {
	Rule conviction;
	conviction.modifierKind = Aura;
	conviction.modifierId = 28;
	Rule might = conviction;
	might.modifierId = 33;
	Rule cursed;
	cursed.modifierKind = Enchantment;
	cursed.modifierId = 7;
	CHECK(AurasAsked({ might, cursed, conviction, might }) ==
		std::vector<unsigned int>({ 28, 33 }));
}

TEST_CASE("A rule changes the icon, the line, or both, and leaves the rest") {
	MapIconStyle base = CrossStyle(0x5B);
	base.line = true;
	base.lineColor = 0x20;

	Rule icon = Colour(0x9B);
	MapIconStyle styled = base;
	Apply(icon, styled);
	CHECK(styled.borderColor == 0x9B);
	CHECK(styled.line);
	CHECK(styled.lineColor == 0x20);

	Rule off;
	off.line = LineOff;
	styled = base;
	Apply(off, styled);
	CHECK_FALSE(styled.line);
	CHECK(styled.borderColor == 0x5B);

	Rule on;
	on.line = LineOn;
	on.style.lineColor = 0x84;
	styled = CrossStyle(0x5B);
	Apply(on, styled);
	CHECK(styled.line);
	CHECK(styled.lineColor == 0x84);
	CHECK(styled.borderColor == 0x5B);
}

TEST_CASE("Where two rules change the same thing, the later one has it") {
	Rule first = Colour(0x9B);
	first.line = LineOn;
	first.style.lineColor = 0x9B;
	Rule second = Colour(0x0C);
	Rule elsewhere = Colour(0x01);
	elsewhere.monsters = { 999 };

	MapIconStyle style = Resolve(CrossStyle(0x5B), { first, second, elsewhere }, Monster(Gloam));
	CHECK(style.borderColor == 0x0C);
	CHECK(style.line);
	CHECK(style.lineColor == 0x9B);

	style = Resolve(CrossStyle(0x5B), {}, Monster(Gloam));
	CHECK(style == CrossStyle(0x5B));
}

TEST_CASE("A rule with no icon and no line leaves nothing on the map") {
	Rule hide;
	hide.setsIcon = true;
	hide.style.icon = MapIcon::None;
	hide.line = LineOff;
	MapIconStyle base = CrossStyle(0x5B);
	base.line = true;
	CHECK_FALSE(Resolve(base, { hide }, Monster(1)).Draws());
}

TEST_CASE("The monster X is an outline, which is what is drawn as lines") {
	MapIconStyle cross = CrossStyle(0x5B);
	CHECK(cross.Draws());
	CHECK(IsOutlineOnly(cross));

	MapIconStyle filled = cross;
	filled.fillColor = 0x20;
	CHECK_FALSE(IsOutlineOnly(filled));

	MapIconStyle wide = cross;
	wide.borderWidth = 2;
	CHECK_FALSE(IsOutlineOnly(wide));

	MapIconStyle circle = cross;
	circle.icon = MapIcon::Circle;
	CHECK_FALSE(IsOutlineOnly(circle));

	MapIconStyle none = cross;
	none.icon = MapIcon::None;
	CHECK_FALSE(IsOutlineOnly(none));
}

TEST_CASE("A rule is written with only what it asks and changes") {
	CHECK(ToJson({}) == "[]");
	CHECK(ToJson({ Rule() }) == "[{}]");

	Rule rule = Colour(0x9B);
	rule.name = "Souls";
	rule.monsters = { 118, 119 };
	rule.superUniques = { 33 };
	rule.ranks = (1u << Normal) | (1u << Minion);
	rule.difficulties = 1u << Hell;
	rule.modifierKind = Enchantment;
	rule.modifierId = 7;
	rule.minLevel = 85;
	rule.line = LineOn;
	rule.style.lineColor = 0x84;
	CHECK(ToJson({ rule }) ==
		R"([{"name":"Souls","monsters":[118,119],"superUniques":[33],)"
		R"("ranks":["normal","minion"],"difficulties":["hell"],)"
		R"("enchantment":"cursed","minLevel":85,)"
		R"("icon":"cross","size":4,"fill":"none","border":"0x9B","borderWidth":1,)"
		R"("line":"drawn","lineColor":"0x84"}])");
}

TEST_CASE("Rules read back as they were written") {
	Rule souls = Colour(0x9B);
	souls.name = "Souls";
	souls.monsters = { 118, 119 };
	souls.ranks = (1u << Normal) | (1u << Minion);
	souls.difficulties = (1u << NormalDifficulty) | (1u << Nightmare);
	souls.maxLevel = 90;

	Rule aura;
	aura.modifierKind = Aura;
	aura.modifierId = 28;
	aura.line = LineOff;

	Rule unnamed;
	unnamed.modifierKind = Enchantment;
	unnamed.modifierId = 23;
	unnamed.setsIcon = true;
	unnamed.style.icon = MapIcon::Circle;
	unnamed.style.size = 2;
	unnamed.style.fillColor = 0x20;
	unnamed.style.borderColor = MapIcon::NoColor;
	unnamed.line = LineOn;
	unnamed.style.lineColor = 0x0A;

	std::vector<Rule> written = { souls, aura, unnamed, Rule() };
	for (Rule& rule : written)
		rule.Normalize();
	std::vector<Rule> read;
	REQUIRE(FromJson(ToJson(written), read));
	CHECK(read == written);
}

TEST_CASE("A rule is read by names in any case, by numbers, and around what it cannot read") {
	std::vector<Rule> rules;
	REQUIRE(FromJson(R"([
		{ "ranks": ["BOSS", "nobody"], "aura": " Conviction ", "icon": "Star",
		  "size": 99, "fill": 151, "border": "not a colour", "line": "Drawn" },
		{ "enchantment": 23, "monsters": [120, "x", 118, 120], "minLevel": -4,
		  "difficulties": ["HELL", 2, "easy"] },
		"not a rule",
		{ "icon": "no such icon", "fill": "0x20" }
	])", rules));
	REQUIRE(rules.size() == 3);

	CHECK(rules[0].ranks == (1u << Boss));
	CHECK(rules[0].modifierKind == Aura);
	CHECK(rules[0].modifierId == 28);
	CHECK(rules[0].setsIcon);
	CHECK(rules[0].style.icon == MapIcon::Star);
	CHECK(rules[0].style.size == MapIcon::MaxSize);
	CHECK(rules[0].style.fillColor == 151);
	// A border that cannot be read is left as a new rule has it.
	CHECK(rules[0].style.borderColor == Rule().style.borderColor);
	CHECK(rules[0].line == LineOn);

	CHECK(rules[1].modifierKind == Enchantment);
	CHECK(rules[1].modifierId == 23);
	CHECK(rules[1].monsters == std::vector<unsigned int>({ 118, 120 }));
	CHECK(rules[1].minLevel == 0);
	CHECK(rules[1].ranks == AllRanks);
	CHECK(rules[1].difficulties == (1u << Hell));
	CHECK(rules[0].difficulties == AllDifficulties);

	// An icon that cannot be read is no icon set, whatever else is given for it.
	CHECK_FALSE(rules[2].setsIcon);
}

TEST_CASE("Text that is not a list of rules reads as no rules") {
	std::vector<Rule> rules = { Rule() };
	CHECK_FALSE(FromJson("{ \"minLevel\": 3 }", rules));
	CHECK(rules.empty());
	rules = { Rule() };
	CHECK_FALSE(FromJson("[ {", rules));
	CHECK(rules.empty());
}

TEST_CASE("The old settings become rules that mark monsters as they did") {
	Legacy legacy;
	legacy.monsterColors = { { 118, 0x9B }, { 212, 0x9B }, { 5, 0x20 } };
	legacy.monsterLines = { { 333, 0x9B } };
	legacy.hidden = { 44 };
	legacy.enhancementColors = { { 25, 0xAB }, { 7, 0x70 }, { 17, 0 } };
	legacy.superUniqueColors = { { 39, 0x9B }, { 36, 0x9E }, { 37, 0x9E } };
	legacy.auraColors = { { 29, 0x0C } };
	std::vector<Rule> rules = FromLegacy(legacy);
	MapIconStyle normal = CrossStyle(0x5B), boss = CrossStyle(0x84);

	// A monster's colour, for all of it but its bosses.
	CHECK(Resolve(normal, rules, Monster(118)).borderColor == 0x9B);
	CHECK(Resolve(normal, rules, Monster(212)).borderColor == 0x9B);
	CHECK(Resolve(normal, rules, Monster(5)).borderColor == 0x20);
	CHECK(Resolve(boss, rules, BossOf(118)).borderColor == 0x84);
	CHECK_FALSE(Resolve(normal, rules, Monster(118)).line);

	// A line, and the colour left as it was.
	MapIconStyle clone = Resolve(boss, rules, BossOf(333));
	CHECK(clone.line);
	CHECK(clone.lineColor == 0x9B);
	CHECK(clone.borderColor == 0x84);

	// The first enchantment listed is the one that colours, over the monster's
	// own colour, and never a boss.
	Facts pack = Monster(118);
	pack.minion = true;
	pack.enchants[0] = 7;
	pack.enchants[1] = 25;
	CHECK(Resolve(normal, rules, pack).borderColor == 0xAB);
	Facts burner = BossOf(1);
	burner.enchants[0] = 25;
	CHECK(Resolve(boss, rules, burner).borderColor == 0x84);

	// A super unique's colour and line, over an enchantment's.
	Facts seis = BossOf(1);
	seis.superUnique = true;
	seis.superUniqueId = 37;
	MapIconStyle seisStyle = Resolve(boss, rules, seis);
	CHECK(seisStyle.borderColor == 0x9E);
	CHECK(seisStyle.line);
	CHECK(seisStyle.lineColor == 0x9E);

	// An aura over all of those, keeping the super unique's line.
	seis.enchants[0] = EnchantAura;
	seis.auras = { 29 };
	seisStyle = Resolve(boss, rules, seis);
	CHECK(seisStyle.borderColor == 0x0C);
	CHECK(seisStyle.lineColor == 0x9E);

	// Hidden is hidden whatever else it is.
	Facts hidden = BossOf(44);
	hidden.enchants[0] = EnchantAura;
	hidden.auras = { 29 };
	CHECK_FALSE(Resolve(boss, rules, hidden).Draws());

	// The herd keeps the colour it always had, under whatever the settings say
	// of hell bovines.
	Facts herd = Monster(HellBovine);
	herd.minion = true;
	herd.enchants[0] = EnchantMagicResistant;
	herd.enchants[1] = EnchantLightning;
	herd.enchants[3] = 5;
	CHECK(Resolve(CrossStyle(0x60), rules, herd).borderColor == 0xE1);
	Legacy cows = legacy;
	cows.monsterColors.push_back({ HellBovine, 0x20 });
	CHECK(Resolve(CrossStyle(0x60), FromLegacy(cows), herd).borderColor == 0x20);

	// One rule for each thing said: the herd, two colours, a line, two
	// enchantments, two super unique colours, an aura and the hidden.
	CHECK(rules.size() == 10);
}

TEST_CASE("With no old settings the herd's is the only rule") {
	std::vector<Rule> rules = FromLegacy(Legacy());
	REQUIRE(rules.size() == 1);
	CHECK(rules[0].cowKingHerd);
	CHECK(rules[0].monsters.empty());
	CHECK(rules[0].setsIcon);
	CHECK(rules[0].style.icon == MapIcon::Cross);
	CHECK(rules[0].style.borderColor == 0xE1);
	CHECK(rules[0].line == LineUnchanged);
}

TEST_CASE("A rule with no name is called by what it matches") {
	Rule rule;
	CHECK(Summary(rule, {}) == "Every monster");

	rule.ranks = 1u << Boss;
	rule.modifierKind = Enchantment;
	rule.modifierId = 7;
	rule.minLevel = 85;
	CHECK(Summary(rule, {}) == "Bosses, cursed, level 85+");

	rule = Rule();
	rule.ranks = AllRanks & ~(1u << Boss);
	rule.modifierKind = Enchantment;
	rule.modifierId = 25;
	CHECK(Summary(rule, {}) == "All but bosses, mana burn");

	rule = Rule();
	rule.ranks = (1u << Champion) | (1u << Boss);
	rule.minLevel = 80;
	rule.maxLevel = 85;
	CHECK(Summary(rule, {}) == "Champions and bosses, level 80-85");

	rule = Rule();
	rule.ranks = 1u << Boss;
	rule.difficulties = (1u << Nightmare) | (1u << Hell);
	CHECK(Summary(rule, {}) == "Bosses, in Nightmare and Hell");
	rule.difficulties = 1u << Hell;
	rule.minLevel = 90;
	CHECK(Summary(rule, { "Diablo" }) == "Diablo, in Hell, level 90+");

	rule = Rule();
	rule.maxLevel = 20;
	CHECK(Summary(rule, {}) == "Up to level 20");
	rule.minLevel = 20;
	CHECK(Summary(rule, {}) == "Level 20");

	rule = Rule();
	rule.modifierKind = Aura;
	rule.modifierId = 77;
	CHECK(Summary(rule, {}) == "Aura 77");

	// What it names says more than its ranks do.
	rule = Rule();
	rule.ranks = 1u << Normal;
	CHECK(Summary(rule, { "Gloam" }) == "Gloam");
	CHECK(Summary(rule, { "Gloam", "Burning Soul" }) == "Gloam, Burning Soul");
	CHECK(Summary(rule, { "Gloam", "Burning Soul", "Black Soul", "Swamp Ghost" }) ==
		"Gloam, Burning Soul +2");

	// Said in full, every one is named, and its ranks with them.
	CHECK(Summary(rule, { "Gloam", "Burning Soul", "Black Soul" }, 0) ==
		"Gloam, Burning Soul, Black Soul, normal monsters");
	rule.ranks = AllRanks;
	rule.maxLevel = 30;
	CHECK(Summary(rule, { "Gloam", "Burning Soul", "Black Soul" }, 0) ==
		"Gloam, Burning Soul, Black Soul, up to level 30");
}

namespace {

const char* const MonStats =
	"Id\thcIdx\tNameStr\tenabled\tnpc\tAlign\tkillable\r\n"
	"skeleton1\t0\tSkeleton\t1\t\t\t1\r\n"
	"willowisp3\t120\tBurningSoul\t1\t\t\t1\r\n"
	"akara\t148\tAkara\t1\t1\t\t\r\n"
	"necroskeleton\t363\tNecroSkeleton\t1\t\t1\t1\r\n"
	"unused\t400\tUnused\t\t\t\t1\r\n"
	"trap\t401\tTrap\t1\t\t\t\r\n"
	"dummy1\t402\tDummy\t1\t\t\t1\r\n"
	"willowisp7\t641\tBurningSoul\t1\t\t\t1\r\n"
	"diablo\t243\tDiablo\t1\t\t\t1\r\n"
	"diabloclone\t333\tDiablo\t1\t\t\t1\r\n"
	"Expansion\r\n"
	"uberdiablo\t705\tDiablo\t1\t\t\t1\r\n"
	"UberAndariel\t707\tPandora\t1\t\t\t1\r\n"
	"baalhighpriest\t557\tBaalHighPriest\t1\t\t\t1\r\n";

const char* const SuperUniques =
	"Superunique\tName\tClass\thcIdx\r\n"
	"Rakanishu\tRakanishu\tfallen2\t3\r\n"
	"Expansion\t\t\t\r\n"
	"Bishibosh\tBishibosh\tfallenshaman1\t0\r\n"
	"Baal Subject 3\tBaal Subject 3\tbaalhighpriest\t63\r\n";

std::string Words(const std::string& key) {
	if (key == "BurningSoul") return "Burning Soul";
	if (key == "NecroSkeleton") return "Skeleton";
	if (key == "Dummy") return "dummy";
	if (key == "BaalHighPriest") return "Council Member";
	if (key == "Pandora") return "Lilith";
	if (key == "Diablo") return "Diablo";
	if (key == "Baal Subject 3") return "Bartuc the Bloody";
	if (key == "Unused" || key == "Trap" || key == "Akara" || key == "Skeleton" ||
			key == "Rakanishu")
		return key;
	return std::string();
}

}  // namespace

TEST_CASE("The monsters to pick from are the ones a player fights, a name once") {
	std::vector<MonsterNames::Entry> entries = MonsterNames::Build(MonStats, SuperUniques, Words);
	REQUIRE(entries.size() == 11);

	// The ubers first, then monsters by name, the rows that share one together,
	// then super uniques, the Cow King's herd among them.
	CHECK(entries[3].name == "Burning Soul");
	CHECK(entries[3].ids == std::vector<unsigned int>({ 120, 641 }));
	CHECK(entries[3].kind == MonsterNames::Monster);
	CHECK(entries[3].search == "burning soul");
	CHECK(entries[4].name == "Council Member");
	CHECK(entries[6].name == "Skeleton");
	// The Necromancer's skeleton is on the player's side, so it is not one.
	CHECK(entries[6].ids == std::vector<unsigned int>({ 0 }));

	CHECK(entries[7].name == "Bartuc the Bloody");
	CHECK(entries[7].kind == MonsterNames::SuperUnique);
	CHECK(entries[7].ids == std::vector<unsigned int>({ 63 }));
	// A key the strings do not have is shown as it is.
	CHECK(entries[8].name == "Bishibosh");
	CHECK(entries[9].name == "Cow King's herd");
	CHECK(entries[9].kind == MonsterNames::Herd);
	CHECK(entries[9].ids.empty());
	CHECK(entries[9].search.find("cow king") != std::string::npos);
	CHECK(entries[10].name == "Rakanishu");
}

TEST_CASE("An uber is an entry of its own, apart from the boss the game names it after") {
	std::vector<MonsterNames::Entry> entries = MonsterNames::Build(MonStats, SuperUniques, Words);
	REQUIRE(entries.size() == 11);

	CHECK(entries[0].name == "Diablo Clone");
	CHECK(entries[0].kind == MonsterNames::Uber);
	CHECK(entries[0].ids == std::vector<unsigned int>({ 333 }));
	// Its row is found whatever case the table keys it in, and it keeps the
	// table's own name where that is nobody else's.
	CHECK(entries[1].name == "Lilith");
	CHECK(entries[1].kind == MonsterNames::Uber);
	CHECK(entries[2].name == "Pandemonium Diablo");
	CHECK(entries[2].ids == std::vector<unsigned int>({ 705 }));

	// The act boss is left with its own row alone.
	CHECK(entries[5].name == "Diablo");
	CHECK(entries[5].kind == MonsterNames::Monster);
	CHECK(entries[5].ids == std::vector<unsigned int>({ 243 }));

	// Every one of them is found by what players call the lot.
	for (int uber = 0; uber < 3; uber++)
		CHECK(entries[uber].search.find("uber") != std::string::npos);
	CHECK(entries[1].search.find("pandemonium") != std::string::npos);
	CHECK(entries[0].search.find("pandemonium") == std::string::npos);
	CHECK(entries[5].search.find("uber") == std::string::npos);

	CHECK(MonsterNames::NamesOf(entries, { 243, 333, 705 }, {}) ==
		std::vector<std::string>({ "Diablo Clone", "Pandemonium Diablo", "Diablo" }));
}

TEST_CASE("Rows are named by the entries that hold them, and by number where none does") {
	std::vector<MonsterNames::Entry> entries = MonsterNames::Build(MonStats, SuperUniques, Words);
	CHECK(MonsterNames::NamesOf(entries, { 641, 0, 120 }, { 3 }) ==
		std::vector<std::string>({ "Burning Soul", "Skeleton", "Rakanishu" }));
	CHECK(MonsterNames::NamesOf(entries, { 9999 }, { 77 }) ==
		std::vector<std::string>({ "Monster 9999", "Super unique 77" }));
	CHECK(MonsterNames::NamesOf(entries, {}, {}).empty());

	// The herd is named where it is asked for, with or without the tables.
	CHECK(MonsterNames::NamesOf(entries, { 0 }, { 3 }, true) ==
		std::vector<std::string>({ "Skeleton", "Cow King's herd", "Rakanishu" }));
	CHECK(MonsterNames::NamesOf({}, {}, {}, true) ==
		std::vector<std::string>({ "Cow King's herd" }));
}
