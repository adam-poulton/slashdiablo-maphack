#pragma once
#include <windows.h>
#include <string>

// A file that is only ever read and written whole, by every client of one
// install at once: each runs its own copy of BH, and any of them may save.
namespace WholeFile {
	// Answers false where there is no file, or it could not be opened.
	bool Read(const std::string& path, std::string& into);

	// Written beside the file and moved over it, so that a client dying
	// mid-write leaves the file it had rather than half of the new one.
	bool Write(const std::string& path, const std::string& text);

	// Whether the text is nothing but whitespace, which is what a file something
	// else truncated holds.
	bool IsBlank(const std::string& text);

	// Held from reading a file to writing it back, so that two clients saving at
	// once cannot each write over what the other just wrote. Every client of an
	// install takes the same lock for a file, whatever case its path is spelled
	// in; the tag keeps one kind of file's lock apart from another's.
	//
	// Bounded rather than waited on for ever, since a save runs on a client's
	// own thread: a wedged client holding the lock costs a save that says it
	// failed, not a client that stops drawing.
	class Lock {
		private:
			HANDLE held;
			bool taken;

			Lock(const Lock&) = delete;
			Lock& operator=(const Lock&) = delete;

		public:
			Lock(const std::string& tag, const std::string& path);
			~Lock();

			bool Taken() const { return taken; }
	};
}
