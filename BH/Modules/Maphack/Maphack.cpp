/* Maphack Module
 *
 */
#include "../../D2Ptrs.h"
#include "../../D2Helpers.h"
#include "../../D2Stubs.h"
#include "../../D2Intercepts.h"
#include "Maphack.h"
#include "../Settings/SettingsRegistry.h"
#include "../../BH.h"
#include "../../Drawing.h"
#include "../Item/ItemDisplay.h"
#include "../Item/ItemFactsLive.h"
#include "../Item/Item.h"
#include "../../AsyncDrawBuffer.h"
#include "../ScreenInfo/ScreenInfo.h"

#pragma optimize( "", off)

using namespace Drawing;
Patch* weatherPatch = new Patch(Jump, D2COMMON, { 0x6CC56, 0x30C36 }, (int)Weather_Interception, 5);
Patch* lightingPatch = new Patch(Call, D2CLIENT, { 0xA9A37, 0x233A7 }, (int)Lighting_Interception, 6);
Patch* infraPatch = new Patch(Call, D2CLIENT, { 0x66623, 0xB4A23 }, (int)Infravision_Interception, 7);
Patch* shakePatch = new Patch(Call, D2CLIENT, { 0x442A2, 0x452F2 }, (int)Shake_Interception, 5);
Patch* diabloDeadMessage = new Patch((PatchType)0x68, D2CLIENT, { 0x52E84, 0x693B4 }, (int)0x14, 5);

Patch* monsterNamePatch = new Patch(Call, D2WIN, { 0x13550, 0x140E0 }, (int)HoverObject_Interception, 5);
Patch* cpuPatch = new Patch(NOP, D2CLIENT, { 0x3CB7C, 0x2770C }, 0, 9);
Patch* fpsPatch = new Patch(NOP, D2CLIENT, { 0x44E51, 0x45EA1 }, 0, 8);

Patch* skipNpcMessages1 = new Patch(Call, D2CLIENT, { 0x4BB07, 0x7EB87 }, (int)NPCQuestMessageStartPatch_ASM, 6);
Patch* skipNpcMessages2 = new Patch(Call, D2CLIENT, { 0x48BD6, 0x7B4C6 }, (int)NPCQuestMessageEndPatch1_ASM, 8);
Patch* skipNpcMessages3 = new Patch(Call, D2CLIENT, { 0x4819F, 0x7A9CF }, (int)NPCQuestMessageEndPatch2_ASM, 5);
Patch* skipNpcMessages4 = new Patch(Call, D2CLIENT, { 0x7E9B7, 0x77737 }, (int)NPCMessageLoopPatch_ASM, 6);


// D2Client computes the automap origin at D2CLIENT+0x60C40, called from the automap
// draw at D2CLIENT+0x626A3 before any terrain is blitted:
//
//   lea ecx, [eax+ecx+0x28]   ; Offset.x = viewportX + playerX/divisor - width/2  + 0x28
//   lea eax, [edx+esi+0x0F]   ; Offset.y = viewportY + playerY/divisor - height/2 + 0x0F
//
// The width/height terms track the live resolution globals, but the +0x28/+0x0F centering constants are hardcoded.
// Patching the two disp8 bytes moves the origin, which shifts the native terrain and every icon
// drawn through Hook::ScreenToAutomap (which subtracts the same Offset) together.
static Offsets automapOriginXDisp = { 0x60D39, 0 };  // 1.13d offsets not mapped
static Offsets automapOriginYDisp = { 0x60D3D, 0 };
static const int automapOriginXBase = 0x28;
static const int automapOriginYBase = 0x0F;

static BOOL fSkipMessageReq = 0;
static DWORD mSkipMessageTimer = 0;
static DWORD mSkipQuestMessage = 1;

DrawDirective automapDraw(true, 5);

Maphack::Maphack() : Module("Maphack") {
	revealType = MaphackRevealAct;
	ResetRevealed();
	missileColors["Player"] = 0x97;
	missileColors["Neutral"] = 0x0A;
	missileColors["Party"] = 0x84;
	missileColors["Hostile"] = 0x5B;
	monsterColors["Normal"] = 0x5B;
	monsterColors["Minion"] = 0x60;
	monsterColors["Champion"] = 0x91;
	monsterColors["Boss"] = 0x84;

	monsterResistanceThreshold = 99;
	lkLinesColor = 105;

	automapOffsetX = 0;
	automapOffsetY = 0;
	automapOriginPatched = false;

	ReadConfig();
}

void Maphack::LoadConfig() {
	enhancementColors.clear();
	auraColors.clear();
	automapMonsterColors.clear();
	automapMonsterLines.clear();
	automapHiddenMonsters.clear();

	ReadConfig();
}

