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
