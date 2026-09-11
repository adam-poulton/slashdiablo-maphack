#pragma once
#include <string>
#include <Windows.h>
#include <thread>
#include <chrono>
#include "Modules/ModuleManager.h"
#include "Config.h"
#include "Drawing.h"
#include "Patch.h"

using namespace std;

//boosts  hash_combine
//https://stackoverflow.com/a/19195373/597419
template <class T>
inline void hash_combine(std::size_t& s, const T& v)
{
	std::hash<T> h;
	s ^= h(v) + 0x9e3779b9 + (s << 6) + (s >> 2);
}

struct cGuardModule
{	
	union {
		HMODULE hModule;
		DWORD dwBaseAddress;
	};
	DWORD _1;
	char szPath[MAX_PATH];
};

namespace BH {
	extern string path;
	extern HINSTANCE instance;
	extern ModuleManager* moduleManager;
	extern Config* config;
	extern Config* itemConfig;
	// The item filter the settings name, which is not always the one itemConfig
	// holds: a filter that cannot be read falls back to BH.cfg without the
	// selection being forgotten.
	extern string itemFilterSource;
	extern Drawing::StatsDisplay* statsDisplay;
	extern WNDPROC OldWNDPROC;
	extern map<string, Toggle>* MiscToggles;
	extern map<string, Toggle>* MiscToggles2;
	extern map<size_t, string> drops;
	extern bool cGuardLoaded;
	extern bool initialized;
	extern Patch* oogDraw;

	extern bool Startup(HINSTANCE instance, VOID* reserved);
	void Initialize();
	extern bool Shutdown();
	extern bool ReloadConfig();

	// Reads the item filter named in the settings into itemConfig.
	void ReadItemConfig();

	// Switches to a named filter, one of FilterSource::Options().
	void SelectItemFilter(const string& name);
};