void Maphack::ReadConfig() {
	BH::config->ReadInt("Reveal Mode", revealType);
	BH::config->ReadInt("Show Monster Resistance", monsterResistanceThreshold);
	BH::config->ReadInt("LK Chest Lines", lkLinesColor);


	BH::config->ReadAssoc("Missile Color", missileColors);
	BH::config->ReadAssoc("Monster Color", monsterColors);

	TextColorMap["\377c0"] = 0x20;  // white
	TextColorMap["\377c1"] = 0x0A;  // red
	TextColorMap["\377c2"] = 0x84;  // green
	TextColorMap["\377c3"] = 0x97;  // blue
	TextColorMap["\377c4"] = 0x0D;  // gold
	TextColorMap["\377c5"] = 0xD0;  // gray
	TextColorMap["\377c6"] = 0x00;  // black
	TextColorMap["\377c7"] = 0x5A;  // tan
	TextColorMap["\377c8"] = 0x60;  // orange
	TextColorMap["\377c9"] = 0x0C;  // yellow
	TextColorMap["\377c;"] = 0x9B;  // purple
	TextColorMap["\377c:"] = 0x76;  // dark green
	TextColorMap["\377c\x06"] = 0x66; // coral
	TextColorMap["\377c\x07"] = 0x82; // sage
	TextColorMap["\377c\x09"] = 0xCB; // teal
	TextColorMap["\377c\x0C"] = 0xD6; // light gray

	std::vector<std::pair<std::string, std::string>> enhancementColorsString;
	BH::config->ReadMapList("Enhancement Color", enhancementColorsString);
	for (auto& entry : enhancementColorsString) {
		enhancementColors.push_back(std::pair(StringToNumber(entry.first), StringToNumber(entry.second)));
	}
	std::vector<std::pair<std::string, std::string>> auraColorsString;
	BH::config->ReadMapList("Aura Color", auraColorsString);
	for (auto& entry : auraColorsString) {
		auraColors.push_back(std::pair(StringToNumber(entry.first), StringToNumber(entry.second)));
	}


	BH::config->ReadAssoc("Monster Color", MonsterColors);
	for (auto it = MonsterColors.cbegin(); it != MonsterColors.cend(); it++) {
		// If the key is a number, it means a monster we've assigned a specific color
		int monsterId = -1;
		stringstream ss((*it).first);
		if ((ss >> monsterId).fail()) {
			continue;
		} else {
			int monsterColor = StringToNumber((*it).second);
			automapMonsterColors[monsterId] = monsterColor;
		}
	}

	BH::config->ReadAssoc("Super Unique Color", SuperUniqueColors);
	for (auto it = SuperUniqueColors.cbegin(); it != SuperUniqueColors.cend(); it++) {
		// If the key is a number, it means a monster we've assigned a specific color
		int monsterId = -1;
		stringstream ss((*it).first);
		if ((ss >> monsterId).fail()) {
			continue;
		}
		else {
			int monsterColor = StringToNumber((*it).second);
			automapSuperUniqueColors[monsterId] = monsterColor;
		}
	}

	
	BH::config->ReadAssoc("Monster Line", MonsterLines);
	for (auto it = MonsterLines.cbegin(); it != MonsterLines.cend(); it++) {
		// If the key is a number, it means a monster we've assigned a specific color
		int monsterId = -1;
		stringstream ss((*it).first);
		if ((ss >> monsterId).fail()) {
			continue;
		} else {
			int lineColor = StringToNumber((*it).second);
			automapMonsterLines[monsterId] = lineColor;
		}
	}

	BH::config->ReadAssoc("Monster Hide", MonsterHides);
	for (auto it = MonsterHides.cbegin(); it != MonsterHides.cend(); it++) {
		// If the key is a number, it means do not draw this monster on map
		int monsterId = -1;
		stringstream ss((*it).first);
		if ((ss >> monsterId).fail()) {
			continue;
		} else {
			automapHiddenMonsters.push_back(monsterId);
		}
	}

	BH::config->ReadToggle("Reveal Map", "None", true, Toggles["Auto Reveal"]);
	BH::config->ReadToggle("Show Monsters", "None", true, Toggles["Show Monsters"]);
	BH::config->ReadToggle("Show Missiles", "None", true, Toggles["Show Missiles"]);
	BH::config->ReadToggle("Show Chests", "None", true, Toggles["Show Chests"]);
	BH::config->ReadToggle("Force Light Radius", "None", true, Toggles["Force Light Radius"]);
	BH::config->ReadToggle("Remove Weather", "None", true, Toggles["Remove Weather"]);
	BH::config->ReadToggle("Infravision", "None", true, Toggles["Infravision"]);
	BH::config->ReadToggle("Remove Shake", "None", false, Toggles["Remove Shake"]);
	BH::config->ReadToggle("Display Level Names", "None", true, Toggles["Display Level Names"]);
	BH::config->ReadToggle("Monster Resistances", "None", true, Toggles["Monster Resistances"]);
	BH::config->ReadToggle("Monster Enchantments", "None", true, Toggles["Monster Enchantments"]);
	BH::config->ReadToggle("Monster Curses", "None", true, Toggles["Monster Curses"]);
	BH::config->ReadToggle("Apply CPU Patch", "None", true, Toggles["Apply CPU Patch"]);
	BH::config->ReadToggle("Apply FPS Patch", "None", true, Toggles["Apply FPS Patch"]);
	BH::config->ReadToggle("Show Automap On Join", "None", false, Toggles["Show Automap On Join"]);
	BH::config->ReadToggle("Skip NPC Quest Messages", "None", true, Toggles["Skip NPC Quest Messages"]);

	BH::config->ReadToggle("Show Normal Monsters", "None", true, Toggles["Show Normal Monsters"]);
	BH::config->ReadInt("Minimap Max Ghost", automapDraw.maxGhost);

	BH::config->ReadInt("Automap Offset X", automapOffsetX);
	BH::config->ReadInt("Automap Offset Y", automapOffsetY);
	ApplyAutomapOriginPatch();
}

void Maphack::ApplyAutomapOriginPatch() {
	// Unset offsets are the common case and leave the client untouched.
	// Only a previously applied offset still needs the vanilla constants written back.
	if (!automapOffsetX && !automapOffsetY && !automapOriginPatched)
		return;

	int xDisp = *(&automapOriginXDisp._113c + D2Version::GetGameVersionID());
	int yDisp = *(&automapOriginYDisp._113c + D2Version::GetGameVersionID());

	if (!xDisp || !yDisp)
		return;

	int xAddr = Patch::GetDllOffset(D2CLIENT, xDisp);
	int yAddr = Patch::GetDllOffset(D2CLIENT, yDisp);
	if (!xAddr || !yAddr)
		return;

	// Confirm both sites hold the vanilla displacements before writing.
	// Checked once: after the first patch the bytes no longer match the base.
	static bool checked = false, sitesMatch = false;
	if (!checked) {
		checked = true;
		sitesMatch = *(BYTE*)xAddr == (BYTE)automapOriginXBase &&
		             *(BYTE*)yAddr == (BYTE)automapOriginYBase;
	}
	if (!sitesMatch) {
		if (D2CLIENT_GetPlayerUnit())
			PrintText(Red, "Automap Offset disabled: unexpected D2Client code at the automap origin.");
		return;
	}

	// Offset is subtracted in Hook::ScreenToAutomap, so a larger constant pushes the overlay left/up.
	// Negated to keep the config in screen space: +X right, +Y down.
	// Offsets of 0 leave the vanilla constants in place.
	int x = automapOriginXBase - automapOffsetX;
	int y = automapOriginYBase - automapOffsetY;
	if (x < -128 || x > 127 || y < -128 || y > 127)
		return;

	// Written as base - delta
	BYTE bytes[1];
	bytes[0] = (BYTE)(signed char)x;
	Patch::WriteBytes(xAddr, 1, bytes);
	bytes[0] = (BYTE)(signed char)y;
	Patch::WriteBytes(yAddr, 1, bytes);
	automapOriginPatched = automapOffsetX || automapOffsetY;
}

void Maphack::ResetRevealed() {
	revealedGame = false;
	for (int act = 0; act < 6; act++)
		revealedAct[act] = false;
	for (int level = 0; level < 255; level++)
		revealedLevel[level] = false;
}

