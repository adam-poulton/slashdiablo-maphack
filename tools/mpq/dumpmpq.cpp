/*
 * Extracts one file out of a Diablo II MPQ archive, so the game's data tables
 * can be read while working on BH. See README.md for how to build and use it.
 *
 * StormLib.lib in ThirdParty is a 32 bit import library whose exports are
 * decorated __stdcall (_SFileOpenArchive@16), so these declarations must say
 * __stdcall, the tool must be built for x86, and StormLib.dll has to sit beside
 * the executable at run time. Declaring them __cdecl fails to link.
 */
#include <windows.h>
#include <stdio.h>

extern "C" {
	BOOL __stdcall SFileOpenArchive(const char* szMpqName, DWORD dwPriority, DWORD dwFlags, HANDLE* phMpq);
	BOOL __stdcall SFileCloseArchive(HANDLE hMpq);
	BOOL __stdcall SFileOpenFileEx(HANDLE hMpq, const char* szFileName, DWORD dwSearchScope, HANDLE* phFile);
	DWORD __stdcall SFileGetFileSize(HANDLE hFile, DWORD* pdwFileSizeHigh);
	BOOL __stdcall SFileReadFile(HANDLE hFile, void* lpBuffer, DWORD dwToRead, DWORD* pdwRead, LPOVERLAPPED lpOverlapped);
	BOOL __stdcall SFileCloseFile(HANDLE hFile);
}

int main(int argc, char** argv) {
	if (argc < 4) {
		printf("usage: dumpmpq <archive> <internal path> <out file>\n");
		printf("   eg: dumpmpq Patch_D2.mpq data\\global\\excel\\runes.txt runes.txt\n");
		return 1;
	}

	HANDLE hMpq = NULL;
	if (!SFileOpenArchive(argv[1], 0, 0x100, &hMpq)) {
		printf("open archive failed: %lu\n", GetLastError());
		return 2;
	}

	HANDLE hFile = NULL;
	if (!SFileOpenFileEx(hMpq, argv[2], 0, &hFile)) {
		// Error 2 is "not in this archive"; the file may live in another one.
		printf("open file failed: %lu\n", GetLastError());
		SFileCloseArchive(hMpq);
		return 3;
	}

	DWORD size = SFileGetFileSize(hFile, NULL);
	char* buffer = new char[size + 1];
	DWORD read = 0;
	SFileReadFile(hFile, buffer, size, &read, NULL);
	buffer[read] = 0;

	FILE* out = NULL;
	fopen_s(&out, argv[3], "wb");
	if (out) {
		fwrite(buffer, 1, read, out);
		fclose(out);
		printf("wrote %lu bytes\n", read);
	} else {
		printf("could not write %s\n", argv[3]);
	}

	delete[] buffer;
	SFileCloseFile(hFile);
	SFileCloseArchive(hMpq);
	return out ? 0 : 4;
}
