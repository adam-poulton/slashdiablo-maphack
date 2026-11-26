#ifdef BH_AGENT

#include "AgentDriver.h"

#include <deque>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "nlohmann/json.hpp"
#include "../../BH.h"
#include "../../D2Ptrs.h"
#include "../../D2Helpers.h"
#include "../../Constants.h"
#include "../../StringUtil.h"
#include "AgentKeys.h"
#include "AgentScreen.h"
#include "DrawLog.h"

using nlohmann::json;

namespace {

// How long the pipe waits for the game's thread to take a request. The thread
// answers once a frame, and draws no frames while a game loads.
constexpr DWORD kAnswerTimeoutMs = 10000;
constexpr size_t kMaxRequestBytes = 64 * 1024;

struct Request {
	json body;
	std::promise<json> reply;
};
typedef std::shared_ptr<Request> RequestPtr;

bool serving = false;
Agent::KeyState keys;

std::mutex incomingLock;
std::deque<RequestPtr> incoming;

// Touched only on the game's thread.
std::vector<RequestPtr> waitingForFrame;
std::vector<RequestPtr> waitingForExit;

json Failure(const std::string& error) {
	return {{"ok", false}, {"error", error}};
}

// --- answers ------------------------------------------------------------------

const char* kClassNames[] = {"amazon", "sorceress", "necromancer", "paladin", "barbarian", "druid", "assassin"};

struct Panel {
	DWORD ui;
	const char* name;
};
const Panel kPanels[] = {
	{UI_INVENTORY, "inventory"}, {UI_CHARACTER, "character"}, {UI_SKILLTREE, "skills"},
	{UI_CHAT_CONSOLE, "chat"}, {UI_ESCMENU_MAIN, "menu"}, {UI_NPCSHOP, "shop"}, {UI_QUEST, "quests"},
	{UI_PARTY, "party"}, {UI_STASH, "stash"}, {UI_CUBE, "cube"}, {UI_MERC, "mercenary"},
};

json GameState() {
	if (!IsGameReady())
		return {{"ok", true}, {"screen", "loading"}};

	UnitAny* player = D2CLIENT_GetPlayerUnit();
	DWORD classId = player->dwTxtFileNo;
	DWORD area = player->pPath->pRoom1->pRoom2->pLevel->dwLevelNo;
	json game = {
		{"name", std::string(player->pPlayerData->szName)},
		{"class", classId < 7 ? kClassNames[classId] : "unknown"},
		{"level", D2COMMON_GetUnitStat(player, STAT_LEVEL, 0)},
		{"difficulty", (int)D2CLIENT_GetDifficulty()},
		{"area", area},
		{"position", {player->pPath->xPos, player->pPath->yPos}},
	};
	json panels = json::array();
	for (const Panel& panel : kPanels) {
		if (D2CLIENT_GetUIState(panel.ui))
			panels.push_back(panel.name);
	}
	game["panels"] = panels;
	// Last, and with nothing read from the player after it: the call leaves the
	// player unit pointer stale (see ScreenInfo::OnAutomapDraw).
	if (const wchar_t* name = D2CLIENT_GetLevelName(area))
		game["areaName"] = ToUtf8(name);

	return {
		{"ok", true},
		{"screen", "game"},
		{"size", {*p_D2CLIENT_ScreenSizeX, *p_D2CLIENT_ScreenSizeY}},
		{"game", game},
	};
}

json MenuState() {
	std::vector<Agent::ControlShape> controls = Agent::ShapesOf(*p_D2WIN_FirstControl);
	json listed = json::array();
	for (const Agent::ControlShape& control : controls)
		listed.push_back(Agent::ToJson(control));
	return {{"ok", true}, {"screen", Agent::NameScreen(controls)}, {"controls", listed}};
}

json FrameRecord(bool inGame) {
	json items = json::array();
	for (const Agent::DrawRecord& record : Agent::DrawLog::End())
		items.push_back(Agent::ToJson(record));
	return {{"ok", true}, {"screen", inGame ? "game" : "menu"}, {"items", items}};
}

// Answers what can be answered now and parks what waits on a later frame or
// the game loop. On the game's thread.
void Dispatch(const RequestPtr& request, bool inGame) {
	std::string command = request->body.value("cmd", "");
	if (command == "state") {
		request->reply.set_value(inGame ? GameState() : MenuState());
	} else if (command == "drawlog") {
		waitingForFrame.push_back(request);
	} else if (command == "exit") {
		if (inGame)
			waitingForExit.push_back(request);
		else
			request->reply.set_value(Failure("not in a game"));
	} else {
		request->reply.set_value(Failure("unknown command '" + command + "'"));
	}
}

// --- the pipe -----------------------------------------------------------------

json Answer(const std::string& line) {
	json body;
	try {
		body = json::parse(line);
	} catch (const json::exception&) {
		return Failure("not a line of JSON");
	}
	if (!body.is_object() || !body.contains("cmd"))
		return Failure("a request is an object with a \"cmd\"");
	if (body["cmd"] == "ping")
		return {{"ok", true}, {"pid", GetCurrentProcessId()}};

	RequestPtr request = std::make_shared<Request>();
	request->body = body;
	std::future<json> reply = request->reply.get_future();
	{
		std::lock_guard<std::mutex> lock(incomingLock);
		incoming.push_back(request);
	}
	if (reply.wait_for(std::chrono::milliseconds(kAnswerTimeoutMs)) != std::future_status::ready)
		return Failure("the game did not take the request: it draws no frames while a game loads");
	return reply.get();
}

bool ReadLine(HANDLE pipe, std::string& line) {
	char buffer[4096];
	while (line.find('\n') == std::string::npos) {
		DWORD read = 0;
		if (!ReadFile(pipe, buffer, sizeof(buffer), &read, NULL) || read == 0)
			return false;
		line.append(buffer, read);
		if (line.size() > kMaxRequestBytes)
			return false;
	}
	line.erase(line.find('\n'));
	return true;
}

void Serve() {
	std::string name = "\\\\.\\pipe\\bhagent-" + std::to_string(GetCurrentProcessId());
	while (true) {
		HANDLE pipe = CreateNamedPipeA(name.c_str(), PIPE_ACCESS_DUPLEX,
			PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
			1, 64 * 1024, 64 * 1024, 0, NULL);
		if (pipe == INVALID_HANDLE_VALUE) {
			Sleep(1000);
			continue;
		}
		if (ConnectNamedPipe(pipe, NULL) || GetLastError() == ERROR_PIPE_CONNECTED) {
			std::string line;
			if (ReadLine(pipe, line)) {
				std::string reply = Answer(line).dump() + "\n";
				DWORD written = 0;
				WriteFile(pipe, reply.data(), (DWORD)reply.size(), &written, NULL);
				FlushFileBuffers(pipe);
			}
			DisconnectNamedPipe(pipe);
		}
		CloseHandle(pipe);
	}
}

// --- the keyboard ---------------------------------------------------------------

SHORT WINAPI AgentGetKeyState(int virtualKey) {
	return keys.KeyStateOf(virtualKey);
}

SHORT WINAPI AgentGetAsyncKeyState(int virtualKey) {
	return keys.AsyncKeyStateOf(virtualKey);
}

BOOL WINAPI AgentGetKeyboardState(PBYTE state) {
	if (!state)
		return FALSE;
	keys.Fill(state);
	return TRUE;
}

// What TranslateMessage does for a key going down, with the posted keys as the
// keyboard: posts the character it types, which Windows would make from a
// keyboard state the posted keys never reach, losing Shift. The message loops
// run this before the window is given the key, which is fine: what it reads is
// the keys already down, never the one being pressed.
BOOL WINAPI AgentTranslateMessage(const MSG* message) {
	if (!message || (message->message != WM_KEYDOWN && message->message != WM_SYSKEYDOWN))
		return FALSE;
	BYTE state[256];
	keys.Fill(state);
	WORD typed = 0;
	UINT scanCode = (UINT)(message->lParam >> 16) & 0xFF;
	if (ToAscii((UINT)message->wParam, scanCode, state, &typed, 0) != 1)
		return FALSE;
	UINT kind = message->message == WM_SYSKEYDOWN ? WM_SYSCHAR : WM_CHAR;
	PostMessageA(message->hwnd, kind, typed & 0xFF, message->lParam);
	return TRUE;
}

// Points the module's imports of a user32 function at a replacement. Matched by
// address rather than by name, because the game's DLLs were linked with their
// imports bound, and a bound import table keeps no names in the slots it binds.
void RedirectImport(HMODULE module, const char* function, void* replacement) {
	FARPROC original = GetProcAddress(GetModuleHandleA("user32.dll"), function);
	BYTE* base = (BYTE*)module;
	IMAGE_NT_HEADERS* headers = (IMAGE_NT_HEADERS*)(base + ((IMAGE_DOS_HEADER*)base)->e_lfanew);
	IMAGE_DATA_DIRECTORY imports = headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
	if (!original || !imports.VirtualAddress)
		return;
	for (IMAGE_IMPORT_DESCRIPTOR* dll = (IMAGE_IMPORT_DESCRIPTOR*)(base + imports.VirtualAddress); dll->Name; dll++) {
		if (_stricmp((const char*)(base + dll->Name), "user32.dll") != 0)
			continue;
		for (IMAGE_THUNK_DATA* slot = (IMAGE_THUNK_DATA*)(base + dll->FirstThunk); slot->u1.Function; slot++) {
			if (slot->u1.Function != (DWORD)original)
				continue;
			DWORD protection;
			VirtualProtect(&slot->u1.Function, sizeof(DWORD), PAGE_READWRITE, &protection);
			slot->u1.Function = (DWORD)replacement;
			VirtualProtect(&slot->u1.Function, sizeof(DWORD), protection, &protection);
		}
	}
}

// The modules that read the keyboard or translate keys into characters: every
// import of these from user32 in the game's DLLs and in BH, the renderers'
// own message loops aside, which a sandbox never loads.
void AnswerKeyboardQueries() {
	HMODULE modules[] = {
		LoadLibraryA("D2Client.dll"), LoadLibraryA("D2Win.dll"), LoadLibraryA("D2Launch.dll"),
		LoadLibraryA("Fog.dll"), LoadLibraryA("Storm.dll"), BH::instance,
	};
	for (HMODULE module : modules) {
		if (!module)
			continue;
		RedirectImport(module, "GetKeyState", (void*)AgentGetKeyState);
		RedirectImport(module, "GetAsyncKeyState", (void*)AgentGetAsyncKeyState);
		RedirectImport(module, "GetKeyboardState", (void*)AgentGetKeyboardState);
		RedirectImport(module, "TranslateMessage", (void*)AgentTranslateMessage);
	}
}

}  // namespace

