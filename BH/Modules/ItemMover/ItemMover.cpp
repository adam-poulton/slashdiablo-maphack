#include "ItemMover.h"
#include "../Settings/SettingsRegistry.h"
#include "../Item/Item.h"
#include "../Item/ItemCapture.h"
#include "../Item/ItemFactsLive.h"
#include "../Item/ItemFactsPacket.h"
#include "../../BH.h"
#include "../../D2Ptrs.h"
#include "../../D2Stubs.h"
#include "../../D2Helpers.h"
#include "../ScreenInfo/ScreenInfo.h"
#include "../Session/Session.h"
#include "ItemGrid.h"

// This module was inspired by the RedVex plugin "Item Mover", written by kaiks.
// Thanks to kaiks for sharing his code.

#define INVENTORY_WIDTH  inventoryLayout->SlotWidth
#define INVENTORY_HEIGHT inventoryLayout->SlotHeight

#define STASH_WIDTH  stashLayout->SlotWidth
#define STASH_HEIGHT stashLayout->SlotHeight

#define CUBE_WIDTH  cubeLayout->SlotWidth
#define CUBE_HEIGHT cubeLayout->SlotHeight

#define CELL_SIZE inventoryLayout->SlotPixelHeight

std::string POTIONS[] = { "hp", "mp", "rv" };

DWORD idBookId;
DWORD unidItemId;

// Items in the list can be half built while they move, so each step checks
// pItemData before following it.
static UnitAny* NextInvItem(UnitAny *item) {
	return item->pItemData ? item->pItemData->pNextInvItem : NULL;
}

// True when nothing in the inventory has room for the scroll: every matching tome
// is above the threshold, or no matching tome is carried at all.
static bool NoTomeWantsScroll(UnitAny *unit, const char *tomeCode, unsigned int threshold) {
	if (!unit || !unit->pInventory)
		return false;
	for (UnitAny *pItem = unit->pInventory->pFirstItem; pItem; pItem = NextInvItem(pItem)) {
		if (!pItem->pItemData || pItem->pItemData->ItemLocation != STORAGE_INVENTORY)
			continue;
		ItemText *text = D2COMMON_GetItemText(pItem->dwTxtFileNo);
		if (!text)
			continue;
		char* code = text->szCode;
		if (code[0] != tomeCode[0] || code[1] != tomeCode[1] || code[2] != tomeCode[2])
			continue;
		if ((unsigned int)D2COMMON_GetUnitStat(pItem, STAT_AMMOQUANTITY, 0) <= threshold)
			return false;
	}
	return true;
}

namespace {

// The tables a running client reads an item packet against: the game's own.
class GameItemTables : public ItemFactsPacket::Tables {
public:
	ItemAttributes* Attributes(const char* code) const override {
		std::map<std::string, ItemAttributes*>::const_iterator found =
			ItemAttributeMap.find(code);
		return (found == ItemAttributeMap.end()) ? NULL : found->second;
	}

	// The list is built with an entry for every stat id up to the highest the
	// tables mention, gaps included, so a stat beyond its end is one the tables
	// never described.
	StatProperties* Stat(unsigned int stat) const override {
		return (stat < AllStatList.size()) ? AllStatList[stat] : NULL;
	}
};

// What a packet that did not read cleanly is worth saying in game.
class GameDiagnostics : public ItemFactsPacket::Diagnostics {
public:
	void UnknownItemCode(const char* code) override {
		HandleUnknownItemCode(const_cast<char*>(code), "from packet");
	}

	void UnreadableStat(unsigned int stat, const char* code) override {
		if ((*BH::MiscToggles2)["Suppress Invalid Stats"].state)
			return;
		PrintText(1, "Invalid stat: %d, %c%c%c", stat, code[0], code[1], code[2]);
	}

