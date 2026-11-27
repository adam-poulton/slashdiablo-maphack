#pragma once
#include <functional>
#include <string>
#include <vector>

// What the monsters a marker can be aimed at are called, read from the game's
// own tables.
//
// The same name is often several rows of monstats.txt: a Burning Soul is one
// monster in Act 3, another as a guest in Act 5 and another in the last waves of
// the Throne. Whoever picks a monster picks it by name, so the rows that share a
// name are one entry.
//
// The ubers are the exception. The game calls the Diablo of the Pandemonium
// event Diablo and its Duriel Duriel, though each is another monster than the
// act boss it is named after, so they are entries of their own, under the names
// players know them by.
namespace MonsterNames {
	// In the order the entries are listed in, but for the herd, which is listed
	// among the super uniques, where the Cow King is.
	enum Kind {
		Uber,			// Diablo Clone, and the bosses of the Pandemonium event
		Monster,
		SuperUnique,
		Herd,			// the Hell Bovines the Cow King leads, which no table names
		KindCount
	};

	// What the herd is called, tables or no tables.
	extern const char* const HerdName;

	struct Entry {
		std::string name;
		Kind kind;

		// What a search is matched against, in lower case: the name, and for an
		// uber the words it is also looked for by.
		std::string search;

		// monstats.txt rows, or for a super unique superuniques.txt rows; ascending.
		// None for the herd, which is told by what it carries and not by its row.
		std::vector<unsigned int> ids;

		Entry() : kind(Monster) {};
	};

	// The ubers, the monsters a player fights and the super uniques, each kind by
	// name. The tables are the text of monstats.txt and superuniques.txt, and name
	// gives the text a string key stands for, empty where it has none.
	std::vector<Entry> Build(const std::string& monStats, const std::string& superUniques,
		const std::function<std::string(const std::string&)>& name);

	// What the rows are called, each name once, in the order All() lists them,
	// and the herd with them where it is asked for. A row no entry holds is
	// called by its number.
	std::vector<std::string> NamesOf(const std::vector<Entry>& entries,
		const std::vector<unsigned int>& monsters,
		const std::vector<unsigned int>& superUniques, bool cowKingHerd = false);

	// Hands over the text of the two tables, once they have been read from the
	// game's archives.
	void SetTables(const std::string& monStats, const std::string& superUniques);

	// Every entry, built the first time it is asked for after the tables and the
	// game's strings are both in; empty until then.
	const std::vector<Entry>& All();
}