void AgentDriver::OnLoad() {
	if (serving || !GetEnvironmentVariableA("BHAGENT_REGISTRY", NULL, 0))
		return;
	serving = true;
	AnswerKeyboardQueries();
	std::thread(Serve).detach();
}

void AgentDriver::OnLoop() {
	if (!serving || waitingForExit.empty())
		return;
	D2CLIENT_ExitGame();
	for (const RequestPtr& request : waitingForExit)
		request->reply.set_value({{"ok", true}});
	waitingForExit.clear();
}

void AgentDriver::FrameEnd(bool inGame) {
	if (!serving)
		return;
	if (Agent::DrawLog::Recording()) {
		json frame = FrameRecord(inGame);
		for (const RequestPtr& request : waitingForFrame)
			request->reply.set_value(frame);
		waitingForFrame.clear();
	}

	std::deque<RequestPtr> taken;
	{
		std::lock_guard<std::mutex> lock(incomingLock);
		taken.swap(incoming);
	}
	for (const RequestPtr& request : taken)
		Dispatch(request, inGame);

	if (!waitingForFrame.empty())
		Agent::DrawLog::Begin();
}

void AgentDriver::ObserveMessage(UINT message, WPARAM wParam) {
	if (serving)
		keys.Observe(message, (unsigned int)wParam);
}

