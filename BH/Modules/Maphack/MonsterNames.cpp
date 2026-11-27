#include "MonsterNames.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <map>
#include "../../StatDescriptions.h"

namespace MonsterNames {
	const char* const HerdName = "Cow King's herd";

	namespace {
		std::string Lower(std::string text) {
			for (char& c : text)
				c = (char)std::tolower((unsigned char)c);
			return text;
		}

		std::vector<std::string> Split(const std::string& line) {
			std::vector<std::string> cells;
			size_t start = 0;
			while (true) {
				size_t tab = line.find('\t', start);
				cells.push_back(line.substr(start, tab == std::string::npos ? tab : tab - start));
				if (tab == std::string::npos)
					break;
				start = tab + 1;
			}
			return cells;
		}

		// A tab separated table, read by column name whatever its case.
		class Table {
			private:
				std::map<std::string, size_t> columns;
				std::vector<std::vector<std::string>> rows;
			public:
				explicit Table(const std::string& text) {
					size_t start = 0;
					bool header = true;
					while (start < text.size()) {
						size_t end = text.find('\n', start);
						if (end == std::string::npos)
							end = text.size();
						std::string line = text.substr(start, end - start);
						if (!line.empty() && line.back() == '\r')
							line.pop_back();
						start = end + 1;
						std::vector<std::string> cells = Split(line);
						if (header) {
							for (size_t i = 0; i < cells.size(); i++)
								columns.emplace(Lower(cells[i]), i);
							header = false;
						} else {
							rows.push_back(cells);
						}
					}
				}

				size_t Rows() const { return rows.size(); }

				std::string Cell(size_t row, const char* column) const {
					std::map<std::string, size_t>::const_iterator found = columns.find(column);
					if (found == columns.end() || found->second >= rows[row].size())
						return std::string();
					return rows[row][found->second];
				}

				bool Flag(size_t row, const char* column) const {
					return std::atoi(Cell(row, column).c_str()) != 0;
				}

				// Whether the cell holds a whole number, which a row that only
				// divides the table does not.
				bool Number(size_t row, const char* column, unsigned int& value) const {
					std::string cell = Cell(row, column);
					if (cell.empty() || !std::isdigit((unsigned char)cell[0]))
						return false;
					value = (unsigned int)std::strtoul(cell.c_str(), NULL, 10);
					return true;
				}
		};

		// The rows of monstats.txt that are an uber, by the Id the table keys them
		// with: what each is called, where the table's own name is another
		// monster's, and what else it is searched for by.
		struct UberName {
			const char* id;
			const char* name;		// NULL for the table's own
			const char* search;
		};

		const UberName Ubers[] = {
			{ "diabloclone", "Diablo Clone", "uber diablo" },
			{ "uberandariel", NULL, "uber andariel pandemonium" },
			{ "uberduriel", "Uber Duriel", "pandemonium" },
			{ "uberizual", "Uber Izual", "pandemonium" },
			{ "ubermephisto", "Uber Mephisto", "pandemonium" },
			{ "uberdiablo", "Pandemonium Diablo", "uber" },
			{ "uberbaal", "Uber Baal", "pandemonium" },
		};

		const UberName* FindUber(const std::string& id) {
			for (const UberName& uber : Ubers)
				if (id.compare(uber.id) == 0)
					return &uber;
			return NULL;
		}

		struct Named {
			std::string name;
			std::string search;		// beyond the name
			unsigned int id;
		};

		// By name, whatever its case, then as the tables list them.
		void Gather(std::vector<Entry>& entries, Kind kind, const std::vector<Named>& named) {
			std::map<std::string, Entry> byName;
			for (const Named& one : named) {
				Entry& entry = byName[Lower(one.name)];
				if (entry.ids.empty()) {
					entry.name = one.name;
					entry.kind = kind;
					entry.search = Lower(one.name);
					if (!one.search.empty())
						entry.search += " " + one.search;
				}
				entry.ids.push_back(one.id);
			}
			for (std::pair<const std::string, Entry>& held : byName) {
				std::sort(held.second.ids.begin(), held.second.ids.end());
				entries.push_back(held.second);
			}
		}

