#pragma once
#include <Windows.h>
#include <algorithm>
#include <locale>
#include <cstdlib>
#include <fstream>
#include <map>
#include <unordered_map>
#include "Constants.h"
#include "Common.h"
#include "D2Structs.h"
#include "ItemTables.h"

/*
 * MPQInit handles the data we can initialize from MPQ files, provided we
 * are able to load StormLib. It also provides defaults in case we cannot
 * read the MPQ archive.
 */

extern unsigned int STAT_MAX;
extern unsigned int SKILL_MAX;

extern std::vector<StatProperties*> AllStatList;
extern std::unordered_map<std::string, StatProperties*> StatMap;
extern std::vector<CharStats*> CharList;
extern std::map<std::string, ItemAttributes*> ItemAttributeMap;


#define STAT_NUMBER(name) (StatMap[name]->ID)

bool IsInitialized();
void InitializeMPQData();
