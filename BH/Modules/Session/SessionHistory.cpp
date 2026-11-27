#include "SessionHistory.h"
#include <algorithm>
#include <filesystem>
#include <map>
#include "nlohmann/json.hpp"
#include "../../WholeFile.h"
#include "SessionFile.h"
#include "SessionModel.h"

typedef nlohmann::ordered_json Json;
using SessionIndex::Entry;

namespace {
	const unsigned int kFileVersion = 1;
	const char* const kIndexName = "index.json";
	const char* const kLogExtension = ".jsonl";
	const char* const kLockTag = "sessions";

	// The logs in a folder, by name, with the size of each.
	std::map<std::string, unsigned long long> LogsIn(const std::string& folder) {
		std::map<std::string, unsigned long long> logs;
		std::error_code failed;
		for (std::filesystem::directory_iterator each(folder, failed), end; !failed && each != end;
				each.increment(failed)) {
			if (!each->is_regular_file(failed) || each->path().extension() != kLogExtension)
				continue;
			logs[each->path().filename().string()] = (unsigned long long)each->file_size(failed);
		}
		return logs;
	}

	// Newest first: a log is named for when its session began.
	void Sort(std::vector<Entry>& entries) {
		std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
			return a.file > b.file;
		});
	}
}

namespace SessionIndex {

Entry Summarise(const std::string& file, unsigned long long size,
		const std::vector<SessionEvents::Event>& events) {
	SessionModel model;
	for (const SessionEvents::Event& event : events)
		model.Apply(event);
	const unsigned long long end = events.empty() ? 0 : events.back().at;

	Entry entry;
	entry.file = file;
	entry.size = size;
	entry.startedAt = model.Header().startedAt;
	entry.character = model.Header().character;
	entry.classId = model.Header().classId;
	entry.hardcore = model.Header().hardcore;
	entry.level = model.Level();
	entry.sessionMs = end;
	entry.playedMs = model.PlayedMs(end);
	entry.games = (unsigned int)model.Games().size();
	entry.experience = (long long)model.Gained() - (long long)model.Lost();
	entry.kills = model.KillTotals().Total();
	entry.drops = (unsigned int)model.Drops().size();
	entry.deaths = model.Deaths();
	entry.runsFinished = 0;
	for (const SessionModel::Run& run : model.Runs()) {
		if (run.finished)
			entry.runsFinished++;
	}
	return entry;
}

std::string Write(const std::vector<Entry>& entries) {
	Json listed = Json::array();
	for (const Entry& entry : entries) {
		Json json = Json::object();
		json["file"] = entry.file;
		json["size"] = entry.size;
		json["start"] = entry.startedAt;
		json["char"] = entry.character;
		json["class"] = entry.classId;
		json["hardcore"] = entry.hardcore;
		json["level"] = entry.level;
		json["session"] = entry.sessionMs;
		json["played"] = entry.playedMs;
		json["games"] = entry.games;
		json["exp"] = entry.experience;
		json["kills"] = entry.kills;
		json["drops"] = entry.drops;
		json["deaths"] = entry.deaths;
		json["runs"] = entry.runsFinished;
		listed.push_back(json);
	}
	Json file = Json::object();
	file["version"] = kFileVersion;
	file["sessions"] = listed;
	return file.dump(1, ' ', false, Json::error_handler_t::replace) + "\n";
}

bool Read(const std::string& text, std::vector<Entry>& entries) {
	entries.clear();
	Json file = Json::parse(text, NULL, false);
	if (!file.is_object() || !file.contains("sessions") || !file["sessions"].is_array())
		return false;

	for (const Json& json : file["sessions"]) {
		if (!json.is_object() || !json.contains("file") || !json["file"].is_string())
			continue;
		Entry entry;
		entry.file = json["file"].get<std::string>();
		entry.size = json.value("size", 0ull);
		entry.startedAt = json.value("start", 0ll);
		entry.character = json.value("char", std::string());
		entry.classId = json.value("class", 0u);
		entry.hardcore = json.value("hardcore", false);
		entry.level = json.value("level", 0u);
		entry.sessionMs = json.value("session", 0ull);
		entry.playedMs = json.value("played", 0ull);
		entry.games = json.value("games", 0u);
		entry.experience = json.value("exp", 0ll);
		entry.kills = json.value("kills", 0u);
		entry.drops = json.value("drops", 0u);
		entry.deaths = json.value("deaths", 0u);
		entry.runsFinished = json.value("runs", 0u);
		entries.push_back(entry);
	}
	return true;
}

std::vector<std::string> Expired(std::vector<std::string> logs, unsigned int keep,
		const std::set<std::string>& spared) {
	std::vector<std::string> expired;
	if (keep == 0 || logs.size() <= keep)
		return expired;

	std::sort(logs.begin(), logs.end());
	size_t over = logs.size() - keep;
	for (size_t i = 0; i < logs.size() && over > 0; i++) {
		if (spared.count(logs[i]))
			continue;
		expired.push_back(logs[i]);
		over--;
	}
	return expired;
}

}

