#include "RunStore.h"
#include "nlohmann/json.hpp"

typedef nlohmann::ordered_json Json;
typedef SessionModel::RunSplit RunSplit;

namespace {
	const unsigned int kFileVersion = 1;
	const char* const kClockNames[RunStore::Clocks] = { "real", "game" };

	long long TimeOf(const RunSplit& split, RunStore::Clock clock) {
		if (split.state != RunSplit::Reached)
			return -1;
		return (long long)(clock == RunStore::RealTime ? split.realMs : split.gameMs);
	}

	long long TotalOf(const SessionModel::Run& run, RunStore::Clock clock) {
		return (long long)(clock == RunStore::RealTime ? run.realMs : run.gameMs);
	}

	RunStore::Times NoTimes(size_t splits) {
		RunStore::Times times;
		times.splits.assign(splits, -1);
		times.total = -1;
		times.segments.assign(splits, -1);
		return times;
	}

	Json WriteTimes(const RunStore::Times& times) {
		Json json = Json::object();
		json["total"] = times.total;
		json["splits"] = times.splits;
		json["segments"] = times.segments;
		return json;
	}

	std::vector<long long> ReadList(const Json& json, const char* key) {
		std::vector<long long> list;
		Json::const_iterator found = json.find(key);
		if (found == json.end() || !found->is_array())
			return list;
		for (const Json& each : *found)
			list.push_back(each.is_number_integer() ? each.get<long long>() : -1);
		return list;
	}

	RunStore::Times ReadTimes(const Json& json) {
		RunStore::Times times = NoTimes(0);
		if (!json.is_object())
			return times;
		Json::const_iterator total = json.find("total");
		if (total != json.end() && total->is_number_integer())
			times.total = total->get<long long>();
		times.splits = ReadList(json, "splits");
		times.segments = ReadList(json, "segments");
		return times;
	}
}

std::string RunStore::KeyFor(const std::string& route, unsigned int classId, bool hardcore) {
	return route + "/" + std::to_string(classId) + (hardcore ? "/hc" : "/sc");
}

const RunStore::Best* RunStore::Find(const std::string& key) const {
	std::map<std::string, Best>::const_iterator found = bests.find(key);
	return found != bests.end() ? &found->second : NULL;
}

void RunStore::Record(const std::string& key, const SessionModel::Run& run) {
	const size_t splits = run.splits.size();
	std::map<std::string, Best>::iterator found = bests.find(key);
	if (found == bests.end()) {
		Best none = {};
		for (int clock = 0; clock < Clocks; clock++)
			none.by[clock] = NoTimes(splits);
		found = bests.insert({ key, none }).first;
	}
	Best& best = found->second;

	best.attempts++;
	if (run.finished)
		best.finished++;

	for (int each = 0; each < Clocks; each++) {
		const Clock clock = (Clock)each;
		Times& times = best.by[clock];
		// Times kept for a route that has since gained or lost a split are of
		// some other route, and are not times to beat.
		if (times.splits.size() != splits || times.segments.size() != splits)
			times = NoTimes(splits);

		for (unsigned int i = 0; i < splits; i++) {
			const long long segment = RunCompare::Segment(run, i, clock);
			if (segment >= 0 && (times.segments[i] < 0 || segment < times.segments[i]))
				times.segments[i] = segment;
		}

		const long long total = TotalOf(run, clock);
		if (run.finished && (times.total < 0 || total < times.total)) {
			times.total = total;
			for (unsigned int i = 0; i < splits; i++)
				times.splits[i] = TimeOf(run.splits[i], clock);
		}
	}
}

bool RunStore::Read(const std::string& text) {
	bests.clear();
	Json file = Json::parse(text, NULL, false);
	if (!file.is_object())
		return false;
	Json::const_iterator kept = file.find("bests");
	if (kept == file.end() || !kept->is_object())
		return false;

	for (Json::const_iterator each = kept->begin(); each != kept->end(); ++each) {
		const Json& json = each.value();
		if (!json.is_object())
			continue;
		Best best = {};
		best.attempts = json.value("attempts", 0u);
		best.finished = json.value("finished", 0u);
		for (int clock = 0; clock < Clocks; clock++)
			best.by[clock] = json.contains(kClockNames[clock]) ?
				ReadTimes(json[kClockNames[clock]]) : NoTimes(0);
		bests[each.key()] = best;
	}
	return true;
}

std::string RunStore::Write() const {
	Json kept = Json::object();
	for (const auto& each : bests) {
		Json json = Json::object();
		json["attempts"] = each.second.attempts;
		json["finished"] = each.second.finished;
		for (int clock = 0; clock < Clocks; clock++)
			json[kClockNames[clock]] = WriteTimes(each.second.by[clock]);
		kept[each.first] = json;
	}
	Json file = Json::object();
	file["version"] = kFileVersion;
	file["bests"] = kept;
	return file.dump(2, ' ', false, Json::error_handler_t::replace) + "\n";
}

namespace RunCompare {

long long Segment(const SessionModel::Run& run, unsigned int index, RunStore::Clock clock) {
	if (index >= run.splits.size())
		return -1;
	const long long end = TimeOf(run.splits[index], clock);
	if (end < 0)
		return -1;
	if (index == 0)
		return end;
	const long long begin = TimeOf(run.splits[index - 1], clock);
	return begin >= 0 && begin <= end ? end - begin : -1;
}

Lines Compare(const SessionModel::Run& run, const RunStore::Best* best, RunStore::Clock clock,
		unsigned long long now) {
	const size_t splits = run.splits.size();
	const RunStore::Times* times = best ? &best->by[clock] : NULL;
	if (times && (times->splits.size() != splits || times->segments.size() != splits))
		times = NULL;

	Lines lines;
	lines.current = -1;
	lines.clock = (long long)now;
	lines.bestTotal = times ? times->total : -1;
	lines.sumOfBest = times ? 0 : -1;
	lines.bestPossible = -1;

	for (unsigned int i = 0; i < splits; i++) {
		const RunSplit& split = run.splits[i];
		Line line;
		line.name = split.name;
		line.state = split.state;
		line.time = TimeOf(split, clock);
		line.best = times ? times->splits[i] : -1;
		line.compared = line.time >= 0 && line.best >= 0;
		line.delta = line.compared ? line.time - line.best : 0;
		line.level = split.level;

		const long long segment = Segment(run, i, clock);
		const long long gold = times ? times->segments[i] : -1;
		// A stretch equal to the best is the run that set it, which is how a
		// run reads once it has been recorded.
		line.gold = segment >= 0 && (gold < 0 || segment <= gold);

		if (lines.current < 0 && split.state == RunSplit::Open)
			lines.current = (int)i;
		if (lines.sumOfBest >= 0)
			lines.sumOfBest = gold >= 0 ? lines.sumOfBest + gold : -1;
		lines.splits.push_back(line);
	}

	// What is done, and from the first open split on every stretch at its
	// best. Not said where a later split was reached ahead of an open one:
	// there is then no telling which stretches are still to run.
	if (times && !run.ended) {
		const unsigned int open = lines.current < 0 ? (unsigned int)splits : (unsigned int)lines.current;
		long long possible = 0;
		for (unsigned int i = 0; i < open; i++) {
			if (lines.splits[i].time > possible)
				possible = lines.splits[i].time;
		}
		bool known = true;
		for (unsigned int i = open; i < splits && known; i++) {
			known = run.splits[i].state == RunSplit::Open && times->segments[i] >= 0;
			if (known)
				possible += times->segments[i];
		}
		if (known)
			lines.bestPossible = possible;
	}	return lines;
}

}