	void Failed(const char* code, const std::string& reason) override {
		PrintText(1, "Exception parsing item: %c%c%c, %s",
			code[0], code[1], code[2], reason.c_str());
	}
};

bool ReadItemPacket(const BYTE* packet, ItemFacts* item) {
	GameItemTables tables;
	GameDiagnostics diagnostics;
	// "Suppress Invalid Stats" asks for a stat of unknown width to be kept and
	// the rest of the packet read on, rather than the item being abandoned.
	bool stopOnUnreadableStat =
		!(*BH::MiscToggles2)["Suppress Invalid Stats"].state;
	ItemFactsPacket::Reader reader(tables, diagnostics, stopOnUnreadableStat);
	return reader.Read(packet, item);
}

}  // namespace

// "Hide Redundant Scrolls": true for a town portal/identify scroll that lands while
// no carried tome has room for it.
static bool IsRedundantScroll(BYTE *packet) {
	ItemFacts item = {};
	ItemFactsPacket::PacketStats stats(item);
	item.stats = &stats;
	bool success = ReadItemPacket(packet, &item);
	if (!success || (item.action != ITEM_ACTION_NEW_GROUND && item.action != ITEM_ACTION_OLD_GROUND))
		return false;

	const char* tomeCode = NULL;
	if (strcmp(item.code, "tsc") == 0) {
		tomeCode = "tbk";
	} else if (strcmp(item.code, "isc") == 0) {
		tomeCode = "ibk";
	} else {
		return false;
	}
	return NoTomeWantsScroll(D2CLIENT_GetPlayerUnit(), tomeCode, Item::GetScrollVisibilityThreshold());
}

// The game caps carried gold at this much per character level.
static const unsigned int GOLD_CARRIED_PER_LEVEL = 10000;

// "Hide Redundant Gold Piles": true for gold that lands while the character already
// carries every coin their level allows. Gold in the stash is a separate pool and
// does not count towards the limit. Never true for gold lying in town, where the
// stash is at hand to make room.
static bool IsRedundantGold(BYTE *packet) {
	ItemFacts item = {};
	ItemFactsPacket::PacketStats stats(item);
	item.stats = &stats;
	bool success = ReadItemPacket(packet, &item);
	if (!success || !item.isGold || !item.ground ||
		(item.action != ITEM_ACTION_NEW_GROUND && item.action != ITEM_ACTION_OLD_GROUND))
		return false;

	UnitAny *player = D2CLIENT_GetPlayerUnit();
	if (!player)
		return false;
	// The pile's own area, not the character's: a drop packet from the far side
	// of a portal arrives before the character has been moved through it.
	if (IsTown(GetAreaAtPosition(item.x, item.y)))
		return false;
	unsigned int level = (unsigned int)D2COMMON_GetUnitStat(player, STAT_LEVEL, 0);
	if (level == 0)
		return false;
	unsigned int carried = (unsigned int)D2COMMON_GetUnitStat(player, STAT_GOLD, 0);
	return carried >= level * GOLD_CARRIED_PER_LEVEL;
}

bool ItemMover::Init() {
	BnetData* pData = (*p_D2LAUNCH_BnData);
	if (!pData) { return false; }
	int xpac = pData->nCharFlags & PLAYER_TYPE_EXPANSION;

	if (xpac) {
		stashLayout = p_D2CLIENT_StashLayout;
	}
	else {
		stashLayout = p_D2CLIENT_ClassicStashLayout;
	}
	inventoryLayout = p_D2CLIENT_InventoryLayout;
	cubeLayout = p_D2CLIENT_CubeLayout;

	//PrintText(1, "Got positions: %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d",
	//	INVENTORY_WIDTH,
	//	INVENTORY_HEIGHT,
	//	STASH_WIDTH,
	//	STASH_HEIGHT,
	//	CUBE_WIDTH,
	//	CUBE_HEIGHT,
	//	INVENTORY_LEFT,
	//	INVENTORY_TOP,
	//	STASH_LEFT,
	//	STASH_TOP,
	//	CUBE_LEFT,
	//	CUBE_TOP,
	//	CELL_SIZE
	//);

	return true;
}