void Maphack::ResetPatches() {
	//Lighting Patch
	if (Toggles["Force Light Radius"].state)
		lightingPatch->Install();
	else
		lightingPatch->Remove();

	//Weather Patch
	if (Toggles["Remove Weather"].state)
		weatherPatch->Install();
	else
		weatherPatch->Remove();

	//Infravision Patch
	if (Toggles["Infravision"].state)
		infraPatch->Install();
	else
		infraPatch->Remove();
		//GameShake Patch
	if (Toggles["Remove Shake"].state)
		shakePatch->Install();
	else
		shakePatch->Remove();
	//Monster Health Bar Patch
	if (Toggles["Monster Resistances"].state)
		monsterNamePatch->Install();
	else
		monsterNamePatch->Remove();

	// patch for cpu-overutilization fix
	if (Toggles["Apply CPU Patch"].state)
		cpuPatch->Install();
	else
		cpuPatch->Remove();

	if (Toggles["Apply FPS Patch"].state)
		fpsPatch->Install();
	else
		fpsPatch->Remove();

	if (Toggles["Skip NPC Quest Messages"].state) {
		skipNpcMessages1->Install();
		skipNpcMessages2->Install();
		skipNpcMessages3->Install();
		skipNpcMessages4->Install();
	} else {
		skipNpcMessages1->Remove();
		skipNpcMessages2->Remove();
		skipNpcMessages3->Remove();
		skipNpcMessages4->Remove();
	}
}

void Maphack::OnLoad() {
	/*ResetRevealed();
	ReadConfig();
	ResetPatches();*/
	diabloDeadMessage->Install();

	// Said rather than drawn. Where these land, how wide they are and what happens
	// when the window is resized is the settings window's business; this only says
	// what the settings are and which of them depend on which.
	Settings::AddToggle(GetName(), Settings::Category::Map, "Reveal Map", "Auto reveal map",
		&Toggles["Auto Reveal"],
		"Reveals each area as you enter it, as far as the reveal mode allows.");
	Settings::AddEnum(GetName(), Settings::Category::Map, "Reveal Mode", "Auto reveal mode", &revealType,
		{ "Game", "Act", "Level" },
		"How much of the game is revealed at once.");

	Settings::AddToggle(GetName(), Settings::Category::Map, "Show Monsters", "Show monsters",
		&Toggles["Show Monsters"], "Marks monsters on the automap.");
	Settings::AddToggle(GetName(), Settings::Category::Map, "Monster Enchantments", "Enchantments",
		&Toggles["Monster Enchantments"],
		"Names the enchantments a champion or boss is carrying.", "Show Monsters");
	Settings::AddToggle(GetName(), Settings::Category::Map, "Monster Curses", "Cursed",
		&Toggles["Monster Curses"], "Shows monsters that are cursed, and may replace your shrine buff.",
		"Show Monsters");
	Settings::AddToggle(GetName(), Settings::Category::Map, "Monster Resistances", "Resistances",
		&Toggles["Monster Resistances"],
		"Shows what a monster resists or is immune to.", "Show Monsters");
	Settings::AddToggle(GetName(), Settings::Category::Map, "Show Normal Monsters", "Normal monsters",
		&Toggles["Show Normal Monsters"],
		"Marks ordinary monsters as well as the ones worth stopping for.",
		"Show Monsters");

	Settings::AddToggle(GetName(), Settings::Category::Map, "Show Missiles", "Show missiles",
		&Toggles["Show Missiles"], "Marks missiles in flight on the automap.");
	Settings::AddToggle(GetName(), Settings::Category::Map, "Show Chests", "Show chests",
		&Toggles["Show Chests"], "Marks chests and other containers.");
	Settings::AddToggle(GetName(), Settings::Category::Map, "Display Level Names", "Level names",
		&Toggles["Display Level Names"], "Names each level on the automap.");
	Settings::AddToggle(GetName(), Settings::Category::Map, "Show Automap On Join", "Show automap on join",
		&Toggles["Show Automap On Join"],
		"Opens the automap as you enter a game rather than waiting to be asked.");

	Settings::AddToggle(GetName(), Settings::Category::Map, "Force Light Radius", "Light radius",
		&Toggles["Force Light Radius"],
		"Lights the whole screen instead of only what your character can see.");
	Settings::AddToggle(GetName(), Settings::Category::Map, "Remove Weather", "Remove weather",
		&Toggles["Remove Weather"], "Stops rain and snow being drawn.");
	Settings::AddToggle(GetName(), Settings::Category::Map, "Infravision", "Infravision",
		&Toggles["Infravision"], "Lights monsters up as infravision does.");
	Settings::AddToggle(GetName(), Settings::Category::Map, "Remove Shake", "Remove shake",
		&Toggles["Remove Shake"], "Stops the screen shaking.");

	Settings::AddHeading(GetName(), Settings::Category::Map, "Monster colors");
	Settings::AddColor(GetName(), Settings::Category::Map, "Monster Color: Normal", "Normal",
		&monsterColors["Normal"], "", "Show Monsters");
	Settings::AddColor(GetName(), Settings::Category::Map, "Monster Color: Minion", "Minion",
		&monsterColors["Minion"], "", "Show Monsters");
	Settings::AddColor(GetName(), Settings::Category::Map, "Monster Color: Champion", "Champion",
		&monsterColors["Champion"], "", "Show Monsters");
	Settings::AddColor(GetName(), Settings::Category::Map, "Monster Color: Boss", "Boss",
		&monsterColors["Boss"], "", "Show Monsters");

	Settings::AddHeading(GetName(), Settings::Category::Map, "Missile colors");
	Settings::AddColor(GetName(), Settings::Category::Map, "Missile Color: Player", "Player",
		&missileColors["Player"], "", "Show Missiles");
	Settings::AddColor(GetName(), Settings::Category::Map, "Missile Color: Neutral", "Neutral",
		&missileColors["Neutral"], "", "Show Missiles");
	Settings::AddColor(GetName(), Settings::Category::Map, "Missile Color: Party", "Party",
		&missileColors["Party"], "", "Show Missiles");
	Settings::AddColor(GetName(), Settings::Category::Map, "Missile Color: Hostile", "Hostile",
		&missileColors["Hostile"], "", "Show Missiles");

	Settings::AddToggle(GetName(), Settings::Category::Input, "Skip NPC Quest Messages", "Skip NPC quest messages",
		&Toggles["Skip NPC Quest Messages"],
		"Skips the speeches an NPC gives when a quest turns over.");

}

void Maphack::OnKey(bool up, BYTE key, LPARAM lParam, bool* block) {
	for (map<string,Toggle>::iterator it = Toggles.begin(); it != Toggles.end(); it++) {
		if (key == (*it).second.toggle) {
			*block = true;
			// Only the flip. Reinstalling the patches is left to the settings
			// poll on the game loop, which notices this the same way it notices
			// the settings window: doing it here would install a patch from the
			// input thread.
			if (up)
				(*it).second.state = !(*it).second.state;
			return;
		}
	}
	return;
}

void Maphack::OnUnload() {
	lightingPatch->Remove();
	weatherPatch->Remove();
	infraPatch->Remove();
	shakePatch->Remove();
	skipNpcMessages1->Remove();
	skipNpcMessages2->Remove();
	skipNpcMessages3->Remove();
	skipNpcMessages4->Remove();
	diabloDeadMessage->Remove();
}

