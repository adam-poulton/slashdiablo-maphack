#pragma once
#include "JSONObject.h"
#include <functional>
#include <map>

class TableReader;
class Tables;

class Table {
	friend class TableReader;
	friend class Tables;
private:
	std::unique_ptr<JSONArray> data;
	std::vector<std::string> headers;
	// A field's rows keyed by the value each carries in it, so that findEntry
	// answers without walking the table. Emptied whenever the rows change,
	// since a lookup outliving them would answer for rows that are gone.
	std::map<std::string, std::map<std::string, JSONObject*>> lookups;
	void addEntry(JSONObject *entry);
	void removeWhere(std::function<bool(JSONElement*)> predicate);
public:
	Table() : data(new JSONArray()){}
	Table(std::string filePath);

	// Keys the rows by what they carry in a field. Where two rows carry the
	// same value the first keeps the key, that being the row a walk would have
	// stopped at and so the only one findEntry has ever returned.
	void lookupBy(std::string field);

	JSONObject* findEntry(std::function<bool(JSONObject*)> predicate);
	JSONObject* findEntry(std::string field, std::string value);
	JSONObject* binarySearch(std::string field, int value);
	JSONObject* entryAt(int index);
	int size();

	bool dump(std::string filePath);
};

class TableReader
{
private:
	static bool readTextTable(std::string filePath, Table &table);
	static bool readTbl(std::string filePath, Table &table);
public:
	static bool readTable(std::string filePath, Table &table);
	static bool loadMPQData(std::string archiveName, Table &table);
};

class Tables {
private:
	static bool init;

	Tables(){}
public:
	static bool initTables();

	// Keys the tables that are searched by a field often enough to be worth it.
	// Called once the rows are in, by whatever put them there: the archives in
	// a running game, fixture files in the tests.
	static void buildLookups();

	static Table ItemStatCost;
	static Table ItemTypes;
	static Table Properties;
	static Table Runewords;
	static Table UniqueItems;
	static Table SetItems;
	static Table Sets;
	static Table Skills;
	static Table MagicPrefix;
	static Table MagicSuffix;
	static Table RarePrefix;
	static Table RareSuffix;
	static Table CharStats;
	static Table Gems;
	static Table SkillDesc;
	static Table CubeMain;
	static Table Weapons;
	static Table Armor;
	static Table Misc;

	static std::string getString(int index);

	static bool isInitialized();
};

inline
void Table::addEntry(JSONObject *entry){
	lookups.clear();
	data->add(entry);
}

inline
JSONObject* Table::entryAt(int index){
	return data->getObject(index);
}
