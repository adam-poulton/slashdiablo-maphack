// Starts one sandboxed Game.exe for an agent and lives exactly as long as it.
//
//   AgentLaunch.exe --sandbox <dir> --registry <HKCU subkey> --desktop <name>
//                   [--lifetime <seconds>] [-- <game args>]
//
// The game runs on a desktop of its own, so no window of it ever reaches the
// player's screen or taskbar, and inside a job that ends it when this process
// ends, however that happens, or when its lifetime (an hour unless given) runs
// out. This process is also the only way to tell whether the game is still
// running: Game.exe denies every other process access to itself, down to
// PROCESS_QUERY_LIMITED_INFORMATION, so only the handle CreateProcess returns
// can see it. Its HKEY_CURRENT_USER is redirected to
// <subkey> by AgentShim.dll, which is loaded into the suspended process before
// Game.exe's own code runs; the game is never resumed past that point unless
// the shim reports the redirect is in place.
//
// Prints "pid=<n>" once the game is running, then waits for it and exits with
// its exit code. Setting the event Local\BHAgentStop-<this process's pid> ends
// the game, and this process exits only once the game has: that is how a
// caller knows the sandbox's files are free. Errors go to stderr with a
// nonzero exit code of this process's own (1 to 11).

#include <windows.h>

#include <cstdio>
#include <string>
#include <vector>

namespace {

constexpr wchar_t kSandboxMarker[] = L"BHAgent.sandbox";
constexpr wchar_t kShimName[] = L"AgentShim.dll";
constexpr DWORD kShimTimeoutMs = 10000;
constexpr DWORD kDefaultLifetimeSeconds = 3600;

enum Exit : int {
	kUsage = 1,
	kNotASandbox = 2,
	kNoShim = 3,
	kNoSandboxKey = 4,
	kDesktopFailed = 5,
	kJobFailed = 6,
	kStartFailed = 7,
	kInjectFailed = 8,
	kShimFailed = 9,
	kLifetimeExpired = 10,
	kStopped = 11,
};

struct Options {
	std::wstring sandbox;
	std::wstring registry;
	std::wstring desktop;
	DWORD lifetimeSeconds = kDefaultLifetimeSeconds;
	std::vector<std::wstring> gameArgs;
};

HANDLE g_job = nullptr;

int Fail(int code, const wchar_t* what) {
	fwprintf(stderr, L"AgentLaunch: %ls (error %lu)\n", what, GetLastError());
	return code;
}

bool FileExists(const std::wstring& path) {
	DWORD attributes = GetFileAttributesW(path.c_str());
	return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
}

std::wstring Join(const std::wstring& dir, const wchar_t* name) {
	std::wstring path = dir;
	if (!path.empty() && path.back() != L'\\') {
		path += L'\\';
	}
	return path + name;
}

std::wstring OwnDirectory() {
	wchar_t path[MAX_PATH];
	DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);
	std::wstring dir(path, length);
	return dir.substr(0, dir.find_last_of(L'\\'));
}

// Quotes one argument the way CommandLineToArgvW reads it back.
void AppendArgument(std::wstring& line, const std::wstring& arg) {
	line += L' ';
	if (!arg.empty() && arg.find_first_of(L" \t\"") == std::wstring::npos) {
		line += arg;
		return;
	}
	line += L'"';
	size_t backslashes = 0;
	for (wchar_t c : arg) {
		if (c == L'\\') {
			++backslashes;
			continue;
		}
		line.append(c == L'"' ? backslashes * 2 + 1 : backslashes, L'\\');
		backslashes = 0;
		line += c;
	}
	line.append(backslashes * 2, L'\\');
	line += L'"';
}

bool Parse(int argc, wchar_t** argv, Options& options) {
	for (int i = 1; i < argc; ++i) {
		std::wstring arg = argv[i];
		if (arg == L"--") {
			options.gameArgs.assign(argv + i + 1, argv + argc);
			break;
		}
		if (i + 1 >= argc) {
			return false;
		}
		if (arg == L"--sandbox") {
			options.sandbox = argv[++i];
		} else if (arg == L"--registry") {
			options.registry = argv[++i];
		} else if (arg == L"--desktop") {
			options.desktop = argv[++i];
		} else if (arg == L"--lifetime") {
			// At most a day, which keeps the wait's milliseconds inside a DWORD.
			options.lifetimeSeconds = wcstoul(argv[++i], nullptr, 10);
			if (options.lifetimeSeconds == 0 || options.lifetimeSeconds > 86400) {
				return false;
			}
		} else {
			return false;
		}
	}
	return !options.sandbox.empty() && !options.registry.empty() && !options.desktop.empty();
}

BOOL WINAPI OnConsoleEvent(DWORD) {
	if (g_job) {
		TerminateJobObject(g_job, ERROR_PROCESS_ABORTED);
	}
	return FALSE;
}

