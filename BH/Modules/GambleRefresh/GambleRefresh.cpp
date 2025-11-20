#include "GambleRefresh.h"
#include "../../BH.h"
#include "../../Common.h"
#include "../../Constants.h"
#include "../../D2Helpers.h"
#include "../../D2Intercepts.h"
#include "../../D2Ptrs.h"

#define UNIT_NPC 1

typedef void(__stdcall* fnClickEntry)();

bool GambleRefresh::Init() {
    pNPCMenu = p_D2CLIENT_NPCMenu;
    pNPCMenuAmount = p_D2CLIENT_NPCMenuAmount;

    return (pNPCMenu != NULL && pNPCMenuAmount != NULL);
}

void GambleRefresh::OnLoad() {
    LoadConfig();

    // Create settings tab
    settingsTab = new Drawing::UITab("Gamble", BH::settingsUI);

    unsigned int x = 4;
    unsigned int y = 10;
    int keyhook_x = 230;

    new Drawing::Checkhook(settingsTab, x, y, &Toggles["Gamble Refresh"].state, "Gamble Refresh");
    new Drawing::Keyhook(settingsTab, keyhook_x, (y + 2), &Toggles["Gamble Refresh"].toggle, "");   

}

void GambleRefresh::LoadConfig() {
    BH::config->ReadToggle("Gamble Refresh", "VK_F5", true, Toggles["Gamble Refresh"]);
}

void GambleRefresh::OnKey(bool bUp, BYTE key, LPARAM lParam, bool* block) {
    if (!D2CLIENT_GetPlayerUnit()) return;
    if (bUp) return;
    if (!Toggles["Gamble Refresh"].state) return;

    if (key == Toggles["Gamble Refresh"].toggle && refreshState == STATE_IDLE) {
        RefreshGambleWindow();
        *block = true;
        return;
    }

}

void GambleRefresh::RefreshGambleWindow() {
    UnitAny* pNpc = D2CLIENT_GetCurrentInteractingNPC();
    if (!pNpc || (pNpc->dwTxtFileNo != NPCID_Gheed && pNpc->dwTxtFileNo != NPCID_Jamella)) return;
    stateStartTime = GetTickCount();
    
    // Block spam presses from firing
    if (stateStartTime - stateEndTime < 200) return;

    npcId = pNpc->dwUnitId;
    refreshState = STATE_CLOSING;


    D2CLIENT_CloseNPCInteract();
}

void GambleRefresh::OnLoop() {
    if (refreshState == STATE_IDLE) return;

    DWORD elapsed = GetTickCount() - stateStartTime;

    switch (refreshState) {
    case STATE_CLOSING:
        refreshState = STATE_WAIT_CLOSED;
        stateStartTime = GetTickCount();
        break;

    case STATE_WAIT_CLOSED:
        if (!D2CLIENT_GetUIState(UI_NPCMENU)) {
            if (elapsed > 0) {
                refreshState = STATE_REOPENING;
                stateStartTime = GetTickCount();
            }
        }
        else if (elapsed > 2000) {
            PrintText(Red, "Gamble refresh close timeout");
            refreshState = STATE_IDLE;
        }
        break;

    case STATE_REOPENING:
        // Send interact packet
        {
            BYTE interactPacket[9];
            interactPacket[0] = 0x13;
            *(int*)&interactPacket[1] = 1;
            *(int*)&interactPacket[5] = npcId;
            D2NET_SendPacket(9, 0, interactPacket);
        }
        refreshState = STATE_WAIT_OPEN;
        stateStartTime = GetTickCount();
        break;

    case STATE_WAIT_OPEN:
        if (D2CLIENT_GetUIState(UI_NPCMENU)) {
            refreshState = STATE_CLICKING_GAMBLE;
            stateStartTime = GetTickCount();

        }
        else if (elapsed > 2000) {
            PrintText(Red, "Gamble refresh open timeout");
            refreshState = STATE_IDLE;
        }
        break;

    case STATE_CLICKING_GAMBLE:
        if (elapsed > 2000) {
            PrintText(Red, "Gamble refresh menu timeout");
            refreshState = STATE_IDLE;
            break;
        }

        if (elapsed < 1) break;

        TryClickGamble();
        break;
    }
}

void GambleRefresh::TryClickGamble() {
    if (!Init()) return;

    Lock();

    NPCMenu* pMenu = (NPCMenu*)p_D2CLIENT_NPCMenu;
    DWORD menuAmount = *p_D2CLIENT_NPCMenuAmount;

    if (!pMenu) {
        Unlock();
        return;
    }

    if (pMenu && menuAmount > 0) {
        for (UINT i = 0; i < menuAmount && i < 10; i++) {
            if (pMenu->dwNPCClassId == NPCID_Gheed) {
                // Gheed: gamble is 3rd option
                fnClickEntry pClick = (fnClickEntry)pMenu->dwEntryFunc3;
                if (pClick) {
                    Unlock();
                    pClick();
                    refreshState = STATE_IDLE;
                    stateEndTime = GetTickCount();
                    return;
                }
                break;
            } 
            else if (pMenu->dwNPCClassId == NPCID_Jamella) {
                // Jamella: gamble is 2nd option
                fnClickEntry pClick = (fnClickEntry)pMenu->dwEntryFunc2;
                if (pClick) {
                    Unlock();
                    pClick();
                    refreshState = STATE_IDLE;
                    stateEndTime = GetTickCount();
                    return;
                }
                break;
            }
            pMenu = (NPCMenu*)((DWORD)pMenu + sizeof(NPCMenu));
        }
    }

    Unlock();
}