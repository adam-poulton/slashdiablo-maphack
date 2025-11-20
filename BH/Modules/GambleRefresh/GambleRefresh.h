#pragma once
#include "../Module.h"
#include "../../D2Structs.h"
#include "../../Drawing.h"

enum RefreshState {
    STATE_IDLE,
    STATE_CLOSING,
    STATE_WAIT_CLOSED,
    STATE_REOPENING,
    STATE_WAIT_OPEN,
    STATE_CLICKING_GAMBLE
};

class GambleRefresh : public Module {
private:
    CRITICAL_SECTION crit;
    Drawing::UITab* settingsTab;
    NPCMenu** pNPCMenu;
    DWORD* pNPCMenuAmount;
    RefreshState refreshState;
    DWORD stateStartTime;
    DWORD stateEndTime;
    DWORD npcId;
    map<std::string, Toggle> Toggles;

public:
    GambleRefresh() : Module("Gamble Refresh"),
        refreshState(STATE_IDLE),
        stateStartTime(0),
        stateEndTime(0),
        npcId(0) {

        InitializeCriticalSection(&crit);
    };

    ~GambleRefresh() {
        DeleteCriticalSection(&crit);
    };

    void Lock() { EnterCriticalSection(&crit); };
    void Unlock() { LeaveCriticalSection(&crit); };
    bool Init();
    void LoadConfig();

    void OnLoop();
    void OnLoad();
    void OnKey(bool bUp, BYTE key, LPARAM lParam, bool* block);

    void RefreshGambleWindow();
    void TryClickGamble();
    bool IsGambleWindowOpen();
};