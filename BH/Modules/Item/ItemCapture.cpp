#include "ItemCapture.h"
#include <fstream>
#include <map>
#include <string>
#include "../../BH.h"
#include "../../Config.h"
#include "../../Constants.h"
#include "../../D2Helpers.h"
#include "../../D2Ptrs.h"
#include "../../D2Version.h"
#include "CaptureFormat.h"
#include "Item.h"
#include "ItemDisplay.h"
#include "ItemFactsLive.h"

using CaptureFormat::Record;

namespace {

const char* kCaptureFile = "item-captures.txt";

bool captureEnabled = false;

// Set whenever configuration is read, so the next recorded item is preceded by
// a fresh description of the rules it was judged against.
bool headerPending = true;

/*
 * The skill lists GOODSK and GOODTBSK are built from. Held here rather than in
 * a local because Config keeps the address it is given and writes through it
 * when settings are saved.
 */
std::map<std::string, std::string> capturedClassSkills;
std::map<std::string, std::string> capturedTabSkills;

/*
 * True when a rule reads something a capture cannot reproduce.
 *
 * Not everything a rule reads beyond the item is past recording. FILTLVL is a
 * setting and sits in the header; DIFF, CLASS and PLAYERTYPE, the character's
 * level that CLVL and CRAFTALVL read, and the area AREAID and AREALVL read are
 * all recorded against each item. What remains is CHARSTAT, which may ask for
 * any stat at all, and PRICE, which asks the game a question it will only
 * answer about an item that already exists as a unit.
 */
bool RulesReadLiveState() {
	for (auto it = rules.cbegin(); it != rules.cend(); ++it) {
		// Condition keys are matched case sensitively when parsed, so the rule
		// text is searched the same way.
		if (it->first.find("CHARSTAT") != std::string::npos ||
				it->first.find("PRICE") != std::string::npos)
			return true;
	}
	return false;
}

void Append(const std::string& line) {
	std::ofstream file(BH::path + kCaptureFile, std::ios::out | std::ios::app);
	if (!file.is_open())
		return;
	file << line << "\n";
}

void AppendAssoc(const char* type, const std::map<std::string, std::string>& entries) {
	for (auto it = entries.cbegin(); it != entries.cend(); ++it) {
		Record record(type);
		record.Add("key", it->first);
		record.Add("value", it->second);
		Append(record.Line());
	}
}

/*
 * The rows of the game's own tables that reading a packet depends on.
 *
 * A packet says as little as it can get away with, and how wide each part of it
 * is comes from these. Without them a recorded packet cannot be read back, so a
 * capture carries them rather than leaving a reader to find a copy of the game
 * to ask.
 *
 * All of them are written, not only the ones this session happened to need,
 * since which rows matter is not known until a packet is read against them.
 * Curation is where the ones nothing refers to are dropped.
 */
void AppendTables() {
	for (auto it = ItemAttributeMap.cbegin(); it != ItemAttributeMap.cend(); ++it) {
		const ItemAttributes* attrs = it->second;
		if (!attrs)
			continue;
		Record record("itemattrs");
		record.Add("code", it->first);
		record.Add("name", attrs->name);
		record.Add("category", attrs->category);
		record.Add("width", (long long)attrs->width);
		record.Add("height", (long long)attrs->height);
		record.Add("stackable", (long long)attrs->stackable);
		record.Add("useable", (long long)attrs->useable);
		record.Add("throwable", (long long)attrs->throwable);
		record.Add("itemLevel", (long long)attrs->itemLevel);
		record.Add("flags", (long long)attrs->flags);
		record.Add("flags2", (long long)attrs->flags2);
		record.Add("qualityLevel", (long long)attrs->qualityLevel);
		record.Add("magicLevel", (long long)attrs->magicLevel);
		Append(record.Line());
	}

	// Written by position rather than by the id they carry: the list has an
	// entry for every id up to the highest, gaps included, and a reader finds a
	// stat by counting along it.
	for (unsigned int i = 0; i < AllStatList.size(); i++) {
		const StatProperties* stat = AllStatList[i];
		if (!stat)
			continue;
		Record record("statwidths");
		record.Add("at", (long long)i);
		record.Add("name", stat->name);
		record.Add("saveBits", (long long)stat->saveBits);
		record.Add("saveParamBits", (long long)stat->saveParamBits);
		record.Add("saveAdd", (long long)stat->saveAdd);
		record.Add("op", (long long)stat->op);
		record.Add("sendParamBits", (long long)stat->sendParamBits);
		Append(record.Line());
	}
}

/*
 * What the filter's decision rested on that only a change of configuration can
 * alter. Anything that moves while playing is recorded against each item
 * instead.
 */
void AppendHeader() {
	BH::itemConfig->ReadAssoc("ClassSkillsList", capturedClassSkills);
	BH::itemConfig->ReadAssoc("TabSkillsList", capturedTabSkills);

	Record header("header");
	header.Add("bhVersion", std::string(BH_VERSION));
	// The game's own version, which is what the data tables a capture is
	// replayed against belong to.
	header.Add("d2Version", D2Version::GetGameVersionString());
	header.Add("contextSensitive", RulesReadLiveState());
	header.Add("filterLevel", (long long)Item::GetFilterLevel());
	header.Add("pingLevel", (long long)Item::GetPingLevel());
	header.Add("trackerPingLevel", (long long)Item::GetTrackerPingLevel());
	header.Add("orderedFiltering", OrderedFiltering);

	Append(header.Line());

	// Rules keep their file order, which is the order they are matched in.
	for (auto it = rules.cbegin(); it != rules.cend(); ++it) {
		Record rule("rule");
		rule.Add("condition", it->first);
		rule.Add("action", it->second);
		Append(rule.Line());
	}

	AppendAssoc("group", condition_group);
	AppendAssoc("classskill", capturedClassSkills);
	AppendAssoc("tabskill", capturedTabSkills);
	AppendTables();
}

}  // namespace