void Maphack::OnLoop() {
	// Get the player unit for area information.
	UnitAny* unit = D2CLIENT_GetPlayerUnit();
	if (!unit || !Toggles["Auto Reveal"].state)
		return;
	
	// Reveal the automap based on configuration.
	switch((MaphackReveal)revealType) {
		case MaphackRevealGame:
			RevealGame();
		break;
		case MaphackRevealAct:
			RevealAct(D2CLIENT_GetPlayerUnit()->pAct->dwAct + 1);
		break;
		case MaphackRevealLevel:
			RevealLevel(D2CLIENT_GetPlayerUnit()->pPath->pRoom1->pRoom2->pLevel);
		break;
	}
}

bool IsObjectChest(ObjectTxt *obj)
{
	//ObjectTxt *obj = D2COMMON_GetObjectTxt(objno);
	return (obj->nSelectable0 && (
		(obj->nOperateFn == 1) || //bed, undef grave, casket, sarc
		//(obj->nOperateFn == 3) || //basket, urn, rockpile, trapped soul
		(obj->nOperateFn == 4) || //chest, corpse, wooden chest, buriel chest, skull and rocks, dead barb
		(obj->nOperateFn == 5) || //barrel
		(obj->nOperateFn == 7) || //exploding barrel
		(obj->nOperateFn == 14) || //loose bolder etc....*
		(obj->nOperateFn == 19) || //armor stand
		(obj->nOperateFn == 20) || //weapon rack
		(obj->nOperateFn == 33) || //writ
		(obj->nOperateFn == 48) || //trapped soul
		(obj->nOperateFn == 51) || //stash
		(obj->nOperateFn == 68)    //evil urn
		));
}

Act* lastAct = NULL;

void Maphack::OnDraw() {
	UnitAny* player = D2CLIENT_GetPlayerUnit();

	if (!player || !player->pAct || player->pPath->pRoom1->pRoom2->pLevel->dwLevelNo == 0)
		return;
	// We're looping over all items and setting 2 flags:
	// UNITFLAG_NO_EXPERIENCE - Whether the item has been checked for a drop notification (to prevent checking it again)
	// UNITFLAG_REVEALED      - Whether the item should be notified and drawn on the automap
	// To my knowledge these flags arent typically used on items. So we can abuse them for our own use.
	for (Room1* room1 = player->pAct->pRoom1; room1; room1 = room1->pRoomNext) {
		for (UnitAny* unit = room1->pUnitFirst; unit; unit = unit->pListNext) {
			if (unit->dwType == UNIT_ITEM && (unit->dwFlags & UNITFLAG_NO_EXPERIENCE) == 0x0) {
				DWORD dwFlags = unit->pItemData->dwFlags;
				LiveItem live(unit);
				UnitItemInfo &uInfo = live.Unit();
				if (live.Known()) {
					// Every rule that has a say about the map has a colour to say
					// it with, so there is nothing here to tell apart, and the
					// list already ends at the rule that asked to stop.
					for (const Action *action : GetItemMapActions(&uInfo)) {
						// Skip notification if ping level requirement not met
						if (action->pingLevel > Item::GetPingLevel()) continue;
						unit->dwFlags |= UNITFLAG_REVEALED;
						if ((*BH::MiscToggles2)["Item Detailed Notifications"].state
						  && ((*BH::MiscToggles2)["Item Close Notifications"].state || (dwFlags & ITEMFLAG_NEW))
						  && action->notifyColor != DEAD_COLOR) {
							std::string itemName = GetItemName(unit);
							size_t start_pos = 0;
							while ((start_pos = itemName.find('\n', start_pos)) != std::string::npos) {
								itemName.replace(start_pos, 1, " - ");
								start_pos += 3;
							}
							PrintText(ItemColorFromQuality(unit->pItemData->dwQuality), "%s", itemName.c_str());
							if (!action->noTracking && !IsTown(GetPlayerArea()) && action->pingLevel <= Item::GetTrackerPingLevel()) {
								ScreenInfo::AddDrop(unit);
							}
							//PrintText(ItemColorFromQuality(unit->pItemData->dwQuality), "%s %x", itemName.c_str(), dwFlags);
							break;
						}
					}
				}
			}
			unit->dwFlags |= UNITFLAG_NO_EXPERIENCE;
		}
	}
}