ItemGrid::Grid ItemMover::GridFor(int location) const {
	const InventoryLayout *layout;
	if (location == STORAGE_INVENTORY) {
		layout = inventoryLayout;
	} else if (location == STORAGE_STASH) {
		layout = stashLayout;
	} else if (location == STORAGE_CUBE) {
		layout = cubeLayout;
	} else {
		return ItemGrid::Grid();
	}
	ItemGrid::Grid grid;
	grid.columns = layout->SlotWidth;
	grid.rows = layout->SlotHeight;
	grid.left = layout->Left;
	grid.top = layout->Top;
	grid.right = layout->Right;
	grid.bottom = layout->Bottom;
	grid.cellSize = CELL_SIZE;
	return grid;
}

bool ItemMover::LoadInventory(UnitAny *unit, int source, int sourceX, int sourceY, bool shiftState, bool ctrlState, int stashUI, int invUI) {
	bool returnValue = false;

	InventoryItemIds.assign(INVENTORY_WIDTH * INVENTORY_HEIGHT, 0);
	StashItemIds.assign(STASH_WIDTH * STASH_HEIGHT, 0);
	CubeItemIds.assign(CUBE_WIDTH * CUBE_HEIGHT, 0);

	unsigned int itemId = 0;
	BYTE itemXSize, itemYSize;
	bool cubeInInventory = false, cubeAnywhere = false;
	for (UnitAny *pItem = unit->pInventory->pFirstItem; pItem; pItem = NextInvItem(pItem)) {
		if (!pItem->pItemData || !pItem->pObjectPath) {
			continue;
		}
		ItemText *text = D2COMMON_GetItemText(pItem->dwTxtFileNo);
		if (!text) {
			continue;
		}

		BYTE location = pItem->pItemData->ItemLocation;
		std::vector<int> *cells;
		int width, height;
		if (location == STORAGE_INVENTORY) {
			cells = &InventoryItemIds;
			width = INVENTORY_WIDTH;
			height = INVENTORY_HEIGHT;
		} else if (location == STORAGE_STASH) {
			cells = &StashItemIds;
			width = STASH_WIDTH;
			height = STASH_HEIGHT;
		} else if (location == STORAGE_CUBE) {
			cells = &CubeItemIds;
			width = CUBE_WIDTH;
			height = CUBE_HEIGHT;
		} else {
			continue;
		}

		bool box = false;
		char *code = text->szCode;
		if (code[0] == 'b' && code[1] == 'o' && code[2] == 'x') {
			if (location == STORAGE_INVENTORY) {
				cubeInInventory = true;
				cubeAnywhere = true;
			}
			if (location == STORAGE_STASH) {
				cubeAnywhere = true;
			}
			box = true;
		}

		int xStart = pItem->pObjectPath->dwPosX;
		int yStart = pItem->pObjectPath->dwPosY;
		BYTE xSize = text->xSize;
		BYTE ySize = text->ySize;
		ItemGrid::Mark(cells->data(), width, height, xStart, yStart, xSize, ySize, pItem->dwUnitId);

		// If you click to move the cube into itself, your character ends up in
		// the amusing (and apparently permanent) state where he has no visible
		// cube and yet is unable to pick one up. Logging out does not fix it.
		// So we disable all cube movements to be on the safe side.
		if (location == source && !box &&
			ItemGrid::Fits(width, height, xStart, yStart, xSize, ySize) &&
			ItemGrid::Covers(xStart, yStart, xSize, ySize, sourceX, sourceY)) {
			itemId = pItem->dwUnitId;
			itemXSize = xSize;
			itemYSize = ySize;
		}
	}

	int destination;
	if (ctrlState && shiftState && ((stashUI && cubeAnywhere) || (invUI && cubeInInventory)) && source != STORAGE_CUBE) {
		destination = STORAGE_CUBE;
	} else if (ctrlState) {
		destination = STORAGE_NULL;  // i.e. the ground
	} else if (source == STORAGE_STASH || source == STORAGE_CUBE) {
		destination = STORAGE_INVENTORY;
	} else if (source == STORAGE_INVENTORY && D2CLIENT_GetUIState(UI_STASH)) {
		destination = STORAGE_STASH;
	} else if (source == STORAGE_INVENTORY && D2CLIENT_GetUIState(UI_CUBE)) {
		destination = STORAGE_CUBE;
	} else {
		return false;
	}

	// Find a spot for the item in the destination container
	if (itemId > 0) {
		returnValue = FindDestination(destination, itemId, itemXSize, itemYSize);
	}

	FirstInit = true;
	return returnValue;
}

