#include "SplitRoute.h"
#include <cctype>
#include "nlohmann/json.hpp"

// Ordered, so a route is written the way it reads: its name, then its splits.
typedef nlohmann::ordered_json Json;

namespace Splits {

namespace {
	const unsigned int kFileVersion = 1;

	const char* const kDifficulties[] = { "normal", "nightmare", "hell" };
	const char* const kSuffixes[] = { "", " (NM)", " (Hell)" };

	// The game's rows for the act bosses and the quests that killing them
	// completes. Duriel's quest is not done until Jerhyn has been spoken to, so
	// it is the bit his death sets that is watched for.
	struct ActBoss {
		const char* name;
		unsigned int classId;
		unsigned int quest;
		unsigned int mask;
	};
	const ActBoss kActBosses[] = {
		{ "Andariel", 156, 6, kQuestDone },
		{ "Duriel", 211, 14, 0x0020 },
		{ "Mephisto", 242, 22, kQuestDone },
		{ "Diablo", 243, 26, kQuestDone },
		{ "Baal", 544, 40, kQuestDone },
	};

	// The Cow King's death is kept as a bit of The Search for Cain.
	const unsigned int kCainQuest = 4;
	const unsigned int kCowKingKilled = 0x0400;

	Trigger Of(Trigger::Kind kind, unsigned int value, unsigned int mask = 0) {
		return { kind, value, mask, std::string() };
	}

	Trigger Holding(const char* code) {
		return { Trigger::Item, 0, 0, code };
	}

	Split BossSplit(const ActBoss& boss, int difficulty) {
		return { std::string(boss.name) + kSuffixes[difficulty], difficulty, false,
			{ Of(Trigger::Boss, boss.classId), Of(Trigger::Quest, boss.quest, boss.mask) } };
	}

	Split Normal(const char* name, std::vector<Trigger> when) {
		return { name, 0, false, when };
	}

	Route ActBossRoute(const char* id, const char* name, int lastDifficulty) {
		Route route = { id, name, {} };
		for (int difficulty = 0; difficulty <= lastDifficulty; difficulty++) {
			for (const ActBoss& boss : kActBosses)
				route.splits.push_back(BossSplit(boss, difficulty));
		}
		return route;
	}

	Route Milestones() {
		const ActBoss* boss = kActBosses;
		return { "milestones-normal", "Milestones (Normal)", {
			BossSplit(boss[0], 0),
			Normal("Radament", { Of(Trigger::Boss, 229), Of(Trigger::Quest, 9, kQuestDone) }),
			Normal("Horadric Cube", { Holding("box") }),
			Normal("Staff of Kings", { Holding("msf") }),
			Normal("Viper Amulet", { Holding("vip") }),
			Normal("Summoner", { Of(Trigger::Boss, 250), Of(Trigger::Quest, 13, kQuestDone) }),
			BossSplit(boss[1], 0),
			Normal("Khalim's Will", { Holding("qf2") }),
			Normal("Travincal", { Of(Trigger::Quest, 21, kQuestDone) }),
			BossSplit(boss[2], 0),
			Normal("Izual", { Of(Trigger::Boss, 256), Of(Trigger::Quest, 25, kQuestDone) }),
			Normal("Hellforge", { Of(Trigger::Quest, 27, kQuestDone) }),
			BossSplit(boss[3], 0),
			Normal("Shenk", { Of(Trigger::SuperUnique, 42), Of(Trigger::Quest, 35, kQuestDone) }),
			Normal("Anya", { Of(Trigger::Quest, 37, kQuestDone) }),
			Normal("Ancients", { Of(Trigger::Quest, 39, kQuestDone) }),
			BossSplit(boss[4], 0),
		} };
	}

	// One goal reached in any order and the run done when all of them are, which
	// is what the last split, wanting every trigger, says.
	Route Completionist() {
		const Trigger quests = Of(Trigger::AllQuests, 0);
		const Trigger cowKing = Of(Trigger::Quest, kCainQuest, kCowKingKilled);
		const Trigger level = Of(Trigger::Level, 40);
		return { "completionist-normal", "Completionist (Normal)", {
			Normal("All quests", { quests }),
			Normal("Cow King", { cowKing }),
			Normal("Level 40", { level }),
			{ "Complete", 0, true, { quests, cowKing, level } },
		} };
	}

