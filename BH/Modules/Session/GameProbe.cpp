#include "GameProbe.h"
#include "../../BH.h"
#include "../../Constants.h"
#include "../../D2Helpers.h"
#include "../../D2Ptrs.h"

using namespace SessionEvents;

namespace {
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
}

void GameProbe::TellExperience(unsigned long long tick, std::vector<Seen>& seen) {
	if (experience == toldExperience)
		return;
	seen.push_back({ tick, Experience{ experience } });
	toldExperience = experience;
	toldExperienceAt = tick;
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
		seen.push_back({ tick, AreaEntered{ nowArea, IsTown(nowArea) } });
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

	if (tick - toldExperienceAt >= kExperienceEveryMs)
		TellExperience(tick, seen);
}

void GameProbe::Leave(unsigned long long tick, std::vector<Seen>& seen) {
	if (joined) {
		TellExperience(tick, seen);
		seen.push_back({ tick, Left() });
	}
	Reset();
}
