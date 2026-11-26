// Points this process's HKEY_CURRENT_USER at a sandbox's own key, so that the
// game reads and writes its options, save path and realm there instead of in
// the player's. AgentLaunch loads it into Game.exe before any of the game's own
// code has run: BH itself is loaded far too late for this, from D2Win after the
// MPQs are open (D2Win.dll 1.13c+D2Template, 0x6F8FA2A1).
//
// It fails closed. A game this cannot redirect is ended here, because the
// alternative is a game running against the player's registry.

#include <windows.h>

#include <cwchar>

namespace {

constexpr wchar_t kRegistryVariable[] = L"BHAGENT_REGISTRY";

// Exit codes AgentLaunch reports by name. Kept clear of the small numbers the
// game and the CRT use for their own exits.
constexpr UINT kNoRegistryVariable = 0xBA0001;
constexpr UINT kNoSandboxKey = 0xBA0002;
constexpr UINT kOverrideFailed = 0xBA0003;

[[noreturn]] void Abort(UINT code) {
	TerminateProcess(GetCurrentProcess(), code);
	ExitProcess(code);
}

void Redirect() {
	wchar_t subkey[512];
	DWORD length = GetEnvironmentVariableW(kRegistryVariable, subkey, ARRAYSIZE(subkey));
	if (length == 0 || length >= ARRAYSIZE(subkey)) {
		Abort(kNoRegistryVariable);
	}

	// Opened, never created: the sandbox's key is written before launch, and a
	// missing one means the launch was not set up by the harness.
	HKEY sandbox = nullptr;
	if (RegOpenKeyExW(HKEY_CURRENT_USER, subkey, 0, KEY_ALL_ACCESS, &sandbox) != ERROR_SUCCESS) {
		Abort(kNoSandboxKey);
	}
	LSTATUS overridden = RegOverridePredefKey(HKEY_CURRENT_USER, sandbox);
	RegCloseKey(sandbox);
	if (overridden != ERROR_SUCCESS) {
		Abort(kOverrideFailed);
	}

	// Tells AgentLaunch the redirect is in place; it ends the game if this
	// never arrives.
	wchar_t eventName[64];
	swprintf_s(eventName, L"Local\\BHAgentShim-%lu", GetCurrentProcessId());
	if (HANDLE ready = OpenEventW(EVENT_MODIFY_STATE, FALSE, eventName)) {
		SetEvent(ready);
		CloseHandle(ready);
	}
}

}  // namespace

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
	if (reason == DLL_PROCESS_ATTACH) {
		DisableThreadLibraryCalls(instance);
		Redirect();
	}
	return TRUE;
}