SessionHistory::SessionHistory(const std::string& folder) : folder(folder) {}

std::string SessionHistory::IndexPath() const {
	return folder + kIndexName;
}

void SessionHistory::Load() {
	std::string text;
	if (!WholeFile::Read(IndexPath(), text) || !SessionIndex::Read(text, entries))
		entries.clear();
	Sort(entries);
}

// Each client of an install keeps the index, so it is written under a lock and
// whole. What it holds is only ever what the logs say, so two clients writing
// it one after the other disagree about nothing that reading a log again will
// not settle.
void SessionHistory::Save() const {
	WholeFile::Lock held(kLockTag, IndexPath());
	if (held.Taken())
		WholeFile::Write(IndexPath(), SessionIndex::Write(entries));
}

unsigned int SessionHistory::Reconcile(unsigned int budget, const std::set<std::string>& skip) {
	std::map<std::string, unsigned long long> logs = LogsIn(folder);
	for (const std::string& each : skip)
		logs.erase(each);

	bool changed = false;
	std::vector<Entry> kept;
	for (const Entry& entry : entries) {
		std::map<std::string, unsigned long long>::iterator log = logs.find(entry.file);
		// Gone, or grown since it was read: either way what is known of it is
		// not to be kept.
		if (log == logs.end() || log->second != entry.size) {
			changed = true;
			continue;
		}
		kept.push_back(entry);
		logs.erase(log);
	}
	entries.swap(kept);

	// What is left is new, or has changed. The newest are read first, being
	// the ones a list of sessions opens on.
	unsigned int pending = (unsigned int)logs.size();
	for (std::map<std::string, unsigned long long>::reverse_iterator log = logs.rbegin();
			log != logs.rend() && budget > 0; ++log, budget--) {
		std::vector<SessionEvents::Event> events;
		if (SessionFile::Read(folder + log->first, events))
			entries.push_back(SessionIndex::Summarise(log->first, log->second, events));
		pending--;
		changed = true;
	}

	if (changed) {
		Sort(entries);
		Save();
	}
	return pending;
}

bool SessionHistory::ReadLog(const std::string& file,
		std::vector<SessionEvents::Event>& events) const {
	// A name and nothing more: the logs are in the one folder.
	if (file.find_first_of("\\/") != std::string::npos)
		return false;
	return SessionFile::Read(folder + file, events);
}

unsigned int SessionHistory::Prune(unsigned int keep, const std::set<std::string>& spared) {
	std::vector<std::string> names;
	for (const auto& log : LogsIn(folder))
		names.push_back(log.first);

	unsigned int gone = 0;
	for (const std::string& name : SessionIndex::Expired(names, keep, spared)) {
		std::error_code failed;
		if (std::filesystem::remove(folder + name, failed))
			gone++;
	}
	return gone;
}