bool ItemMover::FindDestination(int destination, unsigned int itemId, BYTE xSize, BYTE ySize) {
	const int *p = NULL;
	int width = 0, height = 0;
	if (destination == STORAGE_INVENTORY) {
		p = InventoryItemIds.data();
		width = INVENTORY_WIDTH;
		height = INVENTORY_HEIGHT;
	} else if (destination == STORAGE_STASH) {
		p = StashItemIds.data();
		width = STASH_WIDTH;
		height = STASH_HEIGHT;
	} else if (destination == STORAGE_CUBE) {
		p = CubeItemIds.data();
		width = CUBE_WIDTH;
		height = CUBE_HEIGHT;
	}

	bool found = false;
	int destX = 0, destY = 0;
	if (width) {
		bool first_y = true;
		for (int x = 0; x < width; x++) {
			for (int y = 0; y < height; y++) {
				if (ItemGrid::IsVacant(p, width, height, x, y, xSize, ySize)) {
					// Found an empty spot that's big enough for the item
					found = true;
					destX = x;
					destY = y;
					break;
				}
				if (xSize == 1) {
					if (first_y) {
						if (x + 1 < width) {
							x++;
							y--;
							first_y = false;
						}
					} else {
						first_y = true;
						x--;
					}
				}
			} // end y loop
			if (found) {
				break;
			}
			if (xSize == 2 && x % 2 == 0 && x + 2 >= width) {
				x = 0;
			} else {
				x++;
			}
		} // end x loop
	} else {
		found = true;
	}

	bool queued = false;
	if (found) {
		Lock();
		if (ActivePacket.startTicks == 0) {
			ActivePacket.itemId = itemId;
			ActivePacket.x = destX;
			ActivePacket.y = destY;
			ActivePacket.startTicks = BHGetTickCount();
			ActivePacket.destination = destination;
			queued = true;
		}
		Unlock();
	}

	return queued;
}

void ItemMover::PickUpItem() {
	BYTE PacketData[5] = {0x19,0,0,0,0};
	*reinterpret_cast<int*>(PacketData + 1) = ActivePacket.itemId;
	D2NET_SendPacket(5, 1, PacketData);
}

void ItemMover::PutItemInContainer() {
	BYTE PacketData[17] = {0x18,0,0,0,0};
	*reinterpret_cast<int*>(PacketData + 1) = ActivePacket.itemId;
	*reinterpret_cast<int*>(PacketData + 5) = ActivePacket.x;
	*reinterpret_cast<int*>(PacketData + 9) = ActivePacket.y;
	*reinterpret_cast<int*>(PacketData + 13)= ActivePacket.destination;
	D2NET_SendPacket(17, 1, PacketData);
}

void ItemMover::PutItemOnGround() {
	BYTE PacketData[5] = {0x17,0,0,0,0};
	*reinterpret_cast<int*>(PacketData + 1) = ActivePacket.itemId;
	D2NET_SendPacket(5, 1, PacketData);
}

