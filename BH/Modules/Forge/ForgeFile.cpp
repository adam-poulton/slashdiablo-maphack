#include "ForgeFile.h"
#include "../../WholeFile.h"

static const char* const kLockTag = "forge";

ForgeFile::ForgeFile(const std::string& path) : path(path) {
	size_t slash = path.find_last_of("\\/");
	name = slash == std::string::npos ? path : path.substr(slash + 1);
}

bool ForgeFile::Load() {
	problem.clear();
	std::string text;
	if (!WholeFile::Read(path, text))
		text.clear();
	std::string where;
	if (store.Load(text, &where))
		return true;
	problem = "Error in " + name + " at " + where;
	return false;
}

bool ForgeFile::Save() {
	if (!store.HasChanges())
		return true;

	WholeFile::Lock lock(kLockTag, path);
	if (!lock.Taken()) {
		problem = "Could not save " + name;
		return false;
	}

	std::string onDisk, text, where;
	if (!WholeFile::Read(path, onDisk))
		onDisk.clear();
	if (!store.SaveOnto(onDisk, text, &where)) {
		problem = "Error in " + name + " at " + where;
		return false;
	}
	if (!WholeFile::Write(path, text)) {
		problem = "Could not write " + name;
		return false;
	}
	store.MarkSaved(text);
	problem.clear();
	return true;
}
