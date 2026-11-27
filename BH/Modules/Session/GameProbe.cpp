#include "GameProbe.h"
#include "../../BH.h"
#include "../../Constants.h"
#include "../../D2Helpers.h"
#include "../../D2Ptrs.h"
#include "KillKinds.h"

using namespace SessionEvents;

namespace {
	// What the game's alignment stat reads for a monster on the player's side:
	// a mercenary, a summons, a monster a Paladin converted.
	const DWORD kFriendly = 2;

	std::string Narrow(const wchar_t* text) {
		if (!text)
			return std::string();
		char* ansi = UnicodeToAnsi(text);
		std::string narrow(ansi);
		delete[] ansi;
		return narrow;
	}

	// D2Client's own level name function takes the level in esi. BH's stub for
	// it leaves the level there, where the compiler expects to find esi as it
	// left it, so this calls the function and puts esi back.
	__declspec(naked) wchar_t* __fastcall LevelName(DWORD level) {
		__asm {
			push esi
			mov esi, ecx
			call D2CLIENT_GetLevelName_I
			pop esi
			ret
		}
	}

	bool IsDead(UnitAny* monster) {
		return monster->dwMode == NPC_MODE_DEATH || monster->dwMode == NPC_MODE_DEAD;
	}

	bool IsFriendly(UnitAny* monster) {
		return D2COMMON_GetUnitStat(monster, STAT_ALIGNMENT, 0) == kFriendly;
	}

	// A name from a fixed-size field of the game's, which need not end in a zero.
	template <size_t N>
	std::string Name(const char (&field)[N]) {
		return std::string(field, strnlen(field, N));
	}

	unsigned int CountPlayers() {
		unsigned int count = 0;
		for (RosterUnit* roster = *p_D2CLIENT_PlayerUnitList; roster; roster = roster->pNext)
			count++;
		return count;
	}
}

GameProbe::GameProbe() {
	Reset();
}

void GameProbe::Reset() {
	present = false;
	joined = false;
	firstSeenAt = 0;
	identity = Identity();
	level = 0;
	experience = 0;
	toldExperience = 0;
	toldExperienceAt = 0;
	area = 0;
	players = 0;
	seenAlive = false;
	dead = false;
	monsters.clear();
	kills.clear();
	toldKillsAt = 0;
}

std::string GameProbe::AreaName(unsigned int area) {
	return Narrow(LevelName(area));
}

void GameProbe::TellExperience(unsigned long long tick, std::vector<Seen>& seen) {
	if (experience == toldExperience)
		return;
	seen.push_back({ tick, Experience{ experience } });
	toldExperience = experience;
	toldExperienceAt = tick;
}

void GameProbe::TellKills(unsigned long long tick, std::vector<Seen>& seen) {
	for (const auto& tally : kills) {
		Killed fact;
		fact.area = std::get<0>(tally.first);
		fact.classId = std::get<1>(tally.first);
		fact.tier = std::get<2>(tally.first);
		fact.name = tally.second.name;
		fact.count = tally.second.count;
		seen.push_back({ tick, fact });
	}
	kills.clear();
	toldKillsAt = tick;
}

// A monster seen alive and then dead has been killed, by whoever killed it: the
// client is not told who.
void GameProbe::WatchMonsters(UnitAny* player, unsigned long long tick, std::vector<Seen>& seen) {
	for (Room1* room = player->pAct->pRoom1; room; room = room->pRoomNext) {
		for (UnitAny* unit = room->pUnitFirst; unit; unit = unit->pListNext) {
			if (unit->dwType != UNIT_MONSTER)
				continue;
			const bool isDead = IsDead(unit);

			std::unordered_map<unsigned int, Watched>::iterator found = monsters.find(unit->dwUnitId);
			if (found == monsters.end()) {
				monsters[unit->dwUnitId] = { !isDead, !isDead && IsFriendly(unit), false };
				continue;
			}

			Watched& watched = found->second;
			if (!isDead) {
				watched.alive = true;
				continue;
			}
			if (!watched.alive || watched.counted)
				continue;
			watched.counted = true;
			// Asked again as it dies: a monster's stats are not always there the
			// first time it is seen.
			if (!watched.friendly && !IsFriendly(unit))
				CountKill(unit, tick, seen);
		}
	}
}

