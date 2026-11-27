#include "SessionEvents.h"
#include "nlohmann/json.hpp"

// Ordered, so a line reads the same way every time: when, what, then the rest.
typedef nlohmann::ordered_json Json;

namespace SessionEvents {

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