	std::string Lower(std::string text) {
		for (char& c : text)
			c = (char)std::tolower((unsigned char)c);
		return text;
	}

	Json WriteTrigger(const Trigger& trigger) {
		Json json = Json::object();
		switch (trigger.kind) {
			case Trigger::Boss:			json["boss"] = trigger.value; break;
			case Trigger::SuperUnique:	json["superunique"] = trigger.value; break;
			case Trigger::Quest:		json["quest"] = trigger.value; json["mask"] = trigger.mask; break;
			case Trigger::Area:			json["area"] = trigger.value; break;
			case Trigger::Level:		json["level"] = trigger.value; break;
			case Trigger::Item:			json["item"] = trigger.code; break;
			case Trigger::AllQuests:	json["allQuests"] = true; break;
		}
		return json;
	}

	bool Number(const Json& json, const char* key, unsigned int& into) {
		Json::const_iterator found = json.find(key);
		if (found == json.end() || !found->is_number_unsigned())
			return false;
		into = found->get<unsigned int>();
		return true;
	}

	bool ReadTrigger(const Json& json, Trigger& into) {
		if (!json.is_object())
			return false;
		into = Trigger();
		into.value = 0;
		into.mask = 0;
		if (Number(json, "boss", into.value)) {
			into.kind = Trigger::Boss;
		} else if (Number(json, "superunique", into.value)) {
			into.kind = Trigger::SuperUnique;
		} else if (Number(json, "quest", into.value)) {
			into.kind = Trigger::Quest;
			// A quest named without bits means the quest done.
			if (!Number(json, "mask", into.mask) || into.mask == 0)
				into.mask = kQuestDone;
		} else if (Number(json, "area", into.value)) {
			into.kind = Trigger::Area;
		} else if (Number(json, "level", into.value)) {
			into.kind = Trigger::Level;
		} else if (json.contains("item") && json["item"].is_string() &&
				!json["item"].get<std::string>().empty()) {
			into.kind = Trigger::Item;
			into.code = json["item"].get<std::string>();
		} else if (json.contains("allQuests") && json["allQuests"].is_boolean() &&
				json["allQuests"].get<bool>()) {
			into.kind = Trigger::AllQuests;
		} else {
			return false;
		}
		return true;
	}

	int ReadDifficulty(const Json& json) {
		Json::const_iterator found = json.find("difficulty");
		if (found == json.end() || !found->is_string())
			return -1;
		const std::string named = Lower(found->get<std::string>());
		for (int difficulty = 0; difficulty < 3; difficulty++) {
			if (named == kDifficulties[difficulty])
				return difficulty;
		}
		return -1;
	}

	bool ReadSplit(const Json& json, Split& into) {
		if (!json.is_object() || !json.contains("name") || !json["name"].is_string())
			return false;
		into = Split();
		into.name = json["name"].get<std::string>();
		into.difficulty = ReadDifficulty(json);
		into.all = json.contains("all") && json["all"].is_boolean() && json["all"].get<bool>();

		Json::const_iterator when = json.find("when");
		if (when != json.end() && when->is_array()) {
			for (const Json& each : *when) {
				Trigger trigger;
				if (ReadTrigger(each, trigger))
					into.when.push_back(trigger);
			}
		}
		// A split with nothing to mark it is still one to split by hand.
		return !into.name.empty();
	}

	bool ReadRoute(const Json& json, Route& into) {
		if (!json.is_object() || !json.contains("id") || !json["id"].is_string())
			return false;
		into = Route();
		into.id = json["id"].get<std::string>();
		into.name = json.contains("name") && json["name"].is_string() ?
			json["name"].get<std::string>() : into.id;

		Json::const_iterator splits = json.find("splits");
		if (splits != json.end() && splits->is_array()) {
			for (const Json& each : *splits) {
				Split split;
				if (ReadSplit(each, split))
					into.splits.push_back(split);
			}
		}
		// A route with no splits yet is one still being put together.
		return !into.id.empty();
	}