void GameProbe::CountKill(UnitAny* monster, unsigned long long tick, std::vector<Seen>& seen) {
	MonsterData* data = monster->pMonsterData;
	if (!data || !data->pMonStatsTxt)
		return;
	MonStatsTxt* txt = data->pMonStatsTxt;

	const KillKinds::Kind kind = { monster->dwTxtFileNo, txt->dwFlags.bKillable != 0,
		txt->dwFlags.bNPC != 0, txt->dwFlags.bNeverCount != 0, txt->bAlign };
	if (!KillKinds::Counts(kind))
		return;

	const Tier tier = KillKinds::TierOf({ txt->dwFlags.bPrimeEvil != 0, data->fSuperUniq != 0,
		data->fBoss != 0, data->fChamp != 0, data->fMinion != 0 });
	const std::string kindName = Narrow(D2LANG_GetLocaleText(txt->wNameStr));

	if (tier < TierUnique) {
		Tally& tally = kills[TallyKey(area, monster->dwTxtFileNo, tier)];
		tally.name = kindName;
		tally.count++;
		return;
	}

	Notable fact;
	fact.area = area;
	fact.classId = monster->dwTxtFileNo;
	fact.name = Narrow(D2CLIENT_GetUnitName(monster));
	fact.kind = kindName;
	fact.tier = tier;
	fact.superUnique = data->fSuperUniq ? (int)data->wUniqueNo : -1;
	seen.push_back({ tick, fact });
}

void GameProbe::Sample(unsigned long long tick, std::vector<Seen>& seen) {
	if (!IsGameReady())
		return;
	UnitAny* player = D2CLIENT_GetPlayerUnit();

	if (!present) {
		present = true;
		firstSeenAt = tick;
	}

	// The player's stats read as zero for the first frames of a game. A level
	// of zero is that, and nothing read beside it is to be believed.
	const unsigned int nowLevel = (unsigned int)D2COMMON_GetUnitStat(player, STAT_LEVEL, 0);
	if (nowLevel == 0)
		return;
	experience = D2COMMON_GetUnitStat(player, STAT_EXP, 0);

	if (!joined) {
		BnetData* bnet = *p_D2LAUNCH_BnData;
		identity.account = bnet ? Name(bnet->szAccountName) : std::string();
		identity.realm = bnet ? Name(bnet->szRealmName) : std::string();
		identity.character = player->pPlayerData ? Name(player->pPlayerData->szName) : std::string();
		identity.classId = player->dwTxtFileNo;
		identity.hardcore = bnet && (bnet->nCharFlags & PLAYER_TYPE_HARDCORE) != 0;
		identity.expansion = bnet && (bnet->nCharFlags & PLAYER_TYPE_EXPANSION) != 0;

		Joined fact;
		fact.game = bnet ? Name(bnet->szGameName) : std::string();
		fact.difficulty = D2CLIENT_GetDifficulty();
		fact.level = nowLevel;
		fact.experience = experience;
		// Timed from when the player first appeared, not from when its stats did.
		seen.push_back({ firstSeenAt, fact });

		joined = true;
		level = nowLevel;
		toldExperience = experience;
		toldExperienceAt = tick;
	}

	// Experience is told before anything that changes what it is credited to or
	// what it is measured against, and otherwise no more than once a second.
	const unsigned int nowArea = player->pPath->pRoom1->pRoom2->pLevel->dwLevelNo;
	if (nowArea != area) {
		TellExperience(tick, seen);
		TellKills(tick, seen);
		seen.push_back({ tick, AreaEntered{ nowArea, IsTown(nowArea), AreaName(nowArea) } });
		area = nowArea;
	}

	if (nowLevel > level) {
		TellExperience(tick, seen);
		seen.push_back({ tick, LevelReached{ nowLevel } });
		level = nowLevel;
	}

	// A death is the change from alive to dead, so a player that has not been
	// seen alive in this game has not died in it.
	const bool nowDead = player->dwMode == PLAYER_MODE_DEATH || player->dwMode == PLAYER_MODE_DEAD;
	if (!nowDead) {
		seenAlive = true;
		dead = false;
	} else if (seenAlive && !dead) {
		TellExperience(tick, seen);
		seen.push_back({ tick, Died() });
		dead = true;
	}

	const unsigned int nowPlayers = CountPlayers();
	if (nowPlayers != players) {
		seen.push_back({ tick, Players{ nowPlayers } });
		players = nowPlayers;
	}

	WatchMonsters(player, tick, seen);

	if (tick - toldExperienceAt >= kTellEveryMs)
		TellExperience(tick, seen);
	if (tick - toldKillsAt >= kTellEveryMs)
		TellKills(tick, seen);
}

void GameProbe::Leave(unsigned long long tick, std::vector<Seen>& seen) {
	if (joined) {
		TellExperience(tick, seen);
		TellKills(tick, seen);
		seen.push_back({ tick, Left() });
	}
	Reset();
}
