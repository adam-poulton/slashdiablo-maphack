#include "StatDescriptions.h"
#include "StatDescriptionsStrings.h"
#include <string>
#include <Windows.h>
#include "MPQReader.h"

/*
 * Loading the stat description strings out of the game's MPQ archives.
 *
 * This is the only part of the stat description module that knows the archives
 * exist, so anything that can supply the strings for itself - the tests, which
 * read them from a fixture through LoadStrings - can use the rest of the module
 * without the game.
 */

namespace {

// Offsets within a .tbl file, as described in TableReader.cpp.
enum TblOffsets {
	HeaderSize = 0x15,
	ElementSize = 0x02,
	NodeSize = 0x11,
	NumElementsOffset = 0x02,
	ActiveOffset = 0x00,
	KeyStringOffset = 0x07,
	ValueStringOffset = 0x0B
};

std::string ReadTblString(const char* buffer, size_t size, int offset) {
	if (offset < 0 || (size_t)offset >= size)
		return "";
	size_t end = offset;
	while (end < size && buffer[end] != 0)
		end++;
	return std::string(&buffer[offset], end - offset);
}

// The .tbl format is a hash table of key/value string pairs. We only need the
// key to value mapping, so the hashing side of it is ignored.
void ParseTbl(const char* buffer, size_t size) {
	if (size < HeaderSize)
		return;
	unsigned short count = *(unsigned short*)&buffer[NumElementsOffset];
	size_t firstNode = HeaderSize + (ElementSize * (size_t)count);
	for (unsigned short i = 0; i < count; i++) {
		size_t elementPos = HeaderSize + (ElementSize * (size_t)i);
		if (elementPos + ElementSize > size)
			break;
		unsigned short node = *(unsigned short*)&buffer[elementPos];
		size_t nodePos = firstNode + (NodeSize * (size_t)node);
		if (nodePos + NodeSize > size)
			continue;
		if (buffer[nodePos + ActiveOffset] == 0)
			continue;
		std::string key = ReadTblString(buffer, size, *(int*)&buffer[nodePos + KeyStringOffset]);
		std::string value = ReadTblString(buffer, size, *(int*)&buffer[nodePos + ValueStringOffset]);
		StatDescriptions::AddString(key, value);
	}
}

bool LoadTbl(const std::string& name) {
	// The tables live under the locale the client was installed with, and the
	// game's MPQ layer searches every loaded archive for us.
	const char* locales[] = { "eng", "esp", "deu", "fra", "ita", "por", "pol",
			"rus", "jpn", "kor", "chi", "sin", "tw" };
	for (int i = 0; i < (sizeof(locales) / sizeof(locales[0])); i++) {
		std::string path = std::string("data\\local\\lng\\") + locales[i] + "\\" + name + ".tbl";
		BufferData file = loadFile(path);
		if (!file.data)
			continue;
		ParseTbl((const char*)file.data, file.size);
		delete[] file.data;
		return true;
	}
	return false;
}

}	// namespace

namespace StatDescriptions {

bool Initialize() {
	if (IsInitialized())
		return true;
	LoadTbl("string");
	LoadTbl("expansionstring");
	LoadTbl("patchstring");
	return IsInitialized();
}

}	// namespace StatDescriptions
