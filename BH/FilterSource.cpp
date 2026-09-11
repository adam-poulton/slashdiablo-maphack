#include "FilterSource.h"
#include "BH.h"
#include "Common.h"
#include <algorithm>
#include <list>
#include <Windows.h>

namespace FilterSource {
	const char* const Default = "BH (default)";

	static const char* const Folder = "filters";
	static const char* const Extension = ".cfg";
	static const char* const DefaultFile = "BH.cfg";
	static const char* const Separator = "\\";

	static bool EndsWithExtension(const std::string& name) {
		size_t length = strlen(Extension);
		if (name.length() <= length)
			return false;
		return _stricmp(name.c_str() + name.length() - length, Extension) == 0;
	}

	static std::string FileFor(const std::string& selected) {
		return std::string(Folder) + Separator + selected + Extension;
	}

	// The wildcard is matched against short file names as well as long ones, so a
	// name is only a filter once it really does end in .cfg.
	static std::vector<std::string> InFolder() {
		std::vector<std::string> names;
		WIN32_FIND_DATA found;
		std::string pattern = BH::path + Folder + Separator + "*" + Extension;
		HANDLE search = FindFirstFile(pattern.c_str(), &found);
		if (search == INVALID_HANDLE_VALUE)
			return names;
		do {
			if (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				continue;
			std::string name(found.cFileName);
			if (!EndsWithExtension(name))
				continue;
			name.erase(name.length() - strlen(Extension));
			if (name.length() > 0)
				names.push_back(name);
		} while (FindNextFile(search, &found));
		FindClose(search);
		std::sort(names.begin(), names.end());
		return names;
	}

	std::vector<std::string> Options(const std::string& selected) {
		std::vector<std::string> options;
		options.push_back(Default);
		std::vector<std::string> found = InFolder();
		options.insert(options.end(), found.begin(), found.end());
		if (selected.length() > 0 &&
				std::find(options.begin(), options.end(), selected) == options.end())
			options.push_back(selected);
		return options;
	}

	// A file that parses but defines no rules is no more usable as a filter than
	// one that is not there: whatever is in it, it is not what was asked for.
	static bool Read(Config* config, const std::string& file) {
		config->SetConfigName(file);
		if (!config->Parse())
			return false;
		std::list<std::string> keys = config->GetDefinedKeys();
		return std::find(keys.begin(), keys.end(), "ItemDisplay") != keys.end();
	}

	bool Load(Config* config, const std::string& selected) {
		if (selected.length() > 0 && selected.compare(Default) != 0) {
			if (Read(config, FileFor(selected)))
				return true;
			// Only worth saying where it can be read. Out of a game the fallback is
			// silent, and the settings window shows which filter is selected.
			if (D2CLIENT_GetPlayerUnit()) {
				PrintText(1, "Could not read item filter %s, using %s",
					selected.c_str(), Default);
			}
		}
		Read(config, DefaultFile);
		return false;
	}
};
