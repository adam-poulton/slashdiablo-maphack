#pragma once
#include <atomic>
#include <set>
#include <tuple>
#include "../../D2Structs.h"
#include "../Module.h"
#include "../../Config.h"
#include "../../Drawing.h"
#include "../../Drawing/MapIcon/MapIconStyle.h"
#include "ChestKind.h"
#include "MonsterMarks.h"

enum MaphackReveal {
	MaphackRevealGame = 0,
	MaphackRevealAct,
	MaphackRevealLevel
};

struct LevelList {
	unsigned int levelId;
	unsigned int x, y, act;
};

struct BaseSkill {
	WORD Skill;
	BYTE Level;
};

// Where the level says a super chest stands, for marking it before the room it is
// in has been loaded. A live chest within reach of it is the chest itself, and
// is marked in its place.
struct SuperChestAnchor {
	ChestKind::Point position;
	bool matched;		// a closed chest within reach was seen this time round
	bool sawOpened;		// an opened one was
};

class Maphack : public Module {
	private:
		int monsterResistanceThreshold;
		int lkLinesColor;	// read only to start the super chest line from
		unsigned int experienceRangeColor;
		unsigned int roomFrontierColor;
		int automapOffsetX, automapOffsetY;
		bool automapOriginPatched;
		unsigned int revealType;
		bool revealedGame, revealedAct[6], revealedLevel[255];
		std::map<string, unsigned int> TextColorMap; 
		std::map<string, unsigned int> missileColors;
		std::list<LevelList*> automapLevels;

		// What BH_settings.cfg says of monsters, read only to start the Forge's
		// markers from: the colour of each rank, and the rest as the rules
		// they amount to.
		std::map<string, unsigned int> monsterColors;
		std::map<string, string> legacyMonsterColors;
		std::map<string, string> legacySuperUniqueColors;
		std::map<string, string> legacyMonsterLines;
		std::map<string, string> legacyMonsterHides;
		std::vector<MonsterMarks::Rule> legacyMonsterRules;

		// How monsters are marked: the marker each starts from, and the rules,
		// which the Forge's Monsters tab edits. The copy is the automap's own,
		// taken again when the rules move on.
		MapIconStyle monsterStyles[MonsterMarks::RankCount];
		MonsterMarks::RuleList monsterRules;
		std::vector<MonsterMarks::Rule> drawnMonsterRules;
		std::vector<unsigned int> drawnMonsterAuras;
		unsigned int drawnMonsterRevision;

		void ReadMonsterMarks();
		map<std::string, Toggle> Toggles;
		std::map<DWORD, std::vector<BaseSkill>> Skills;

		// How each kind of chest and container is marked, and where a sparkly
		// chest counts as a super chest.
		MapIconStyle chestStyles[ChestKind::KindCount];
		unsigned int superChestScope;

		// Anchors whose chest has been seen opened this game, by level and
		// position. A preset knows nothing of whether its chest was opened, so a
		// chest opened and walked away from would otherwise be marked again.
		std::set<std::tuple<DWORD, int, int>> openedAnchors;

		// Set on joining a game, which is not on the thread the automap is
		// worked out on; the automap forgets the anchors when it next sees it.
		std::atomic<bool> forgetOpenedAnchors;

		std::vector<SuperChestAnchor> FindSuperChestAnchors(Level* level);

	public:
	Maphack();

	void ReadConfig();
	void OnLoad();
	void OnUnload();

	void LoadConfig();
	void OnSettingsChanged(const vector<string>& keys);

	void OnLoop();
	void OnDraw();
	void OnAutomapDraw();
	void OnGameJoin();
	void OnGamePacketRecv(BYTE* packet, bool *block);

	void ResetRevealed();
	void ResetPatches();
	void ApplyAutomapOriginPatch();

	void OnKey(bool up, BYTE key, LPARAM lParam, bool* block);

	// The rules, for the window that edits them.
	MonsterMarks::RuleList& MonsterRules() { return monsterRules; }

	void RevealGame();
	void RevealAct(int act);
	void RevealLevel(Level* level);
	void RevealRoom(Room2* room);

	static Level* GetLevel(Act* pAct, int level);
	static AutomapLayer* InitLayer(int level);
};

void Weather_Interception();
void Lighting_Interception();
void Infravision_Interception();
void __stdcall Shake_Interception(LPDWORD lpX, LPDWORD lpY);
void HoverObject_Interception();
void DiabloMessage_Interception();

void  NPCMessageLoopPatch_ASM();
void  NPCQuestMessageStartPatch_ASM();
void  NPCQuestMessageEndPatch1_ASM();
void  NPCQuestMessageEndPatch2_ASM();

