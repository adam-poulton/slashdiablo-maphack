#include <algorithm>
#include "TableReader.h"
#include "MPQReader.h"

/*
 * Filling the game's data tables from the MPQ archives.
 *
 * The archives are the game, so this is kept apart from TableReader.cpp: what
 * reads a table, searches it and hands out its rows can then be built and
 * tested without them, against tables stood up from files instead.
 */

bool TableReader::loadMPQData(std::string archiveName, Table &table)
{
	std::transform(archiveName.begin(), archiveName.end(), archiveName.begin(), ::tolower);
	MPQData* mpq = MpqDataMap[archiveName];
	if (!mpq) return false;
	for (auto iter = mpq->data.begin(); iter != mpq->data.end(); iter++){
		auto entry = *iter;
		JSONObject *obj = new JSONObject();
		for (auto header = mpq->fields.begin(); header != mpq->fields.end(); header++){
			std::string h = *header;
			if (entry[h].length() > 0){
				obj->set(h, entry[h]);
			}
		}
		table.addEntry(obj);
	}

	return true;
}

bool Tables::initTables(){
	bool success = true;
	if (!init){
		init = true;
		success &= TableReader::loadMPQData("itemstatcost", ItemStatCost);
		success &= TableReader::loadMPQData("ItemTypes", ItemTypes);
		success &= TableReader::loadMPQData("Properties", Properties);
		success &= TableReader::loadMPQData("runes", Runewords);
		success &= TableReader::loadMPQData("skills", Skills);
		success &= TableReader::loadMPQData("MagicPrefix", MagicPrefix);
		success &= TableReader::loadMPQData("MagicSuffix", MagicSuffix);
		success &= TableReader::loadMPQData("UniqueItems", UniqueItems);
		success &= TableReader::loadMPQData("SetItems", SetItems);
		success &= TableReader::loadMPQData("Sets", Sets);
		success &= TableReader::loadMPQData("RarePrefix", RarePrefix);
		success &= TableReader::loadMPQData("RareSuffix", RareSuffix);
		success &= TableReader::loadMPQData("CharStats", CharStats);
		success &= TableReader::loadMPQData("Gems", Gems);
		success &= TableReader::loadMPQData("SkillDesc", SkillDesc);
		success &= TableReader::loadMPQData("CubeMain", CubeMain);

		UniqueItems.removeWhere([](JSONElement* obj){
			return ((JSONObject*)obj)->getString("index").compare("Expansion") == 0;
		});
		SetItems.removeWhere([](JSONElement* obj){
			return ((JSONObject*)obj)->getString("item").length() == 0;
		});
		// Sets.txt ends on a blank row.
		Sets.removeWhere([](JSONElement* obj){
			return ((JSONObject*)obj)->getString("index").length() == 0;
		});

		buildLookups();
	}

	return success;
}
