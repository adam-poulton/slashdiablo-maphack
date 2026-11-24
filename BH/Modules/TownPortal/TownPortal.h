#pragma once
#include <atomic>
#include "../Module.h"
#include "../../D2Structs.h"
#include "PortalDelay.h"

// The re-entry delay, in milliseconds: what the slider offers and what it starts
// at. The game's own wait lasts as long as the state does, so a delay longer
// than that changes nothing.
#define MIN_PORTAL_DELAY		500
#define MAX_PORTAL_DELAY		3000
#define STEP_PORTAL_DELAY		250
#define DEFAULT_PORTAL_DELAY	1000

class TownPortal : public Module {
	private:
		static bool shorten;
		static unsigned int delay;
		static PortalDelay::Clock clock;

		// Set by OnGameJoin(), which runs on BH's own thread, and acted on by the
		// game loop, which is the thread the clock lives on.
		static std::atomic<bool> joined;

		static void ApplyPatch();

	public:
		TownPortal() : Module("Town Portal") {};

		void OnLoad();
		void OnUnload();
		void LoadConfig();
		void OnSettingsChanged(const vector<string>& keys);

		void OnGameJoin();
		void OnLoop();
		void OnGamePacketRecv(BYTE* packet, bool* block);

		// Stands in for D2COMMON_GetUnitState at the client's town portal check, so
		// it keeps that call's contract: stdcall, non-zero refuses the portal.
		static int __stdcall CheckPortal(UnitAny* pUnit, DWORD nState);
};