void ItemMover::OnLeftClick(bool up, unsigned int x, unsigned int y, bool* block) {
	UnitAny *unit = D2CLIENT_GetPlayerUnit();
	bool shiftState = ((GetKeyState(VK_LSHIFT) & 0x80) || (GetKeyState(VK_RSHIFT) & 0x80));
	
	if (up || !unit || !unit->pInventory || !unit->pPath || !shiftState || D2CLIENT_GetCursorItem()>0 ||
		(!D2CLIENT_GetUIState(UI_INVENTORY) && !D2CLIENT_GetUIState(UI_STASH)
			&& !D2CLIENT_GetUIState(UI_CUBE) && !D2CLIENT_GetUIState(UI_NPCSHOP)) ||
		!Init()) {
		return;
	}

	unidItemId = 0;
	idBookId = 0;

	int mouseX = *p_D2CLIENT_MouseX;
	int mouseY = *p_D2CLIENT_MouseY;
	for (UnitAny *pItem = unit->pInventory->pFirstItem; pItem; pItem = NextInvItem(pItem)) {
		if (!pItem->pItemData || !pItem->pObjectPath) {
			continue;
		}
		ItemText *text = D2COMMON_GetItemText(pItem->dwTxtFileNo);
		if (!text) {
			continue;
		}

		char *code = text->szCode;
		BYTE location = pItem->pItemData->ItemLocation;
		int column, row;
		if (!(pItem->pItemData->dwFlags & ITEM_IDENTIFIED) &&
			ItemGrid::CellAt(GridFor(location), mouseX, mouseY, &column, &row) &&
			ItemGrid::Covers((int)pItem->pObjectPath->dwPosX, (int)pItem->pObjectPath->dwPosY,
				text->xSize, text->ySize, column, row)) {
			if ((location == STORAGE_STASH && !D2CLIENT_GetUIState(UI_STASH)) || (location == STORAGE_CUBE && !D2CLIENT_GetUIState(UI_CUBE))) {
				return;
			}
			unidItemId = pItem->dwUnitId;
		}
		if (code[0] == 'i' && code[1] == 'b' && code[2] == 'k' && location == STORAGE_INVENTORY && D2COMMON_GetUnitStat(pItem, STAT_AMMOQUANTITY, 0)>0) {
			idBookId = pItem->dwUnitId;
		}
		if (unidItemId > 0 && idBookId > 0) {
			BYTE PacketData[13] = { 0x20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
			*reinterpret_cast<int*>(PacketData + 1) = idBookId;
			*reinterpret_cast<WORD*>(PacketData + 5) = (WORD)unit->pPath->xPos;
			*reinterpret_cast<WORD*>(PacketData + 9) = (WORD)unit->pPath->yPos;
			D2NET_SendPacket(13, 0, PacketData);
			*block = true;
			return;
		}
	}
}

void ItemMover::OnRightClick(bool up, unsigned int x, unsigned int y, bool* block) {
	UnitAny *unit = D2CLIENT_GetPlayerUnit();
	bool shiftState = ((GetKeyState(VK_LSHIFT) & 0x80) || (GetKeyState(VK_RSHIFT) & 0x80));
	bool ctrlState = ((GetKeyState(VK_LCONTROL) & 0x80) || (GetKeyState(VK_RCONTROL) & 0x80));
	if (up || !unit || !unit->pInventory || !unit->pPath || !(shiftState || ctrlState) || !Init()) {
		return;
	}

	int source, sourceX, sourceY;
	int invUI = D2CLIENT_GetUIState(UI_INVENTORY);
	int stashUI = D2CLIENT_GetUIState(UI_STASH);
	int cubeUI = D2CLIENT_GetUIState(UI_CUBE);
	if ((invUI || stashUI || cubeUI) && ItemGrid::CellAt(GridFor(STORAGE_INVENTORY), (int)x, (int)y, &sourceX, &sourceY)) {
		source = STORAGE_INVENTORY;
	} else if (stashUI && ItemGrid::CellAt(GridFor(STORAGE_STASH), (int)x, (int)y, &sourceX, &sourceY)) {
		source = STORAGE_STASH;
	} else if (cubeUI && ItemGrid::CellAt(GridFor(STORAGE_CUBE), (int)x, (int)y, &sourceX, &sourceY)) {
		source = STORAGE_CUBE;
	} else {
		return;
	}

	bool moveItem = LoadInventory(unit, source, sourceX, sourceY, shiftState, ctrlState, stashUI, invUI);
	if (moveItem) {
		PickUpItem();
	}
	*block = true;
}

void ItemMover::LoadConfig() {
	BH::config->ReadKey("Use TP Tome", "VK_NUMPADADD", TpKey);
	BH::config->ReadKey("Use Healing Potion", "VK_NUMPADMULTIPLY", HealKey);
	BH::config->ReadKey("Use Mana Potion", "VK_NUMPADSUBTRACT", ManaKey);
	BH::config->ReadKey("Use Rejuv Potion", "VK_NUMPADDIVIDE", JuvKey);

	BH::config->ReadInt("Low TP Warning", tp_warn_quantity, 3);
}

void ItemMover::OnLoad() {
	LoadConfig();

	Settings::AddKey(GetName(), Settings::Category::Input, "Use TP Tome", "Quick town portal", &TpKey,
		"Opens a town portal from the tome in your inventory.");
	Settings::AddKey(GetName(), Settings::Category::Input, "Use Healing Potion", "Use healing potion",
		&HealKey, "Drinks the smallest healing potion you are carrying.");
	Settings::AddKey(GetName(), Settings::Category::Input, "Use Mana Potion", "Use mana potion",
		&ManaKey, "Drinks the smallest mana potion you are carrying.");
	Settings::AddKey(GetName(), Settings::Category::Input, "Use Rejuv Potion", "Use rejuv potion",
		&JuvKey, "Drinks the smallest rejuvenation potion you are carrying.");

	Settings::AddHeading(GetName(), Settings::Category::Input, "QOL hotkeys");
	Settings::AddNote(GetName(), Settings::Category::Input,
		"- Shift-leftclick identifies an item if there is an ID tome in your inventory.");
	Settings::AddNote(GetName(), Settings::Category::Input,
		"- Shift-rightclick moves an item between the stash or an open cube and your inventory.");
	Settings::AddNote(GetName(), Settings::Category::Input, "- Ctrl-rightclick moves an item to the ground.");
	Settings::AddNote(GetName(), Settings::Category::Input,
		"- Ctrl-shift-rightclick moves an item into the cube without opening it.");

}

void ItemMover::OnKey(bool up, BYTE key, LPARAM lParam, bool* block)  {
	UnitAny *unit = D2CLIENT_GetPlayerUnit();
	if (!unit || !unit->pInventory || !unit->pPath)
		return;

	if (!up && (key == HealKey || key == ManaKey || key == JuvKey)) {
		int idx = key == JuvKey ? 2 : key == ManaKey ? 1 : 0;
		std::string startChars = POTIONS[idx];
		char minPotion = 127;
		DWORD minItemId = 0;
		bool isBelt = false;
		for (UnitAny *pItem = unit->pInventory->pFirstItem; pItem; pItem = NextInvItem(pItem)) {
			if (!pItem->pItemData)
				continue;
			if (pItem->pItemData->ItemLocation == STORAGE_INVENTORY ||
				pItem->pItemData->ItemLocation == STORAGE_NULL && pItem->pItemData->NodePage == NODEPAGE_BELTSLOTS) {
				ItemText *text = D2COMMON_GetItemText(pItem->dwTxtFileNo);
				if (!text)
					continue;
				char* code = text->szCode;
				if (code[0] == startChars[0] && code[1] == startChars[1] && code[2] < minPotion) {
					minPotion = code[2];
					minItemId = pItem->dwUnitId;
					isBelt = pItem->pItemData->NodePage == NODEPAGE_BELTSLOTS;
				}
			}
			//char *code = D2COMMON_GetItemText(pItem->dwTxtFileNo)->szCode;
			//if (code[0] == 'b' && code[1] == 'o' && code[2] == 'x') {
			//	// Hack to pick up cube to fix cube-in-cube problem
			//	BYTE PacketDataCube[5] = {0x19,0,0,0,0};
			//	*reinterpret_cast<int*>(PacketDataCube + 1) = pItem->dwUnitId;
			//	D2NET_SendPacket(5, 1, PacketDataCube);
			//	break;
			//}
		}
		if (minItemId > 0) {
			if (isBelt){
				BYTE PacketData[13] = { 0x26, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
				*reinterpret_cast<int*>(PacketData + 1) = minItemId;
				D2NET_SendPacket(13, 0, PacketData);
			}
			else{
				//PrintText(1, "Sending packet %d, %d, %d", minItemId, unit->pPath->xPos, unit->pPath->yPos);
				BYTE PacketData[13] = { 0x20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
				*reinterpret_cast<int*>(PacketData + 1) = minItemId;
				*reinterpret_cast<WORD*>(PacketData + 5) = (WORD)unit->pPath->xPos;
				*reinterpret_cast<WORD*>(PacketData + 9) = (WORD)unit->pPath->yPos;
				D2NET_SendPacket(13, 0, PacketData);
			}
			*block = true;
		}
	}
	if (!up && (key == TpKey)) {
		DWORD tpId = 0;
		int tp_quantity = 0;
		for (UnitAny *pItem = unit->pInventory->pFirstItem; pItem; pItem = NextInvItem(pItem)) {
			if (pItem->pItemData && pItem->pItemData->ItemLocation == STORAGE_INVENTORY) {
				ItemText *text = D2COMMON_GetItemText(pItem->dwTxtFileNo);
				if (!text)
					continue;
				char* code = text->szCode;
				if (code[0] == 't' && code[1] == 'b' && code[2] =='k') {
					tp_quantity = D2COMMON_GetUnitStat(pItem, STAT_AMMOQUANTITY, 0);
					if (tp_quantity > 0) {
						tpId = pItem->dwUnitId;
						break;
					}
				}
			}
		}
		if (tpId > 0) {
			BYTE PacketData[13] = { 0x20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
			*reinterpret_cast<int*>(PacketData + 1) = tpId;
			*reinterpret_cast<WORD*>(PacketData + 5) = (WORD)unit->pPath->xPos;
			*reinterpret_cast<WORD*>(PacketData + 9) = (WORD)unit->pPath->yPos;
			if (tp_quantity < tp_warn_quantity) {
				PrintText(Red, "TP tome is running low!");
			}
			D2NET_SendPacket(13, 0, PacketData);
			*block = true;
		}
	}
}

void ItemMover::OnGamePacketRecv(BYTE* packet, bool* block) {
	switch (packet[0])
	{
	case 0x3F:
		{
			// We get this packet after our cursor change. Will only ID if we found book and item previously. packet[1] = 0 guarantees the cursor is changing to "id ready" state.
			if (packet[1] == 0 && idBookId > 0 && unidItemId > 0) {
				BYTE PacketData[9] = {0x27,0,0,0,0,0,0,0,0};
				*reinterpret_cast<int*>(PacketData + 1) = unidItemId;
				*reinterpret_cast<int*>(PacketData + 5) = idBookId;
				D2NET_SendPacket(9, 0, PacketData);
				*block = true;
				// Reseting variables after we ID an item so the next ID works.
				unidItemId = 0;
				idBookId = 0;
			}
			break;
		}
	case 0x9c:
		{
			// We get this packet after placing an item in a container or on the ground
			if (FirstInit) {
				BYTE action = packet[1];
				unsigned int itemId = *(unsigned int*)&packet[4];
				Lock();
				if (itemId == ActivePacket.itemId) {
					//PrintText(1, "Placed item id %d", itemId);
					ActivePacket.itemId = 0;
					ActivePacket.x = 0;
					ActivePacket.y = 0;
					ActivePacket.startTicks = 0;
					ActivePacket.destination = 0;
				}
				Unlock();
			}

			if ((*BH::MiscToggles2)["Hide Redundant Scrolls"].state && IsRedundantScroll(packet)) {
				*block = true;
				break;
			}

			if ((*BH::MiscToggles2)["Hide Redundant Gold Piles"].state && IsRedundantGold(packet)) {
				*block = true;
				break;
			}

			if ((*BH::MiscToggles2)["Advanced Item Display"].state) {
				ItemFacts item = {};
				ItemFactsPacket::PacketStats stats(item);
				item.stats = &stats;
				bool success = ReadItemPacket(packet, &item);
				// Where the item landed, which the packet gives as a position
				// and the rules ask for as an area.
				if (success && item.ground) {
					item.areaId = GetAreaAtPosition(item.x, item.y);
					item.areaLevel = GetAreaLevel(item.areaId);
				}
				// The world the item landed in, read once for all the rules.
				LiveContext context;
				//PrintText(1, "Item packet: %s, %s, %X, %d, %d", item.name.c_str(), item.code, item.attrs->flags, item.sockets, GetDefense(&item));
				if ((item.action == ITEM_ACTION_NEW_GROUND || item.action == ITEM_ACTION_OLD_GROUND) && success) {
					RuleLists lists = { &MapRuleList, &DoNotBlockRuleList,
						&IgnoreRuleList };
					RuleMatch match = MatchRules(lists, item, context.Context(),
						Item::GetPingLevel());
					bool showOnMap = match.showOnMap;
					bool noTracking = match.noTracking;
					auto pingLevel = match.pingLevel;
					auto color = match.color;
					unsigned int keepIndex = match.keepIndex;
					unsigned int ignoreIndex = match.ignoreIndex;
					bool blocked = match.blocked;
					if (ItemCapture::IsEnabled()) {
						ItemCapture::Outcome outcome = {};
						outcome.keepIndex = keepIndex;
						outcome.ignoreIndex = ignoreIndex;
						outcome.blocked = blocked;
						outcome.showOnMap = showOnMap;
						outcome.noTracking = noTracking;
						outcome.color = color;
						outcome.pingLevel = pingLevel;
						ItemCapture::RecordDrop((const unsigned char*)packet, item, outcome);
					}
					if (blocked) {
						*block = true;
						//PrintText(1, "Blocking item: %s, %s, %d", item.name.c_str(), item.code, item.amount);
					}
					//PrintText(1, "Item on ground: %s, %s, %s, %X", item.name.c_str(), item.code, item.attrs->category.c_str(), item.attrs->flags);
					if(!blocked && showOnMap && !(*BH::MiscToggles2)["Item Detailed Notifications"].state) {
						if (!noTracking && !IsTown(GetPlayerArea()) && (unsigned)pingLevel <= Item::GetTrackerPingLevel()) {
							ScreenInfo::AddDrop(item.name.c_str(), item.x, item.y);
						}
						if (color == UNDEFINED_COLOR) {
							color = ItemColorFromQuality(item.quality);
						}
						if (!noTracking && !IsTown(GetPlayerArea())) {
							// A rule that pings without announcing has no colour to
							// announce in, so the item's own stands for it.
							Session::NoteDrop({ item.id, item.name, item.code, item.quality,
								(unsigned int)(color == DEAD_COLOR ? ItemColorFromQuality(item.quality) : color),
								(unsigned int)(pingLevel < 0 ? 0 : pingLevel), item.areaId });
						}
						if ((*BH::MiscToggles2)["Item Drop Notifications"].state &&
								item.action == ITEM_ACTION_NEW_GROUND &&
								color != DEAD_COLOR
							 ) {
							PrintText(color, "%s%s",
									item.name.c_str(),
									(*BH::MiscToggles2)["Verbose Notifications"].state ? " \377c5drop" : ""
									);
						}
						if ((*BH::MiscToggles2)["Item Close Notifications"].state &&
								item.action == ITEM_ACTION_OLD_GROUND &&
								color != DEAD_COLOR
							 ) {
							PrintText(color, "%s%s",
									item.name.c_str(),
									(*BH::MiscToggles2)["Verbose Notifications"].state ? " \377c5close" : ""
									);
						}
					}
				}
			}
			break;
		}
	case 0x9d:
		{
			// We get this packet after picking up an item
			if (FirstInit) {
				BYTE action = packet[1];
				unsigned int itemId = *(unsigned int*)&packet[4];
				Lock();
				if (itemId == ActivePacket.itemId) {
					//PrintText(2, "Picked up item id %d", itemId);
					if (ActivePacket.destination == STORAGE_NULL) {
						PutItemOnGround();
					} else {
						PutItemInContainer();
					}
				}
				Unlock();
			}
			break;
		}
	default:
		break;
	}
	return;
}

void ItemMover::OnGameExit() {
	ActivePacket.itemId = 0;
	ActivePacket.x = 0;
	ActivePacket.y = 0;
	ActivePacket.startTicks = 0;
	ActivePacket.destination = 0;
}

// Code for reading the 0x9c bitstream (borrowed from heroin_glands)
