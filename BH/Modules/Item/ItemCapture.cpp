#include "ItemCapture.h"
#include <fstream>
#include <map>
#include <string>
#include "../../BH.h"
#include "../../Config.h"
#include "../../Constants.h"
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
 * Not everything a rule reads of the character is beyond recording. DIFF, CLASS,
 * PLAYERTYPE and FILTLVL are all in the header, and CLVL and CRAFTALVL read the
 * character's level, which is there too. What remains is CHARSTAT, which may ask
 * for any stat at all, and PRICE, which asks the game a question it will only
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
 * Everything other than the item that the filter's decision rested on. The
 * character's flags are recorded whole rather than unpacked, because that word
 * is what the filter's own PLAYERTYPE test reads bits out of.
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

	UnitAny* player = D2CLIENT_GetPlayerUnit();
	if (player) {
		header.Add("charClass", (long long)player->dwTxtFileNo);
		header.Add("difficulty", (long long)D2CLIENT_GetDifficulty());
		// CLVL compares against this, and CRAFTALVL works an affix level out
		// of it, so without it neither can be replayed.
		header.Add("charLevel",
			(long long)D2COMMON_GetUnitStat(player, STAT_LEVEL, 0));
	}
	if (p_D2LAUNCH_BnData && *p_D2LAUNCH_BnData)
		header.Add("charFlags", (long long)(*p_D2LAUNCH_BnData)->nCharFlags);

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

void RecordDrop(const unsigned char* packet, const ItemInfo& item,
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
