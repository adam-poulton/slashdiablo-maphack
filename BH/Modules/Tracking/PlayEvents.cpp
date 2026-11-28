#include "PlayEvents.h"
#include "nlohmann/json.hpp"

// Ordered, so a line reads the same way every time: when, what, then the rest.
typedef nlohmann::ordered_json Json;

namespace PlayEvents {

namespace {
	// What the log's lines say of themselves, on the line that starts a session.
	const unsigned int kLogVersion = 1;

	const char* const kAt = "t";
	const char* const kKind = "e";

	struct Writer {
		Json& line;

		void operator()(const Started& fact) const {
			line[kKind] = "session";
			line["v"] = kLogVersion;
			line["start"] = fact.startedAt;
			line["account"] = fact.account;
			line["realm"] = fact.realm;
			line["char"] = fact.character;
			line["class"] = fact.classId;
			line["hardcore"] = fact.hardcore;
			line["expansion"] = fact.expansion;
			line["bh"] = fact.version;
		}

		void operator()(const Joined& fact) const {
			line[kKind] = "join";
			line["game"] = fact.game;
			line["diff"] = fact.difficulty;
			line["level"] = fact.level;
			line["exp"] = fact.experience;
		}

		void operator()(const Left&) const {
			line[kKind] = "leave";
		}

		void operator()(const AreaEntered& fact) const {
			line[kKind] = "area";
			line["area"] = fact.area;
			line["town"] = fact.town;
			if (!fact.name.empty())
				line["name"] = fact.name;
		}

		void operator()(const Killed& fact) const {
			line[kKind] = "kills";
			line["area"] = fact.area;
			line["class"] = fact.classId;
			line["name"] = fact.name;
			line["tier"] = NameOf(fact.tier);
			line["n"] = fact.count;
		}

		void operator()(const Notable& fact) const {
			line[kKind] = "notable";
			line["area"] = fact.area;
			line["class"] = fact.classId;
			line["name"] = fact.name;
			line["kind"] = fact.kind;
			line["tier"] = NameOf(fact.tier);
			if (fact.superUnique >= 0)
				line["su"] = fact.superUnique;
		}

		void operator()(const Dropped& fact) const {
			line[kKind] = "drop";
			line["id"] = fact.id;
			line["name"] = fact.name;
			line["code"] = fact.code;
			line["quality"] = fact.quality;
			line["color"] = fact.color;
			line["ping"] = fact.ping;
			line["area"] = fact.area;
			if (!fact.areaName.empty())
				line["areaName"] = fact.areaName;
		}

		void operator()(const Quest& fact) const {
			line[kKind] = "quest";
			line["quest"] = fact.quest;
			line["bits"] = fact.bits;
		}

		void operator()(const ItemHeld& fact) const {
			line[kKind] = "item";
			line["code"] = fact.code;
		}

		void operator()(const RunStarted& fact) const {
			line[kKind] = "run";
			line["route"] = fact.route;
			line["name"] = fact.name;
			line["splits"] = fact.splits;
		}

		void operator()(const SplitReached& fact) const {
			line[kKind] = "split";
			line["index"] = fact.index;
			if (fact.manual)
				line["manual"] = true;
		}

		void operator()(const SplitUndone& fact) const {
			line[kKind] = "unsplit";
			line["index"] = fact.index;
		}

		void operator()(const SplitSkipped& fact) const {
			line[kKind] = "skip";
			line["index"] = fact.index;
		}

		void operator()(const RunEnded& fact) const {
			line[kKind] = "end";
			line["finished"] = fact.finished;
		}

		void operator()(const Experience& fact) const {
			line[kKind] = "exp";
			line["exp"] = fact.total;
		}

		void operator()(const LevelReached& fact) const {
			line[kKind] = "level";
			line["level"] = fact.level;
		}

		void operator()(const Died&) const {
			line[kKind] = "death";
		}

		void operator()(const Players& fact) const {
			line[kKind] = "players";
			line["count"] = fact.count;
		}
	};

	// A field of the kind asked for, or the value a missing or mistyped one
	// stands for.
	std::string Text(const Json& line, const char* key) {
		Json::const_iterator found = line.find(key);
		if (found == line.end() || !found->is_string())
			return std::string();
		return found->get<std::string>();
	}

	unsigned long long Number(const Json& line, const char* key) {
		Json::const_iterator found = line.find(key);
		if (found == line.end() || !found->is_number_unsigned())
			return 0;
		return found->get<unsigned long long>();
	}

	long long Signed(const Json& line, const char* key) {
		Json::const_iterator found = line.find(key);
		if (found == line.end() || !found->is_number_integer())
			return 0;
		return found->get<long long>();
	}

	bool Flag(const Json& line, const char* key) {
		Json::const_iterator found = line.find(key);
		return found != line.end() && found->is_boolean() && found->get<bool>();
	}

	// A tier by its name. One this build has no name for is the least of them:
	// the kill is still a kill.
	Tier TierNamed(const std::string& name) {
		for (int tier = 0; tier < TierCount; tier++) {
			if (name == NameOf((Tier)tier))
				return (Tier)tier;
		}
		return TierNormal;
	}