void Maphack::OnAutomapDraw() {
	UnitAny* player = D2CLIENT_GetPlayerUnit();
	
	if (!player || !player->pAct || player->pPath->pRoom1->pRoom2->pLevel->dwLevelNo == 0)
		return;

	if (lastAct != player->pAct){
		lastAct = player->pAct;
		automapDraw.forceUpdate();
	}

	if (!IsInitialized()){
		Drawing::Texthook::Draw(10, 70, Drawing::None, 12, Gold, "Loading MPQ Data...");
	}
	
	automapDraw.draw([=](AsyncDrawBuffer &automapBuffer) -> void {
		POINT MyPos;
		Drawing::Hook::ScreenToAutomap(&MyPos,
			D2CLIENT_GetUnitX(D2CLIENT_GetPlayerUnit()),
			D2CLIENT_GetUnitY(D2CLIENT_GetPlayerUnit()));
		for (Room1* room1 = player->pAct->pRoom1; room1; room1 = room1->pRoomNext) {
			for (UnitAny* unit = room1->pUnitFirst; unit; unit = unit->pListNext) {
				//POINT automapLoc;
				DWORD xPos, yPos;

				// Draw monster on automap
				if (unit->dwType == UNIT_MONSTER && IsValidMonster(unit) && Toggles["Show Monsters"].state) {
					int lineColor = -1;
					int color = monsterColors["Normal"];
					if (unit->pMonsterData->fBoss)
						color = monsterColors["Boss"];
					if (unit->pMonsterData->fChamp)
						color = monsterColors["Champion"];
					if (unit->pMonsterData->fMinion)
						color = monsterColors["Minion"];
					//Cow king pack
					if (unit->dwTxtFileNo == 391 &&
							unit->pMonsterData->anEnchants[0] == ENCH_MAGIC_RESISTANT &&
							unit->pMonsterData->anEnchants[1] == ENCH_LIGHTNING_ENCHANTED &&
							unit->pMonsterData->anEnchants[3] != 0)
						color = 0xE1;

					// User can override colors of non-boss monsters
					if (automapMonsterColors.find(unit->dwTxtFileNo) != automapMonsterColors.end() && !unit->pMonsterData->fBoss) {
						color = automapMonsterColors[unit->dwTxtFileNo];
					}

					// User can hide monsters from map
					if (std::find(automapHiddenMonsters.begin(), automapHiddenMonsters.end(), unit->dwTxtFileNo) != automapHiddenMonsters.end() ) {
						continue;
					}

					if (!Toggles["Show Normal Monsters"].state && !unit->pMonsterData->fBoss && !unit->pMonsterData->fChamp && !unit->pMonsterData->fMinion) {
						continue;
					}

					// User can make it draw lines to monsters
					if (automapMonsterLines.find(unit->dwTxtFileNo) != automapMonsterLines.end() ) {
						lineColor = automapMonsterLines[unit->dwTxtFileNo];
					}

					//Determine immunities
					string szImmunities[] = { "\377c0p", "\377c8i", "\377c1i", "\377c9i", "\377c3i", "\377c2i" };
					string szResistances[] = { "\377c7r", "\377c8r", "\377c1r", "\377c9r", "\377c3r", "\377c2r" };
					DWORD dwImmunities[] = {
						STAT_DMGREDUCTIONPCT,
						STAT_MAGICDMGREDUCTIONPCT,
						STAT_FIRERESIST,
						STAT_LIGHTNINGRESIST,
						STAT_COLDRESIST,
						STAT_POISONRESIST
					};
					string immunityText;
					for (int n = 0; n < 6; n++) {
						int nImm = D2COMMON_GetUnitStat(unit, dwImmunities[n], 0);
						if (nImm >= 100) {
							immunityText += szImmunities[n];
						}
						else if (nImm >= monsterResistanceThreshold) {
							immunityText += szResistances[n];
						}
					}
					
					std::unordered_map<unsigned int, bool> enhancements;

					//Determine Enchantments
					string enchantText;
					string szEnchantments[] = {"\377c3m", "\377c1e", "\377c9e", "\377c3e"};
					string szCursed = "\377c;C";
						
					for (int n = 0; n < 9; n++) {
						if (Toggles["Monster Enchantments"].state && unit->pMonsterData->fBoss) {
							if (unit->pMonsterData->anEnchants[n] == ENCH_MANA_BURN)
								enchantText += szEnchantments[0];
							if (unit->pMonsterData->anEnchants[n] == ENCH_FIRE_ENCHANTED)
								enchantText += szEnchantments[1];
							if (unit->pMonsterData->anEnchants[n] == ENCH_LIGHTNING_ENCHANTED)
								enchantText += szEnchantments[2];
							if (unit->pMonsterData->anEnchants[n] == ENCH_COLD_ENCHANTED)
								enchantText += szEnchantments[3];
						}
						if (Toggles["Monster Curses"].state && unit->pMonsterData->fBoss) {
							if (unit->pMonsterData->anEnchants[n] == ENCH_CURSED)
								enchantText += szCursed;
						}
						enhancements[unit->pMonsterData->anEnchants[n]] = true;
					}

					for (auto& enhancement : enhancementColors) {
						if (enhancements.find(enhancement.first) != enhancements.end() && enhancement.second > 0 && !unit->pMonsterData->fBoss) {
							color = enhancement.second;
							break;
						}
					}

					// User can override colors of super unique monsters
					if (unit->pMonsterData->fSuperUniq &&
						automapSuperUniqueColors.find(unit->pMonsterData->wUniqueNo) != automapSuperUniqueColors.end()) {
						color = automapSuperUniqueColors[unit->pMonsterData->wUniqueNo];
						lineColor = automapSuperUniqueColors[unit->pMonsterData->wUniqueNo];
					}

					// auras has highest predence
					if (enhancements[ENCH_AURACHANT]) {
						for (auto& enhancement : auraColors) {
							if (D2COMMON_GetUnitState(unit, enhancement.first) != 0 && enhancement.second > 0) {
								color = enhancement.second;
								break;
							}
						}
					}

					xPos = unit->pPath->xPos;
					yPos = unit->pPath->yPos;
					automapBuffer.push([immunityText, enchantText, color, xPos, yPos, lineColor, MyPos]()->void{
						POINT automapLoc;
						Drawing::Hook::ScreenToAutomap(&automapLoc, xPos, yPos);
						if (immunityText.length() > 0)
							Drawing::Texthook::Draw(automapLoc.x, automapLoc.y - 8, Drawing::Center, 6, White, immunityText);
						if (enchantText.length() > 0)
							Drawing::Texthook::Draw(automapLoc.x, automapLoc.y - 14, Drawing::Center, 6, White, enchantText);
						Drawing::Crosshook::Draw(automapLoc.x, automapLoc.y, color);
						if (lineColor != -1) {
							Drawing::Linehook::Draw(MyPos.x, MyPos.y, automapLoc.x, automapLoc.y, lineColor);
						}
					});
				}
				else if (unit->dwType == UNIT_MISSILE && Toggles["Show Missiles"].state) {
					int color = 255;
					switch (GetRelation(unit)) {
					case 0:
						continue;
						break;
					case 1://Me
						color = missileColors["Player"];
						break;
					case 2://Neutral
						color = missileColors["Neutral"];
						break;
					case 3://Partied
						color = missileColors["Party"];
						break;
					case 4://Hostile
						color = missileColors["Hostile"];
						break;
					}

					xPos = unit->pPath->xPos;
					yPos = unit->pPath->yPos;					
					automapBuffer.push([color, unit, xPos, yPos]()->void{
						POINT automapLoc;
						Drawing::Hook::ScreenToAutomap(&automapLoc, xPos, yPos);
						Drawing::Boxhook::Draw(automapLoc.x - 1, automapLoc.y - 1, 2, 2, color, Drawing::BTHighlight);
					});
				}
				else if (unit->dwType == UNIT_ITEM && (unit->dwFlags & UNITFLAG_REVEALED) == UNITFLAG_REVEALED) {
					LiveItem live(unit);
					UnitItemInfo &uInfo = live.Unit();
					if (live.Known()) {
						// The list already ends at the rule that asked to stop.
						for (const Action *action : GetItemMapActions(&uInfo)) {
							// skip action if the ping level requirement isn't met
							if (action->pingLevel > Item::GetPingLevel()) continue;
							auto color = action->colorOnMap;
							auto borderColor = action->borderColor;
							auto dotColor = action->dotColor;
							auto pxColor = action->pxColor;
							auto lineColor = action->lineColor;
							xPos = unit->pItemPath->dwPosX;
							yPos = unit->pItemPath->dwPosY;
							automapBuffer.push_top_layer(
									[color, unit, xPos, yPos, MyPos, borderColor, dotColor, pxColor, lineColor]()->void{
								POINT automapLoc;
								Drawing::Hook::ScreenToAutomap(&automapLoc, xPos, yPos);
								if (borderColor != UNDEFINED_COLOR)
									Drawing::Boxhook::Draw(automapLoc.x - 4, automapLoc.y - 4, 8, 8, borderColor, Drawing::BTHighlight);
								if (color != UNDEFINED_COLOR)
									Drawing::Boxhook::Draw(automapLoc.x - 3, automapLoc.y - 3, 6, 6, color, Drawing::BTHighlight);
								if (dotColor != UNDEFINED_COLOR)
									Drawing::Boxhook::Draw(automapLoc.x - 2, automapLoc.y - 2, 4, 4, dotColor, Drawing::BTHighlight);
								if (pxColor != UNDEFINED_COLOR)
									Drawing::Boxhook::Draw(automapLoc.x - 1, automapLoc.y - 1, 2, 2, pxColor, Drawing::BTHighlight);
								if (lineColor != UNDEFINED_COLOR) {
									Drawing::Linehook::Draw(MyPos.x, MyPos.y, automapLoc.x, automapLoc.y, lineColor);
								}
							});
						}
					}
					else {
						HandleUnknownItemCode(uInfo.itemCode, "on map");
					}
				}
				else if (unit->dwType == UNIT_OBJECT && !unit->dwMode /* Not opened */ && Toggles["Show Chests"].state && IsObjectChest(unit->pObjectData->pTxt)) {
					xPos = unit->pObjectPath->dwPosX;
					yPos = unit->pObjectPath->dwPosY;
					automapBuffer.push([xPos, yPos]()->void{
						POINT automapLoc;
						Drawing::Hook::ScreenToAutomap(&automapLoc, xPos, yPos);
						Drawing::Boxhook::Draw(automapLoc.x - 1, automapLoc.y - 1, 2, 2, 255, Drawing::BTHighlight);
					});
				}				
			}
		}
		if (lkLinesColor > 0 && player->pPath->pRoom1->pRoom2->pLevel->dwLevelNo == MAP_A3_LOWER_KURAST) {
			for(Room2 *pRoom =  player->pPath->pRoom1->pRoom2->pLevel->pRoom2First; pRoom; pRoom = pRoom->pRoom2Next) {
				for (PresetUnit* preset = pRoom->pPreset; preset; preset = preset->pPresetNext) {
					DWORD xPos, yPos;
					int lkLineColor = lkLinesColor;
					if (preset->dwTxtFileNo == 160) {
						xPos = (preset->dwPosX) + (pRoom->dwPosX * 5);
						yPos = (preset->dwPosY) + (pRoom->dwPosY * 5);
						automapBuffer.push([xPos, yPos, MyPos, lkLineColor]()->void{
							POINT automapLoc;
							Drawing::Hook::ScreenToAutomap(&automapLoc, xPos, yPos);
							Drawing::Linehook::Draw(MyPos.x, MyPos.y, automapLoc.x, automapLoc.y, lkLineColor);
						});
					}
				}
			}
		}
		if (!Toggles["Display Level Names"].state)
			return;
		for (list<LevelList*>::iterator it = automapLevels.begin(); it != automapLevels.end(); it++) {
			if (player->pAct->dwAct == (*it)->act) {
				string tombStar = ((*it)->levelId == player->pAct->pMisc->dwStaffTombLevel) ? "\377c2*" : "\377c4";
				POINT unitLoc;
				Hook::ScreenToAutomap(&unitLoc, (*it)->x, (*it)->y);
				char* name = UnicodeToAnsi(D2CLIENT_GetLevelName((*it)->levelId));
				std::string nameStr = name;
				delete[] name;

				automapBuffer.push([nameStr, tombStar, unitLoc]()->void{
					Texthook::Draw(unitLoc.x, unitLoc.y - 15, Center, 6, Gold, "%s%s", nameStr.c_str(), tombStar.c_str());
				});
			}
		}
	});
}

