#pragma once
#include <functional>
#include <string>
#include "AccountStore.h"

// The file the accounts are kept in, and the only thing that writes it.
//
// Several clients run at once, each with its own copy of BH and its own store in
// memory, and the file is written whole. So nothing here holds a store between
// one change and the next. Every change is applied to what the file says at the
// moment of the change, under a lock every instance takes, and written back
// before the lock is let go. A store in memory is a copy to draw from and never
// the thing that is saved, which is what stops one client's save from quietly
// losing another's.
class AccountFile {
	private:
		std::string path;
		std::string lockName;

	public:
		explicit AccountFile(const std::string& path);

		const std::string& GetPath() const { return path; }

		// What the file says now, whatever the store said before. A file that is
		// not there is nothing kept rather than a failure: the first save is what
		// creates it. Answers false only where there is a file and it could not
		// be read, or where what it holds is not the file's shape.
		bool Read(AccountStore& into) const;

		// Applies one change to what the file says now and writes the result
		// back, with every other instance held off for the length of it. The
		// change answers whether it changed anything, and nothing is written
		// where it did not.
		//
		// The store handed to the change is what the file said a moment ago,
		// never what this client last drew, so a change made here cannot undo
		// one made in another client. Answers whether the file was written.
		bool Modify(const std::function<bool(AccountStore&)>& change);
};