// Queues LoadLibraryW(shim) on the suspended main thread. The thread runs it
// when the loader first waits alertably, which is after the game's static DLLs
// are initialised and before Game.exe's entry point.
bool QueueShim(const PROCESS_INFORMATION& process, const std::wstring& shimPath) {
	size_t bytes = (shimPath.size() + 1) * sizeof(wchar_t);
	void* remote = VirtualAllocEx(process.hProcess, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (!remote || !WriteProcessMemory(process.hProcess, remote, shimPath.c_str(), bytes, nullptr)) {
		return false;
	}
	// Kernel32 sits at the same address in every 32-bit process of a session,
	// so this process's LoadLibraryW is the game's.
	auto loadLibrary = reinterpret_cast<PAPCFUNC>(
		GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW"));
	return loadLibrary && QueueUserAPC(loadLibrary, process.hThread, reinterpret_cast<ULONG_PTR>(remote));
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
	Options options;
	if (!Parse(argc, argv, options)) {
		fwprintf(stderr, L"usage: AgentLaunch.exe --sandbox <dir> --registry <HKCU subkey> --desktop <name>"
			L" [--lifetime <seconds>] [-- <game args>]\n");
		return kUsage;
	}

	// Refuses anything the harness did not build, which keeps it off both of
	// the player's installs whatever path it is handed.
	std::wstring game = Join(options.sandbox, L"Game.exe");
	if (!FileExists(Join(options.sandbox, kSandboxMarker)) || !FileExists(game)) {
		return Fail(kNotASandbox, L"not a sandbox: no BHAgent.sandbox or Game.exe");
	}
	std::wstring shim = Join(OwnDirectory(), kShimName);
	if (!FileExists(shim)) {
		return Fail(kNoShim, L"AgentShim.dll is not beside AgentLaunch.exe");
	}
	HKEY key = nullptr;
	if (RegOpenKeyExW(HKEY_CURRENT_USER, options.registry.c_str(), 0, KEY_READ, &key) != ERROR_SUCCESS) {
		return Fail(kNoSandboxKey, L"the sandbox's registry key does not exist");
	}
	RegCloseKey(key);

	wchar_t stopName[64];
	swprintf_s(stopName, L"Local\\BHAgentStop-%lu", GetCurrentProcessId());
	HANDLE stop = CreateEventW(nullptr, TRUE, FALSE, stopName);
	if (!stop) {
		return Fail(kJobFailed, L"cannot create the stop event");
	}

	// Held open until the game exits: a desktop no handle and no thread refers
	// to is destroyed, and the game's threads only attach on their first window
	// call.
	HDESK desktop = CreateDesktopW(options.desktop.c_str(), nullptr, nullptr, 0, GENERIC_ALL, nullptr);
	if (!desktop) {
		return Fail(kDesktopFailed, L"cannot create the desktop");
	}

	g_job = CreateJobObjectW(nullptr, nullptr);
	JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits = {};
	limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
	if (!g_job || !SetInformationJobObject(g_job, JobObjectExtendedLimitInformation, &limits, sizeof(limits))) {
		return Fail(kJobFailed, L"cannot create the job");
	}
	SetConsoleCtrlHandler(OnConsoleEvent, TRUE);

	std::wstring commandLine = L"\"" + game + L"\"";
	for (const std::wstring& arg : options.gameArgs) {
		AppendArgument(commandLine, arg);
	}
	std::wstring desktopPath = L"WinSta0\\" + options.desktop;
	SetEnvironmentVariableW(L"BHAGENT_REGISTRY", options.registry.c_str());

	STARTUPINFOW startup = {sizeof(startup)};
	startup.lpDesktop = desktopPath.data();
	PROCESS_INFORMATION process = {};
	if (!CreateProcessW(game.c_str(), commandLine.data(), nullptr, nullptr, FALSE,
			CREATE_SUSPENDED, nullptr, options.sandbox.c_str(), &startup, &process)) {
		return Fail(kStartFailed, L"cannot start Game.exe");
	}
	if (!AssignProcessToJobObject(g_job, process.hProcess)) {
		TerminateProcess(process.hProcess, kJobFailed);
		return Fail(kJobFailed, L"cannot put Game.exe in the job");
	}

	wchar_t eventName[64];
	swprintf_s(eventName, L"Local\\BHAgentShim-%lu", process.dwProcessId);
	HANDLE ready = CreateEventW(nullptr, TRUE, FALSE, eventName);
	if (!ready || !QueueShim(process, shim)) {
		TerminateProcess(process.hProcess, kInjectFailed);
		return Fail(kInjectFailed, L"cannot queue AgentShim.dll");
	}

	ResumeThread(process.hThread);
	HANDLE waits[] = {ready, process.hProcess};
	DWORD woke = WaitForMultipleObjects(2, waits, FALSE, kShimTimeoutMs);
	if (woke != WAIT_OBJECT_0) {
		DWORD gameExit = 0;
		GetExitCodeProcess(process.hProcess, &gameExit);
		TerminateProcess(process.hProcess, kShimFailed);
		fwprintf(stderr, L"AgentLaunch: AgentShim.dll did not redirect the registry (game exit %#lx)\n", gameExit);
		return kShimFailed;
	}

	wprintf(L"pid=%lu\n", process.dwProcessId);
	fflush(stdout);

	HANDLE ends[] = {process.hProcess, stop};
	DWORD ended = WaitForMultipleObjects(2, ends, FALSE, options.lifetimeSeconds * 1000);
	int result = 0;
	if (ended == WAIT_OBJECT_0) {
		DWORD gameExit = 0;
		GetExitCodeProcess(process.hProcess, &gameExit);
		result = static_cast<int>(gameExit);
	} else {
		result = ended == WAIT_OBJECT_0 + 1 ? kStopped : kLifetimeExpired;
		TerminateJobObject(g_job, result);
		WaitForSingleObject(process.hProcess, INFINITE);
		if (result == kLifetimeExpired) {
			fwprintf(stderr, L"AgentLaunch: lifetime of %lus ran out; the game was ended\n", options.lifetimeSeconds);
		}
	}
	CloseHandle(process.hThread);
	CloseHandle(process.hProcess);
	CloseHandle(ready);
	CloseHandle(stop);
	CloseHandle(g_job);
	CloseDesktop(desktop);
	return result;
}
