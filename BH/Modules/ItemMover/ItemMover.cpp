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

// This module was inspired by the RedVex plugin "Item Mover", written by kaiks.
// Thanks to kaiks for sharing his code.

#define INVENTORY_WIDTH  inventoryLayout->SlotWidth
#define INVENTORY_HEIGHT inventoryLayout->SlotHeight
#define INVENTORY_LEFT   inventoryLayout->Left
#define INVENTORY_RIGHT  inventoryLayout->Right
#define INVENTORY_TOP    inventoryLayout->Top
#define INVENTORY_BOTTOM inventoryLayout->Bottom

#define STASH_WIDTH  stashLayout->SlotWidth
#define STASH_HEIGHT stashLayout->SlotHeight
#define STASH_LEFT   stashLayout->Left
#define STASH_RIGHT  stashLayout->Right
#define STASH_TOP    stashLayout->Top
#define STASH_BOTTOM stashLayout->Bottom

#define CUBE_WIDTH  cubeLayout->SlotWidth
#define CUBE_HEIGHT cubeLayout->SlotHeight
#define CUBE_LEFT   cubeLayout->Left
#define CUBE_RIGHT  cubeLayout->Right
#define CUBE_TOP    cubeLayout->Top
#define CUBE_BOTTOM cubeLayout->Bottom

#define CELL_SIZE inventoryLayout->SlotPixelHeight

std::string POTIONS[] = { "hp", "mp", "rv" };

DWORD idBookId;
DWORD unidItemId;

// Returns false when no matching tome is carried.
static bool AllTomesAboveThreshold(UnitAny *unit, const char *tomeCode, unsigned int threshold) {
	if (!unit || !unit->pInventory)
		return false;
	bool foundTome = false;
	for (UnitAny *pItem = unit->pInventory->pFirstItem; pItem; pItem = pItem->pItemData->pNextInvItem) {
		if (pItem->pItemData->ItemLocation != STORAGE_INVENTORY)
			continue;
		char* code = D2COMMON_GetItemText(pItem->dwTxtFileNo)->szCode;
		if (code[0] != tomeCode[0] || code[1] != tomeCode[1] || code[2] != tomeCode[2])
			continue;
		foundTome = true;
		if ((unsigned int)D2COMMON_GetUnitStat(pItem, STAT_AMMOQUANTITY, 0) <= threshold)
			return false;
	}
	return foundTome;
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
// every matching tome is still above the visibility threshold.
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
	return AllTomesAboveThreshold(D2CLIENT_GetPlayerUnit(), tomeCode, Item::GetScrollVisibilityThreshold());
}

