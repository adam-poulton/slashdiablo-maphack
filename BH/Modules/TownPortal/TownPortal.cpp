#include "TownPortal.h"
#include "../Settings/SettingsRegistry.h"
#include "../../BH.h"
#include "../../Constants.h"
#include "../../D2Ptrs.h"

// D2Client 1.13c, where interacting with an object asks whether the player can
// take a town portal (object 59):
//
//     push 0x66                     ; STATE_JUST_PORTALED
//     push edi                      ; the player unit
//     call D2COMMON_GetUnitState    ; stdcall, through the import thunk
//     test eax, eax                 ; non-zero refuses the portal
//
// The patch replaces the 5-byte call with one to CheckPortal.
Patch* portalCheckPatch = new Patch(Call, D2CLIENT, { 0x1AB81, 0 }, (int)TownPortal::CheckPortal, 5);

bool TownPortal::shorten;
unsigned int TownPortal::delay;
PortalDelay::Clock TownPortal::clock;
std::atomic<bool> TownPortal::joined(false);

void TownPortal::OnLoad() {
	shorten = false;
	delay = DEFAULT_PORTAL_DELAY;
	LoadConfig();

	Settings::AddHeading(GetName(), Settings::Category::Input, "Town portal");
	Settings::AddSlider(GetName(), Settings::Category::Input, "Portal Reentry Delay",
		"Portal re-entry delay", &delay, 0, MAX_PORTAL_DELAY, STEP_PORTAL_DELAY, " ms",
		"How long after taking a town portal before you can take one again. "
		"Off leaves the game's own wait.",
		"", &shorten);
}

void TownPortal::OnUnload() {
	portalCheckPatch->Remove();
}

void TownPortal::LoadConfig() {
	BH::config->ReadBoolean("Shorten Portal Reentry Delay", shorten);
	BH::config->ReadInt("Portal Reentry Delay", delay, DEFAULT_PORTAL_DELAY);
	if (delay > MAX_PORTAL_DELAY)
		delay = MAX_PORTAL_DELAY;
	ApplyPatch();
}

void TownPortal::OnSettingsChanged(const vector<string>& keys) {
	ApplyPatch();
}

void TownPortal::ApplyPatch() {
	if (shorten)
		portalCheckPatch->Install();
	else
		portalCheckPatch->Remove();
}

void TownPortal::OnGameJoin() {
	joined = true;
}

void TownPortal::OnLoop() {
	if (joined.exchange(false))
		clock.Reset();

	UnitAny* player = D2CLIENT_GetPlayerUnit();
	if (!player)
		return;
	clock.Observe(D2COMMON_GetUnitState(player, STATE_JUST_PORTALED) != 0, GetTickCount());
}

void TownPortal::OnGamePacketRecv(BYTE* packet, bool* block) {
	// 0xA8 sets a state on a unit: the unit's type at 1, its id at 2, the state
	// at 7.
	if (packet[0] != 0xA8 || packet[7] != STATE_JUST_PORTALED || packet[1] != UNIT_PLAYER)
		return;

	UnitAny* player = D2CLIENT_GetPlayerUnit();
	if (player && *(DWORD*)&packet[2] == player->dwUnitId)
		clock.Gained(GetTickCount());
}

int __stdcall TownPortal::CheckPortal(UnitAny* pUnit, DWORD nState) {
	int state = D2COMMON_GetUnitState(pUnit, nState);
	if (!state || nState != STATE_JUST_PORTALED || pUnit != D2CLIENT_GetPlayerUnit())
		return state;
	return clock.Refuses(true, GetTickCount(), delay) ? state : 0;
}