		std::string& MonStatsText() {
			static std::string text;
			return text;
		}

		std::string& SuperUniquesText() {
			static std::string text;
			return text;
		}
	}

	std::vector<Entry> Build(const std::string& monStats, const std::string& superUniques,
			const std::function<std::string(const std::string&)>& name) {
		std::vector<Entry> entries;

		// What a player fights: switched on, able to die, not a townsperson, and
		// not on the player's side, as a hireling or a summoned thing is.
		Table monsters(monStats);
		std::vector<Named> named, ubers;
		for (size_t row = 0; row < monsters.Rows(); row++) {
			unsigned int id = 0;
			if (!monsters.Number(row, "hcidx", id))
				continue;
			if (!monsters.Flag(row, "enabled") || !monsters.Flag(row, "killable") ||
					monsters.Flag(row, "npc") || monsters.Flag(row, "align"))
				continue;
			std::string key = monsters.Cell(row, "namestr");
			std::string shown = name(key);
			// A row with no name of its own is one of the game's stand-ins.
			if (key.empty() || shown.empty() || Lower(shown).compare("dummy") == 0)
				continue;
			const UberName* uber = FindUber(Lower(monsters.Cell(row, "id")));
			if (uber)
				ubers.push_back({ uber->name ? uber->name : shown, uber->search, id });
			else
				named.push_back({ shown, std::string(), id });
		}
		Gather(entries, Uber, ubers);
		Gather(entries, Monster, named);

		Table uniques(superUniques);
		named.clear();
		for (size_t row = 0; row < uniques.Rows(); row++) {
			unsigned int id = 0;
			if (!uniques.Number(row, "hcidx", id))
				continue;
			std::string key = uniques.Cell(row, "name");
			if (key.empty())
				continue;
			std::string shown = name(key);
			named.push_back({ shown.empty() ? key : shown, std::string(), id });
		}
		Gather(entries, SuperUnique, named);

		// The herd, by name among the super uniques it is listed with.
		Entry herd;
		herd.name = HerdName;
		herd.kind = Herd;
		herd.search = Lower(HerdName) + " the cow king minions pack";
		std::vector<Entry>::iterator at = entries.begin();
		while (at != entries.end() && (at->kind != SuperUnique || Lower(at->name) < Lower(herd.name)))
			++at;
		entries.insert(at, herd);
		return entries;
	}

	std::vector<std::string> NamesOf(const std::vector<Entry>& entries,
			const std::vector<unsigned int>& monsters,
			const std::vector<unsigned int>& superUniques, bool cowKingHerd) {
		std::vector<std::string> names;
		std::vector<unsigned int> leftMonsters = monsters, leftUniques = superUniques;
		for (const Entry& entry : entries) {
			if (entry.kind == Herd) {
				if (cowKingHerd)
					names.push_back(entry.name);
				cowKingHerd = false;
				continue;
			}
			std::vector<unsigned int>& left = entry.kind == SuperUnique ? leftUniques :
				leftMonsters;
			bool held = false;
			for (unsigned int id : entry.ids) {
				std::vector<unsigned int>::iterator found = std::find(left.begin(), left.end(), id);
				if (found == left.end())
					continue;
				left.erase(found);
				held = true;
			}
			if (held)
				names.push_back(entry.name);
		}
		for (unsigned int id : leftMonsters)
			names.push_back("Monster " + std::to_string(id));
		for (unsigned int id : leftUniques)
			names.push_back("Super unique " + std::to_string(id));
		if (cowKingHerd)
			names.push_back(HerdName);
		return names;
	}

	void SetTables(const std::string& monStats, const std::string& superUniques) {
		MonStatsText() = monStats;
		SuperUniquesText() = superUniques;
	}

	const std::vector<Entry>& All() {
		static std::vector<Entry> entries;
		static bool built = false;
		if (!built && StatDescriptions::IsInitialized() && !MonStatsText().empty()) {
			entries = Build(MonStatsText(), SuperUniquesText(), StatDescriptions::GetString);
			built = true;
			MonStatsText().clear();
			SuperUniquesText().clear();
		}
		return entries;
	}
}