// --- what BH draws ------------------------------------------------------------
//
// The names BH calls to draw, standing in for the game's in the agent build
// (D2Ptrs.h): each records the call and makes it.

void __stdcall D2GFX_DrawLine(int X1, int Y1, int X2, int Y2, DWORD dwColor, DWORD dwUnk) {
	Agent::DrawLog::Line(X1, Y1, X2, Y2, dwColor);
	D2GFX_DrawLine_Game(X1, Y1, X2, Y2, dwColor, dwUnk);
}

void __stdcall D2GFX_DrawRectangle(int X1, int Y1, int X2, int Y2, DWORD dwColor, DWORD dwTrans) {
	Agent::DrawLog::Box(X1, Y1, X2, Y2, dwColor, (int)dwTrans);
	D2GFX_DrawRectangle_Game(X1, Y1, X2, Y2, dwColor, dwTrans);
}

void __fastcall D2WIN_DrawText(const wchar_t* wStr, int xPos, int yPos, DWORD dwColor, DWORD dwUnk) {
	if (Agent::DrawLog::Recording() && wStr) {
		// The font in use is only to be had by setting another and taking back
		// what the call returns.
		DWORD font = D2WIN_SetTextSize(0);
		D2WIN_SetTextSize(font);
		DWORD width = 0, fileNo = 0;
		D2WIN_GetTextWidthFileNo(const_cast<wchar_t*>(wStr), &width, &fileNo);
		Agent::DrawLog::Text(xPos, yPos, (int)width, (int)font, dwColor, wStr);
	}
	D2WIN_DrawText_Game(wStr, xPos, yPos, dwColor, dwUnk);
}

#endif