namespace ItemCapture {

void LoadConfig() {
	BH::config->ReadBoolean("Capture Item Drops", captureEnabled);
	headerPending = true;
}

void SettingsChanged() {
	headerPending = true;
}

bool IsEnabled() {
	return captureEnabled;
}

void RecordDrop(const unsigned char* packet, const ItemFacts& item,
		const Outcome& outcome) {
	if (!captureEnabled)
		return;

	if (headerPending) {
		AppendHeader();
		headerPending = false;
	}

	Record drop("drop");
	drop.Add("code", std::string(item.code, 3));
	drop.Add("name", item.name);
	drop.Add("action", (long long)item.action);

	/*
	 * What reading the packet made of it.
	 *
	 * The packet and these together are a worked example: given those bytes and
	 * the tables above, this is what the fields came out as. Reading a packet is
	 * a few hundred lines of counting bits, where a field of the wrong width
	 * puts every field after it somewhere else, and this is what would catch
	 * that.
	 *
	 * How many properties were read stands for the whole stat list, which is
	 * where the widths are read from the tables and where being one bit out
	 * shows up first.
	 */
	drop.Add("quality", (long long)item.quality);
	drop.Add("level", (long long)item.level);
	drop.Add("sockets", (long long)item.sockets);
	drop.Add("usedSockets", (long long)item.usedSockets);
	drop.Add("defense", (long long)item.defense);
	drop.Add("durability", (long long)item.durability);
	drop.Add("maxDurability", (long long)item.maxDurability);
	drop.Add("amount", (long long)item.amount);
	drop.Add("prefix", (long long)item.prefix);
	drop.Add("suffix", (long long)item.suffix);
	drop.Add("setCode", (long long)item.setCode);
	drop.Add("uniqueCode", (long long)item.uniqueCode);
	drop.Add("runewordId", (long long)item.runewordId);
	drop.Add("properties", (long long)item.properties.size());
	drop.Add("identified", item.identified);
	drop.Add("ethereal", item.ethereal);
	drop.Add("runeword", item.runeword);
	drop.Add("personalized", item.personalized);
	drop.Add("isGold", item.isGold);
	drop.Add("ear", item.ear);
	drop.Add("simpleItem", item.simpleItem);
	drop.Add("hasSockets", item.hasSockets);

	// Byte two of the packet is the message size the game itself declares, and
	// is what bounds the bytes worth keeping.
	unsigned int size = packet[2];
	drop.Add("packetSize", (long long)size);
	drop.Add("packet", std::string((const char*)packet, size));

	/*
	 * The world as it stood when the item landed. All of this moves while
	 * playing: the character walks between areas, gains levels, and a capture
	 * may run across more than one game, so it belongs to the item rather than
	 * to the header.
	 *
	 * The character's flags are recorded whole rather than unpacked, because
	 * that word is what PLAYERTYPE reads a bit out of and what decides whether
	 * an area's level is read from the expansion column.
	 */
	UnitAny* player = D2CLIENT_GetPlayerUnit();
	if (player) {
		drop.Add("charClass", (long long)player->dwTxtFileNo);
		drop.Add("charLevel",
			(long long)D2COMMON_GetUnitStat(player, STAT_LEVEL, 0));
	}
	drop.Add("difficulty", (long long)D2CLIENT_GetDifficulty());
	if (p_D2LAUNCH_BnData && *p_D2LAUNCH_BnData)
		drop.Add("charFlags", (long long)(*p_D2LAUNCH_BnData)->nCharFlags);
	drop.Add("areaId", (long long)GetPlayerArea());
	// Worked out from the area and the difficulty against the game's own level
	// table, and recorded rather than the table, since it is the whole of what
	// AREALVL asks for.
	drop.Add("areaLevel", (long long)GetCurrentAreaLevel());

	drop.Add("keepIndex", (long long)outcome.keepIndex);
	drop.Add("ignoreIndex", (long long)outcome.ignoreIndex);
	drop.Add("blocked", outcome.blocked);
	drop.Add("showOnMap", outcome.showOnMap);
	drop.Add("noTracking", outcome.noTracking);
	drop.Add("color", (long long)outcome.color);
	drop.Add("pingLevel", (long long)outcome.pingLevel);

	Append(drop.Line());
}

}  // namespace ItemCapture