// The patches follow the settings, so they are applied when a setting changes
// rather than being reapplied every frame in case one had.
void Maphack::OnSettingsChanged(const vector<string>& keys) {
	ResetPatches();
}

void Maphack::OnGameJoin() {
	ResetRevealed();
	automapLevels.clear();
	*p_D2CLIENT_AutomapOn = Toggles["Show Automap On Join"].state;
}

void Squelch(DWORD Id, BYTE button) {
	LPBYTE aPacket = new BYTE[7];	//create packet
	*(BYTE*)&aPacket[0] = 0x5d;	
	*(BYTE*)&aPacket[1] = button;	
	*(BYTE*)&aPacket[2] = 1;	
	*(DWORD*)&aPacket[3] = Id;
	D2NET_SendPacket(7, 0, aPacket);

	delete [] aPacket;	//clearing up data

	return;
}

void Maphack::OnGamePacketRecv(BYTE *packet, bool *block) {
	switch (packet[0]) {

	case 0x9c: {
		INT64 icode   = 0;
        char code[5]  = "";
        BYTE mode     = packet[1];
        DWORD gid     = *(DWORD*)&packet[4];
        BYTE dest     = ((packet[13] & 0x1C) >> 2);

        switch(dest)
        {
                case 0: 
                case 2:
                        icode = *(INT64 *)(packet+15)>>0x04;
                        break;
                case 3:
                case 4:
                case 6:
                        if(!((mode == 0 || mode == 2) && dest == 3))
                        {
                                if(mode != 0xF && mode != 1 && mode != 12)
                                        icode = *(INT64 *)(packet+17) >> 0x1C;
                                else
                                        icode = *(INT64 *)(packet+15) >> 0x04;
                        } 
                        else  
                                icode = *(INT64 *)(packet+17) >> 0x05;
                        break;
                default:
                        break;
        }

        memcpy(code, &icode, 4);
        if(code[3] == ' ') code[3] = '\0';

        //PrintText(1, "%s", code);

		//if(mode == 0x0 || mode == 0x2 || mode == 0x3) {
		//	BYTE ear = packet[10] & 0x01;
		//	if(ear) *block = true;
		//}
		break;
		}

	case 0xa8:
	case 0xa7: {
			//if(packet[1] == 0x0) {
			//	if(packet[6+(packet[0]-0xa7)] == 100) {
			//		UnitAny* pUnit = D2CLIENT_FindServerSideUnit(*(DWORD*)&packet[2], 0);
			//		if(pUnit)
			//			PrintText(1, "Alert: \377c4Player \377c2%s \377c4drank a \377c1Health \377c4potion!", pUnit->pPlayerData->szName);
			//	} else if (packet[6+(packet[0]-0xa7)] == 105) {
			//		UnitAny* pUnit = D2CLIENT_FindServerSideUnit(*(DWORD*)&packet[2], 0);
			//		if(pUnit)
			//			if(pUnit->dwTxtFileNo == 1)
			//				if(D2COMMON_GetUnitState(pUnit, 30))
			//					PrintText(1, "Alert: \377c4ES Sorc \377c2%s \377c4drank a \377c3Mana \377c4Potion!", pUnit->pPlayerData->szName);
			//	} else if (packet[6+(packet[0]-0xa7)] == 102) {//remove portal delay
			//		*block = true;
			//	}
			//}
			break;			   
		}
	case 0x94: {
			BYTE Count = packet[1];
			DWORD Id = *(DWORD*)&packet[2];
			for(DWORD i = 0;i < Count;i++) {
				BaseSkill S{};
				S.Skill = *(WORD*)&packet[6+(3*i)];
				S.Level = *(BYTE*)&packet[8+(3*i)];
				Skills[Id].push_back(S);
			}
			//for(vector<BaseSkill>::iterator it = Skills[Id].begin();  it != Skills[Id].end(); it++)
			//	PrintText(1, "Skill %d, Level %d", it->Skill, it->Level);
			break;
		}
	case 0x5b: {	//36   Player In Game      5b [WORD Packet Length] [DWORD Player Id] [BYTE Char Type] [NULLSTRING[16] Char Name] [WORD Char Lvl] [WORD Party Id] 00 00 00 00 00 00 00 00
			WORD lvl = *(WORD*)&packet[24];
			DWORD Id = *(DWORD*)&packet[3];
			char* name = (char*)&packet[8];
			UnitAny* Me = D2CLIENT_GetPlayerUnit();
			if(!Me)
				return;
			else if (!strcmp(name, Me->pPlayerData->szName))
				return;
			//if(lvl < 9)
			//	Squelch(Id, 3);
		}			//2 = mute, 3 = squelch, 4 = hostile
	}
}