	bool ReadFact(const Json& line, Fact& into) {
		const std::string kind = Text(line, kKind);
		if (kind == "session") {
			Started fact;
			fact.startedAt = Signed(line, "start");
			fact.account = Text(line, "account");
			fact.realm = Text(line, "realm");
			fact.character = Text(line, "char");
			fact.classId = (unsigned int)Number(line, "class");
			fact.hardcore = Flag(line, "hardcore");
			fact.expansion = Flag(line, "expansion");
			fact.version = Text(line, "bh");
			into = fact;
		} else if (kind == "join") {
			Joined fact;
			fact.game = Text(line, "game");
			fact.difficulty = (unsigned int)Number(line, "diff");
			fact.level = (unsigned int)Number(line, "level");
			fact.experience = Number(line, "exp");
			into = fact;
		} else if (kind == "leave") {
			into = Left();
		} else if (kind == "area") {
			AreaEntered fact;
			fact.area = (unsigned int)Number(line, "area");
			fact.town = Flag(line, "town");
			fact.name = Text(line, "name");
			into = fact;
		} else if (kind == "kills") {
			Killed fact;
			fact.area = (unsigned int)Number(line, "area");
			fact.classId = (unsigned int)Number(line, "class");
			fact.name = Text(line, "name");
			fact.tier = TierNamed(Text(line, "tier"));
			fact.count = (unsigned int)Number(line, "n");
			into = fact;
		} else if (kind == "notable") {
			Notable fact;
			fact.area = (unsigned int)Number(line, "area");
			fact.classId = (unsigned int)Number(line, "class");
			fact.name = Text(line, "name");
			fact.kind = Text(line, "kind");
			fact.tier = TierNamed(Text(line, "tier"));
			fact.superUnique = line.contains("su") ? (int)Number(line, "su") : -1;
			into = fact;
		} else if (kind == "drop") {
			Dropped fact;
			fact.id = (unsigned int)Number(line, "id");
			fact.name = Text(line, "name");
			fact.code = Text(line, "code");
			fact.quality = (unsigned int)Number(line, "quality");
			fact.color = (unsigned int)Number(line, "color");
			fact.ping = (unsigned int)Number(line, "ping");
			fact.area = (unsigned int)Number(line, "area");
			fact.areaName = Text(line, "areaName");
			into = fact;
		} else if (kind == "quest") {
			Quest fact;
			fact.quest = (unsigned int)Number(line, "quest");
			fact.bits = (unsigned int)Number(line, "bits");
			into = fact;
		} else if (kind == "item") {
			ItemHeld fact;
			fact.code = Text(line, "code");
			into = fact;
		} else if (kind == "run") {
			RunStarted fact;
			fact.route = Text(line, "route");
			fact.name = Text(line, "name");
			Json::const_iterator splits = line.find("splits");
			if (splits != line.end() && splits->is_array()) {
				for (const Json& split : *splits)
					fact.splits.push_back(split.is_string() ? split.get<std::string>() : std::string());
			}
			into = fact;
		} else if (kind == "split") {
			SplitReached fact;
			fact.index = (unsigned int)Number(line, "index");
			fact.manual = Flag(line, "manual");
			into = fact;
		} else if (kind == "unsplit") {
			SplitUndone fact;
			fact.index = (unsigned int)Number(line, "index");
			into = fact;
		} else if (kind == "skip") {
			SplitSkipped fact;
			fact.index = (unsigned int)Number(line, "index");
			into = fact;
		} else if (kind == "end") {
			RunEnded fact;
			fact.finished = Flag(line, "finished");
			into = fact;
		} else if (kind == "exp") {
			Experience fact;
			fact.total = Number(line, "exp");
			into = fact;
		} else if (kind == "level") {
			LevelReached fact;
			fact.level = (unsigned int)Number(line, "level");
			into = fact;
		} else if (kind == "death") {
			into = Died();
		} else if (kind == "players") {
			Players fact;
			fact.count = (unsigned int)Number(line, "count");
			into = fact;
		} else {
			return false;
		}
		return true;
	}
}

const char* NameOf(Tier tier) {
	static const char* const names[TierCount] = { "normal", "minion", "champion",
		"unique", "superunique", "boss" };
	return tier < TierCount ? names[tier] : names[TierNormal];
}

std::string ToLine(const Event& event) {
	Json line = Json::object();
	line[kAt] = event.at;
	std::visit(Writer{ line }, event.fact);
	// Names come from the game in the system's code page, not in UTF-8. A byte
	// that is not UTF-8 is written as the replacement character rather than
	// refused, since a line that cannot be written is an event lost.
	return line.dump(-1, ' ', false, Json::error_handler_t::replace);
}

bool FromLine(const std::string& text, Event& into) {
	Json line = Json::parse(text, NULL, false);
	if (!line.is_object())
		return false;

	Json::const_iterator at = line.find(kAt);
	if (at == line.end() || !at->is_number_unsigned())
		return false;
	if (!ReadFact(line, into.fact))
		return false;
	into.at = at->get<unsigned long long>();
	return true;
}

}
