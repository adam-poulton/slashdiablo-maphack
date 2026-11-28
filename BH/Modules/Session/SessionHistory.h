#pragma once
#include <set>
#include <string>
#include <vector>
#include "../Tracking/PlayEvents.h"

// The sessions that are over, as far as their logs say: what there is to list
// of each without reading its log again, and the folder the logs are kept in.
namespace SessionIndex {

// What a session came to. Worked out by reading its log once, and good for as
// long as the log is the size it was.
struct Entry {
	std::string file;				// the log's name, without its folder
	unsigned long long size;		// of the log, when this was worked out
	long long startedAt;			// seconds since the Unix epoch
	std::string character;
	unsigned int classId;
	bool hardcore;
	unsigned int level;				// as the session left it
	unsigned long long sessionMs;	// up to the log's last event
	unsigned long long playedMs;
	unsigned int games;
	long long experience;			// gained, less what deaths took
	unsigned int kills;
	unsigned int drops;
	unsigned int deaths;
};

Entry Summarise(const std::string& file, unsigned long long size,
	const std::vector<PlayEvents::Event>& events);

std::string Write(const std::vector<Entry>& entries);
bool Read(const std::string& text, std::vector<Entry>& entries);

// The logs to delete so that no more than `keep` are left: the oldest, which a
// log's name puts first. A log that is spared is never one of them, and counts
// towards what is kept. Nothing where `keep` is 0, which is to keep them all.
std::vector<std::string> Expired(std::vector<std::string> logs, unsigned int keep,
	const std::set<std::string>& spared);

}

// The folder of session logs and the index kept beside them. The index is only
// what reading the logs would say, kept so that they are read once: a log that
// has changed size since is read again, and one that has gone is forgotten.
class SessionHistory {
	public:
		explicit SessionHistory(const std::string& folder);

		// Reads the index as it was last written.
		void Load();

		// Brings the index a step nearer the folder: reads up to `budget` logs
		// that are new or have changed, and forgets those that have gone. A log
		// in `skip` is left out altogether: one still being written. Answers
		// how many logs are still to be read.
		unsigned int Reconcile(unsigned int budget, const std::set<std::string>& skip);

		// Newest first.
		const std::vector<SessionIndex::Entry>& Entries() const { return entries; }

		bool ReadLog(const std::string& file, std::vector<PlayEvents::Event>& events) const;

		// Deletes the oldest logs past `keep`, never one that is spared. Answers
		// how many went.
		unsigned int Prune(unsigned int keep, const std::set<std::string>& spared);

	private:
		std::string folder;			// ending in its separator
		std::vector<SessionIndex::Entry> entries;

		std::string IndexPath() const;
		void Save() const;
};