void Maphack::RevealGame() {
	// Check if we have already revealed the game.
	if (revealedGame)
		return;

	// Iterate every act and reveal it.
	for (int act = 1; act <= ((*p_D2CLIENT_ExpCharFlag) ? 5 : 4); act++) {
		RevealAct(act);
	}

	revealedGame = true;
}

void Maphack::RevealAct(int act) {
	// Make sure we are given a valid act
	if (act < 1 || act > 5)
		return;

	// Check if the act is already revealed
	if (revealedAct[act])
		return;

	UnitAny* player = D2CLIENT_GetPlayerUnit();
	if (!player || !player->pAct)
		return;

	// Initalize the act incase it is isn't the act we are in.
	int actIds[6] = {1, 40, 75, 103, 109, 137};
	Act* pAct = D2COMMON_LoadAct(act - 1, player->pAct->dwMapSeed, *p_D2CLIENT_ExpCharFlag, 0, D2CLIENT_GetDifficulty(), NULL, actIds[act - 1], D2CLIENT_LoadAct_1, D2CLIENT_LoadAct_2);
	if (!pAct || !pAct->pMisc)
		return;

	// Iterate every level for the given act.
	for (int level = actIds[act - 1]; level < actIds[act]; level++) {
		Level* pLevel = GetLevel(pAct, level);
		if (!pLevel)
			continue;
		if (!pLevel->pRoom2First)
			D2COMMON_InitLevel(pLevel);
		RevealLevel(pLevel);
	}

	InitLayer(player->pPath->pRoom1->pRoom2->pLevel->dwLevelNo);
	D2COMMON_UnloadAct(pAct);
	revealedAct[act] = true;
}

void Maphack::RevealLevel(Level* level) {
	// Basic sanity checks to ensure valid level
	int LevelNo = level->dwLevelNo;
	if (!level || LevelNo < 0 || LevelNo > 255)
		return;

	// Check if the level has been previous revealed.
	if (revealedLevel[LevelNo])
		return;

	InitLayer(LevelNo);

	// Iterate every room in the level.
	for(Room2* room = level->pRoom2First; room; room = room->pRoom2Next) {
		bool roomData = false;

		//Add Room1 Data if it is not already there.
		if (!room->pRoom1) {
			D2COMMON_AddRoomData(level->pMisc->pAct, level->dwLevelNo, room->dwPosX, room->dwPosY, room->pRoom1);
			roomData = true;
		}

		//Make sure we have Room1
		if (!room->pRoom1)
			continue;

		//Reveal the room
		D2CLIENT_RevealAutomapRoom(room->pRoom1, TRUE, *p_D2CLIENT_AutomapLayer);

		//Reveal the presets
		RevealRoom(room);

		//Remove Data if Added
		if (roomData)
			D2COMMON_RemoveRoomData(level->pMisc->pAct, level->dwLevelNo, room->dwPosX, room->dwPosY, room->pRoom1);
	}

	revealedLevel[LevelNo] = true;
}

void Maphack::RevealRoom(Room2* room) {
	//Grabs all the preset units in room.
	for (PresetUnit* preset = room->pPreset; preset; preset = preset->pPresetNext)
	{
		int cellNo = -1;
		
		// Special NPC Check
		if (preset->dwType == UNIT_MONSTER)
		{
			// Izual Check
			if (preset->dwTxtFileNo == 256)
				cellNo = 300;
			// Hephasto Check
			if (preset->dwTxtFileNo == 745)
				cellNo = 745;
		// Special Object Check
		} else if (preset->dwType == UNIT_OBJECT) {
			// Uber Chest in Lower Kurast Check
			if (preset->dwTxtFileNo == 580 && room->pLevel->dwLevelNo == MAP_A3_LOWER_KURAST)
				cellNo = 318;

			// Countess Chest Check
			else if (preset->dwTxtFileNo == 371) 
				cellNo = 301;
			// Act 2 Orifice Check
			else if (preset->dwTxtFileNo == 152) 
				cellNo = 300;
			// Frozen Anya Check
			else if (preset->dwTxtFileNo == 460) 
				cellNo = 1468; 
			// Canyon / Arcane Waypoint Check
			else if ((preset->dwTxtFileNo == 402) && (room->pLevel->dwLevelNo == 46))
				cellNo = 0;
			// Hell Forge Check
			else if (preset->dwTxtFileNo == 376)
				cellNo = 376;

			// If it isn't special, check for a preset.
			if (cellNo == -1 && preset->dwTxtFileNo <= 572) {
				ObjectTxt *obj = D2COMMON_GetObjectTxt(preset->dwTxtFileNo);
				if (obj)
					cellNo = obj->nAutoMap;//Set the cell number then.
			}
		} else if (preset->dwType == UNIT_TILE) {
			LevelList* level = new LevelList;
			for (RoomTile* tile = room->pRoomTiles; tile; tile = tile->pNext) {
				if (*(tile->nNum) == preset->dwTxtFileNo) {
					level->levelId = tile->pRoom2->pLevel->dwLevelNo;
					break;
				}
			}
			level->x = (preset->dwPosX + (room->dwPosX * 5));
			level->y = (preset->dwPosY + (room->dwPosY * 5));
			level->act = room->pLevel->pMisc->pAct->dwAct;
			automapLevels.push_back(level);
		}

		//Draw the cell if wanted.
		if ((cellNo > 0) && (cellNo < 1258))
		{
			AutomapCell* cell = D2CLIENT_NewAutomapCell();

			cell->nCellNo = cellNo;
			int x = (preset->dwPosX + (room->dwPosX * 5));
			int y = (preset->dwPosY + (room->dwPosY * 5));
			cell->xPixel = (((x - y) * 16) / 10) + 1;
			cell->yPixel = (((y + x) * 8) / 10) - 3;

			D2CLIENT_AddAutomapCell(cell, &((*p_D2CLIENT_AutomapLayer)->pObjects));
		}

	}
	return;
}

