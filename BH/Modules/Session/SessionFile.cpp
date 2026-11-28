#include "SessionFile.h"
#include <cctype>
#include <cstdio>
#include <filesystem>

bool SessionFile::Open(const std::string& to) {
	if (file.is_open())
		file.close();
	file.clear();
	path = to;

	std::error_code failed;
	const std::filesystem::path folder = std::filesystem::path(path).parent_path();
	if (!folder.empty())
		std::filesystem::create_directories(folder, failed);

	// Binary, so a line ends in the one byte it was given on every system.
	file.open(path.c_str(), std::ios::binary | std::ios::app);
	return file.is_open();
}

void SessionFile::Append(const PlayEvents::Event& event) {
	if (!file.is_open())
		return;
	file << PlayEvents::ToLine(event) << '\n';
}

void SessionFile::Flush() {
	if (file.is_open())
		file.flush();
}

bool SessionFile::Read(const std::string& path,
		std::vector<PlayEvents::Event>& into) {
	std::ifstream file(path.c_str(), std::ios::binary);
	if (!file.is_open())
		return false;

	std::string line;
	while (std::getline(file, line)) {
		PlayEvents::Event event;
		if (PlayEvents::FromLine(line, event))
			into.push_back(event);
	}
	return true;
}

std::string SessionFile::NameFor(const std::tm& began, const std::string& character,
		unsigned long process) {
	char stamp[32];
	sprintf_s(stamp, "%04d%02d%02d-%02d%02d%02d", began.tm_year + 1900,
		began.tm_mon + 1, began.tm_mday, began.tm_hour, began.tm_min, began.tm_sec);

	std::string safe;
	for (char c : character) {
		if (std::isalnum((unsigned char)c) || c == '-' || c == '_')
			safe += c;
	}
	if (safe.empty())
		safe = "character";

	return std::string(stamp) + "-" + safe + "-" + std::to_string(process) + ".jsonl";
}
