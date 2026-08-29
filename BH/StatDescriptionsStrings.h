#pragma once
#include <string>

/*
 * Between the stat description module and the loader that fills it out of the
 * MPQ archives, which are the only two that write string table text. A caller
 * without a game supplies it through StatDescriptions::LoadStrings instead.
 */
namespace StatDescriptions {
	// Records one string table key and the text it stands for. Later text
	// replaces earlier, which is the order the game applies its three tables in.
	void AddString(const std::string& key, const std::string& text);
}