AutomapLayer* Maphack::InitLayer(int level) {
	//Get the layer for the level.
	AutomapLayer2 *layer = D2COMMON_GetLayer(level);

	//Insure we have found the Layer.
	if (!layer)
		return false;

	//Initalize the layer!
	return (AutomapLayer*)D2CLIENT_InitAutomapLayer(layer->nLayerNo);
}

Level* Maphack::GetLevel(Act* pAct, int level)
{
	//Insure that the shit we are getting is good.
	if (level < 0 || !pAct)
		return NULL;

	//Loop all the levels in this act
	
	for(Level* pLevel = pAct->pMisc->pLevelFirst; pLevel; pLevel = pLevel->pNextLevel)
	{
		//Check if we have reached a bad level.
		if (!pLevel)
			break;

		//If we have found the level, return it!
		if (pLevel->dwLevelNo == level && pLevel->dwPosX > 0)
			return pLevel;
	}
	//Default old-way of finding level.
	return D2COMMON_GetLevel(pAct->pMisc, level);
}

int HoverMonsterColor(UnitAny *pUnit) {
	int color = White;
	if (pUnit->pMonsterData->fBoss)
		color = Gold;
	if (pUnit->pMonsterData->fChamp)
		color = Blue;
	return color;
}
int HoverObjectPatch(UnitAny* pUnit, DWORD tY, DWORD unk1, DWORD unk2, DWORD tX, wchar_t *wTxt)
{
	if (!pUnit || pUnit->dwType != UNIT_MONSTER || pUnit->pMonsterData->pMonStatsTxt->bAlign != MONSTAT_ALIGN_ENEMY)
		return 0;
	DWORD dwImmunities[] = {
		STAT_DMGREDUCTIONPCT,
		STAT_MAGICDMGREDUCTIONPCT,
		STAT_FIRERESIST,
		STAT_LIGHTNINGRESIST,
		STAT_COLDRESIST,
		STAT_POISONRESIST
	};
	int dwResistances[] = {
		0,0,0,0,0,0
	};
	for (int n = 0; n < 6; n++) {
		dwResistances[n] = D2COMMON_GetUnitStat(pUnit, dwImmunities[n], 0);
	}
	double maxhp = (double)(D2COMMON_GetUnitStat(pUnit, STAT_MAXHP, 0) >> 8);
	double hp = (double)(D2COMMON_GetUnitStat(pUnit, STAT_HP, 0) >> 8);
	POINT p = Texthook::GetTextSize(wTxt, 1);
	int center = tX + (p.x / 2);
	int y = tY - p.y;
	Texthook::Draw(center, y - 12, Center, 6, White, L"\377c7%d \377c8%d \377c1%d \377c9%d \377c3%d \377c2%d", dwResistances[0], dwResistances[1], dwResistances[2], dwResistances[3], dwResistances[4], dwResistances[5]);
	Texthook::Draw(center, y, Center, 6, White, L"\377c%d%s", HoverMonsterColor(pUnit), wTxt);
	Texthook::Draw(center, y + 8, Center, 6, White, L"%.0f%%", (hp / maxhp) * 100.0);
	return 1;
}


void __declspec(naked) Weather_Interception()
{
	__asm {
		je rainold
		xor al,al
rainold:
		ret 0x04
	}
}

BOOL __fastcall InfravisionPatch(UnitAny *unit)
{
	return false;
}

void __declspec(naked) Lighting_Interception()
{
	__asm {
		je lightold
		mov eax,0xff
		mov byte ptr [esp+4+0], al
		mov byte ptr [esp+4+1], al
		mov byte ptr [esp+4+2], al
		add dword ptr [esp], 0x72;
		ret
		lightold:
		push esi
		call D2COMMON_GetLevelIdFromRoom_I;
		ret
	}
}

void __declspec(naked) Infravision_Interception()
{
	__asm {
		mov ecx, esi
		call InfravisionPatch
		add dword ptr [esp], 0x72
		ret
	}
}

VOID __stdcall Shake_Interception(LPDWORD lpX, LPDWORD lpY)
{

	*p_D2CLIENT_xShake = 0;
	*p_D2CLIENT_yShake = 0;

}

//basically call HoverObjectPatch, if that function returns 0 execute
//the normal display code used basically for any hovered 
//object text (stash, merc, akara, etc...). if it returned 1
//that means we did our custom display text and shouldn't
//execute the draw method
void __declspec(naked) HoverObject_Interception()
{
	static DWORD rtn = 0;
	__asm {
		pop rtn
		push eax
		push ecx
		push edx
		call D2CLIENT_HoveredUnit_I
		push[esp + 0x10]
		push eax
		call HoverObjectPatch
		cmp eax, 0
		je origobjectname
		push rtn
		ret 0x28
		origobjectname:
		add esp, 0x8
		pop edx
		pop ecx
		pop eax
		call D2WIN_DrawTextBuffer
		push rtn
		ret
	}
}

//credits to https://github.com/jieaido/d2hackmap/blob/master/SkipNpcMessage.cpp
void  __declspec(naked) NPCMessageLoopPatch_ASM()
{
	__asm {
		test eax, eax
		jne noje
		mov eax, [mSkipQuestMessage]
		cmp eax, 0
		je oldje
		cmp[fSkipMessageReq], 0
		je oldje
		add[mSkipMessageTimer], 1
		cmp[mSkipMessageTimer], eax
		jle oldje
		mov[mSkipMessageTimer], 0
		mov eax, 1
		ret
	oldje :
		xor eax, eax
		add dword ptr[esp], 0xB9  // 0F84B8000000
	noje :
		ret
	}
}

void __declspec(naked) NPCQuestMessageStartPatch_ASM()
{
	__asm {
		mov[fSkipMessageReq], 1
		mov[mSkipMessageTimer], 0
		//oldcode:
		mov ecx, dword ptr[esi + 0x0C]
		movzx edx, di
		ret
	}
}

void __declspec(naked) NPCQuestMessageEndPatch1_ASM()
{
	__asm {
		mov[fSkipMessageReq], 0
		//oldcode:
		mov eax, dword ptr[esp + 0x24]
		mov ecx, dword ptr[esp + 0x20]
		ret
	}
}

void __declspec(naked) NPCQuestMessageEndPatch2_ASM()
{
	__asm {
		mov[fSkipMessageReq], 0
		//oldcode:
		mov edx, 1
		ret
	}
}

#pragma optimize( "", on)