	// Where a byte of the text is, as an editor counts it.
	std::string Position(const std::string& text, size_t at) {
		unsigned int line = 1, column = 1;
		for (size_t i = 0; i < at && i < text.size(); i++) {
			if (text[i] == '\n') {
				line++;
				column = 1;
			} else {
				column++;
			}
		}
		return "line " + std::to_string(line) + ", column " + std::to_string(column);
	}
}

const std::vector<unsigned int>& QuestIds() {
	static const std::vector<unsigned int> ids = {
		1, 2, 3, 4, 5, 6,
		9, 10, 11, 12, 13, 14,
		17, 18, 19, 20, 21, 22,
		25, 26, 27,
		35, 36, 37, 38, 39, 40,
	};
	return ids;
}

std::vector<Route> Presets() {
	std::vector<Route> routes;
	routes.push_back(ActBossRoute("bosses-normal", "Act bosses (Normal)", 0));
	routes.push_back(ActBossRoute("bosses-hell", "Act bosses (through Hell)", 2));
	routes.push_back(Milestones());
	for (int act = 0; act < 5; act++) {
		const ActBoss& boss = kActBosses[act];
		routes.push_back({ "act" + std::to_string(act + 1),
			"Act " + std::to_string(act + 1) + " (" + boss.name + ")",
			{ BossSplit(boss, 0) } });
	}
	routes.push_back(Completionist());
	return routes;
}

std::string Write(const std::vector<Route>& routes) {
	Json listed = Json::array();
	for (const Route& route : routes) {
		Json splits = Json::array();
		for (const Split& split : route.splits) {
			Json one = Json::object();
			one["name"] = split.name;
			if (split.difficulty >= 0 && split.difficulty < 3)
				one["difficulty"] = kDifficulties[split.difficulty];
			if (split.all)
				one["all"] = true;
			Json when = Json::array();
			for (const Trigger& trigger : split.when)
				when.push_back(WriteTrigger(trigger));
			one["when"] = when;
			splits.push_back(one);
		}
		Json one = Json::object();
		one["id"] = route.id;
		one["name"] = route.name;
		one["splits"] = splits;
		listed.push_back(one);
	}

	Json file = Json::object();
	file["version"] = kFileVersion;
	file["routes"] = listed;
	return file.dump(2, ' ', false, Json::error_handler_t::replace) + "\n";
}

bool Read(const std::string& text, std::vector<Route>& routes, std::string* error) {
	Json file;
	try {
		file = Json::parse(text);
	} catch (const Json::parse_error& e) {
		if (error)
			*error = Position(text, e.byte > 0 ? e.byte - 1 : 0);
		return false;
	} catch (const Json::exception&) {
		if (error)
			*error = Position(text, 0);
		return false;
	}

	Json::const_iterator listed = file.is_object() ? file.find("routes") : file.end();
	if (!file.is_object() || listed == file.end() || !listed->is_array()) {
		if (error)
			*error = Position(text, 0);
		return false;
	}

	routes.clear();
	for (const Json& each : *listed) {
		Route route;
		if (ReadRoute(each, route))
			routes.push_back(route);
	}
	return true;
}

const Route* Find(const std::vector<Route>& routes, const std::string& asked) {
	const std::string wanted = Lower(asked);
	for (const Route& route : routes) {
		if (Lower(route.id) == wanted)
			return &route;
	}
	for (const Route& route : routes) {
		if (Lower(route.name) == wanted)
			return &route;
	}
	return NULL;
}

std::set<std::string> ItemCodes(const std::vector<Route>& routes) {
	std::set<std::string> codes;
	for (const Route& route : routes) {
		for (const Split& split : route.splits) {
			for (const Trigger& trigger : split.when) {
				if (trigger.kind == Trigger::Item)
					codes.insert(trigger.code);
			}
		}
	}
	return codes;
}

}