bool ItemMover::Init() {
	BnetData* pData = (*p_D2LAUNCH_BnData);
	if (!pData) { return false; }
	int xpac = pData->nCharFlags & PLAYER_TYPE_EXPANSION;

	if (xpac) {
		stashLayout = p_D2CLIENT_StashLayout;
		StashItemIds = LODStashItemIds;
	}
	else {
		stashLayout = p_D2CLIENT_ClassicStashLayout;
		StashItemIds = ClassicStashItemIds;
	}
	inventoryLayout = p_D2CLIENT_InventoryLayout;
	cubeLayout = p_D2CLIENT_CubeLayout;

	if (!InventoryItemIds) {
		InventoryItemIds = new int[INVENTORY_WIDTH * INVENTORY_HEIGHT];
	}
	if (!StashItemIds) {
		StashItemIds = new int[STASH_WIDTH * STASH_HEIGHT];
	}
	if (!CubeItemIds) {
		CubeItemIds = new int[CUBE_WIDTH * CUBE_HEIGHT];
	}

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

bool ItemMover::LoadInventory(UnitAny *unit, int source, int sourceX, int sourceY, bool shiftState, bool ctrlState, int stashUI, int invUI) {
	bool returnValue = false;

	memset(InventoryItemIds, 0, INVENTORY_WIDTH * INVENTORY_HEIGHT * sizeof(int));
	memset(StashItemIds, 0, STASH_WIDTH * STASH_HEIGHT * sizeof(int));
	memset(CubeItemIds, 0, CUBE_WIDTH * CUBE_HEIGHT * sizeof(int));

	unsigned int itemId = 0;
	BYTE itemXSize, itemYSize;
	bool cubeInInventory = false, cubeAnywhere = false;
	for (UnitAny *pItem = unit->pInventory->pFirstItem; pItem; pItem = pItem->pItemData->pNextInvItem) {
		int *p, width;
		if (pItem->pItemData->ItemLocation == STORAGE_INVENTORY) {
			p = InventoryItemIds;
			width = INVENTORY_WIDTH;
		} else if (pItem->pItemData->ItemLocation == STORAGE_STASH) {
			p = StashItemIds;
			width = STASH_WIDTH;
		} else if (pItem->pItemData->ItemLocation == STORAGE_CUBE) {
			p = CubeItemIds;
			width = CUBE_WIDTH;
		} else {
			continue;
		}

		bool box = false;
		char *code = D2COMMON_GetItemText(pItem->dwTxtFileNo)->szCode;
		if (code[0] == 'b' && code[1] == 'o' && code[2] == 'x') {
			if (pItem->pItemData->ItemLocation == STORAGE_INVENTORY) {
				cubeInInventory = true;
				cubeAnywhere = true;
			}
			if (pItem->pItemData->ItemLocation == STORAGE_STASH) {
				cubeAnywhere = true;
			}
			box = true;
		}

		int xStart = pItem->pObjectPath->dwPosX;
		int yStart = pItem->pObjectPath->dwPosY;
		BYTE xSize = D2COMMON_GetItemText(pItem->dwTxtFileNo)->xSize;
		BYTE ySize = D2COMMON_GetItemText(pItem->dwTxtFileNo)->ySize;
		for (int x = xStart; x < xStart + xSize; x++) {
			for (int y = yStart; y < yStart + ySize; y++) {
				p[y*width + x] = pItem->dwUnitId;

				// If you click to move the cube into itself, your character ends up in
				// the amusing (and apparently permanent) state where he has no visible
				// cube and yet is unable to pick one up. Logging out does not fix it.
				// So we disable all cube movements to be on the safe side.
				if (x == sourceX && y == sourceY && pItem->pItemData->ItemLocation == source && !box) {
					// This is the item we want to move
					itemId = pItem->dwUnitId;
					itemXSize = xSize;
					itemYSize = ySize;
				}
			}
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
	int *p, width = 0, height = 0;
	if (destination == STORAGE_INVENTORY) {
		p = InventoryItemIds;
		width = INVENTORY_WIDTH;
		height = INVENTORY_HEIGHT;
	} else if (destination == STORAGE_STASH) {
		p = StashItemIds;
		width = STASH_WIDTH;
		height = STASH_HEIGHT;
	} else if (destination == STORAGE_CUBE) {
		p = CubeItemIds;
		width = CUBE_WIDTH;
		height = CUBE_HEIGHT;
	}

	bool found = false;
	int destX = 0, destY = 0;
	if (width) {
		bool first_y = true;
		for (int x = 0; x < width; x++) {
			for (int y = 0; y < height; y++) {
				bool abort = false;
				int vacancies = 0;
				for (int testx = x; testx < x + xSize && testx < width; testx++) {
					for (int testy = y; testy < y + ySize && testy < height; testy++) {
						if (p[testy*width + testx]) {
							abort = true;
							break;
						} else {
							vacancies++;
						}
					}
					if (abort) {
						break;
					}
				}
				if (vacancies == xSize * ySize) {
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

	if (found) {
		Lock();
		if (ActivePacket.startTicks == 0) {
			ActivePacket.itemId = itemId;
			ActivePacket.x = destX;
			ActivePacket.y = destY;
			ActivePacket.startTicks = BHGetTickCount();
			ActivePacket.destination = destination;
		}
		Unlock();
	}

	return found;
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
	
	if (up || !unit || !shiftState || D2CLIENT_GetCursorItem()>0 ||
		(!D2CLIENT_GetUIState(UI_INVENTORY) && !D2CLIENT_GetUIState(UI_STASH)
			&& !D2CLIENT_GetUIState(UI_CUBE) && !D2CLIENT_GetUIState(UI_NPCSHOP)) ||
		!Init()) {
		return;
	}

	unidItemId = 0;
	idBookId = 0;
	
	int mouseX,mouseY;	

	for (UnitAny *pItem = unit->pInventory->pFirstItem; pItem; pItem = pItem->pItemData->pNextInvItem) {
		char *code = D2COMMON_GetItemText(pItem->dwTxtFileNo)->szCode;
		if ((pItem->pItemData->dwFlags & ITEM_IDENTIFIED) <= 0) {
			int xStart = pItem->pObjectPath->dwPosX;
			int yStart = pItem->pObjectPath->dwPosY;
			BYTE xSize = D2COMMON_GetItemText(pItem->dwTxtFileNo)->xSize;
			BYTE ySize = D2COMMON_GetItemText(pItem->dwTxtFileNo)->ySize;
			if (pItem->pItemData->ItemLocation == STORAGE_INVENTORY) {
				mouseX = (*p_D2CLIENT_MouseX - INVENTORY_LEFT) / CELL_SIZE;
				mouseY = (*p_D2CLIENT_MouseY - INVENTORY_TOP) / CELL_SIZE;
			} else if(pItem->pItemData->ItemLocation == STORAGE_STASH) {
				mouseX = (*p_D2CLIENT_MouseX - STASH_LEFT) / CELL_SIZE;
				mouseY = (*p_D2CLIENT_MouseY - STASH_TOP) / CELL_SIZE;
			} else if(pItem->pItemData->ItemLocation == STORAGE_CUBE) {
				mouseX = (*p_D2CLIENT_MouseX - CUBE_LEFT) / CELL_SIZE;
				mouseY = (*p_D2CLIENT_MouseY - CUBE_TOP) / CELL_SIZE;
			}
			for (int x = xStart; x < xStart + xSize; x++) {
				for (int y = yStart; y < yStart + ySize; y++) {
					if (x == mouseX && y == mouseY) {
						if ((pItem->pItemData->ItemLocation == STORAGE_STASH && !D2CLIENT_GetUIState(UI_STASH)) || (pItem->pItemData->ItemLocation == STORAGE_CUBE && !D2CLIENT_GetUIState(UI_CUBE))) {
							return;
						}
						unidItemId = pItem->dwUnitId;								
					}
				}
			}
		}
		if (code[0] == 'i' && code[1] == 'b' && code[2] == 'k' && pItem->pItemData->ItemLocation == STORAGE_INVENTORY && D2COMMON_GetUnitStat(pItem, STAT_AMMOQUANTITY, 0)>0) {
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
	if (up || !unit || !(shiftState || ctrlState) || !Init()) {
		return;
	}

	int source, sourceX, sourceY;
	int invUI = D2CLIENT_GetUIState(UI_INVENTORY);
	int stashUI = D2CLIENT_GetUIState(UI_STASH);
	int cubeUI = D2CLIENT_GetUIState(UI_CUBE);
	if ((invUI || stashUI || cubeUI) && x >= INVENTORY_LEFT && x <= INVENTORY_RIGHT && y >= INVENTORY_TOP && y <= INVENTORY_BOTTOM) {
		source = STORAGE_INVENTORY;
		sourceX = (x - INVENTORY_LEFT) / CELL_SIZE;
		sourceY = (y - INVENTORY_TOP) / CELL_SIZE;
	} else if (stashUI && x >= STASH_LEFT && x <= STASH_RIGHT && y >= STASH_TOP && y <= STASH_BOTTOM) {
		source = STORAGE_STASH;
		sourceX = (x - STASH_LEFT) / CELL_SIZE;
		sourceY = (y - STASH_TOP) / CELL_SIZE;
	} else if (cubeUI && x >= CUBE_LEFT && x <= CUBE_RIGHT && y >= CUBE_TOP && y <= CUBE_BOTTOM) {
		source = STORAGE_CUBE;
		sourceX = (x - CUBE_LEFT) / CELL_SIZE;
		sourceY = (y - CUBE_TOP) / CELL_SIZE;
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

	BH::config->ReadInt("Low TP Warning", tp_warn_quantity);
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
	if (!unit)
		return;

	if (!up && (key == HealKey || key == ManaKey || key == JuvKey)) {
		int idx = key == JuvKey ? 2 : key == ManaKey ? 1 : 0;
		std::string startChars = POTIONS[idx];
		char minPotion = 127;
		DWORD minItemId = 0;
		bool isBelt = false;
		for (UnitAny *pItem = unit->pInventory->pFirstItem; pItem; pItem = pItem->pItemData->pNextInvItem) {
			if (pItem->pItemData->ItemLocation == STORAGE_INVENTORY ||
				pItem->pItemData->ItemLocation == STORAGE_NULL && pItem->pItemData->NodePage == NODEPAGE_BELTSLOTS) {
				char* code = D2COMMON_GetItemText(pItem->dwTxtFileNo)->szCode;
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
		for (UnitAny *pItem = unit->pInventory->pFirstItem; pItem; pItem = pItem->pItemData->pNextInvItem) {
			if (pItem->pItemData->ItemLocation == STORAGE_INVENTORY) {
				char* code = D2COMMON_GetItemText(pItem->dwTxtFileNo)->szCode;
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

			if ((*BH::MiscToggles2)["Advanced Item Display"].state) {
				ItemFacts item = {};
				ItemFactsPacket::PacketStats stats(item);
				item.stats = &stats;
				bool success = ReadItemPacket(packet, &item);
				// The world the item landed in, read once for all the rules.
				LiveContext context;
				//PrintText(1, "Item packet: %s, %s, %X, %d, %d", item.name.c_str(), item.code, item.attrs->flags, item.sockets, GetDefense(&item));
				if ((item.action == ITEM_ACTION_NEW_GROUND || item.action == ITEM_ACTION_OLD_GROUND) && success) {
					bool showOnMap = false;
					bool noTracking = false;
					auto pingLevel = -1;
					auto color = UNDEFINED_COLOR;
					// config position of the earliest rule that wants this item kept, and of the
					// earliest one that wants it hidden. Ordered filtering compares the two.
					unsigned int keepIndex = NO_RULE_MATCH;
					unsigned int ignoreIndex = NO_RULE_MATCH;

					for (vector<Rule*>::iterator it = MapRuleList.begin(); it != MapRuleList.end(); it++) {
						if ((*it)->Evaluate(item, context.Context())) {
							if ((*it)->action.index < keepIndex) keepIndex = (*it)->action.index;
							// skip map and notification if ping level requirement is not met
							if ((*it)->action.pingLevel > Item::GetPingLevel()) continue;
							auto action_color = (*it)->action.notifyColor;
							// never overwrite color with an undefined color. never overwrite a defined color with dead color.
							if (action_color != UNDEFINED_COLOR && (action_color != DEAD_COLOR || color == UNDEFINED_COLOR))
								color = action_color;
							showOnMap = true;
							noTracking = (*it)->action.noTracking;
							pingLevel = (*it)->action.pingLevel;
							// break unless %CONTINUE% is used
							if ((*it)->action.stopProcessing) break;
						}
					}
					// Don't block items that have a white-listed name
					for (vector<Rule*>::iterator it = DoNotBlockRuleList.begin(); it != DoNotBlockRuleList.end(); it++) {
						if ((*it)->Evaluate(item, context.Context())) {
							if ((*it)->action.index < keepIndex) keepIndex = (*it)->action.index;
							break;
						}
					}
					// With ordered filtering off this list only matters when nothing kept the item,
					// so skip the scan entirely in that case to keep the old cost.
					if (OrderedFiltering || keepIndex == NO_RULE_MATCH) {
						for (vector<Rule*>::iterator it = IgnoreRuleList.begin(); it != IgnoreRuleList.end(); it++) {
							if ((*it)->Evaluate(item, context.Context())) {
								ignoreIndex = (*it)->action.index;
								break;
							}
						}
					}
					bool blocked = IsItemBlocked(ignoreIndex, keepIndex);
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
