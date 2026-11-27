#pragma once
#include <ctime>
#include <fstream>
#include <string>
#include <vector>
#include "SessionEvents.h"

// A session's log on disk: a line per event, only ever added to.
//
// Each client writes a file of its own, so nothing is locked. Lines are handed
// to the system by Flush(), after which a client dying does not lose them, and a
// reader skips a line it cannot make sense of, as one cut short would be.
class SessionFile {
	private:
		std::string path;
		std::ofstream file;

	public:
		// Opens the log to add to, making its folder if it is not there. False
		// where it cannot be opened, after which Append() does nothing.
		bool Open(const std::string& path);

		bool IsOpen() const { return file.is_open(); }
		const std::string& GetPath() const { return path; }

		void Append(const SessionEvents::Event& event);
		void Flush();

		// Every event of a log, in order. False where there is no such file.
		static bool Read(const std::string& path,
			std::vector<SessionEvents::Event>& into);

		// What a session's log is called: when it began, whose it is, and the
		// client that wrote it, which is what keeps two clients starting in the
		// same second on the same character apart. A character's name is cut
		// down to what is safe in a file name.
		static std::string NameFor(const std::tm& began, const std::string& character,
			unsigned long process);
};
