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
#include "../../Drawing/Hook.h"
#include "AgentKeys.h"
#include "AgentScreen.h"
#include "DrawLog.h"
#include "FrameCapture.h"
#include "ImportRedirect.h"
#include "ItemDrop.h"

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

// A recorded item packet rewritten to land near the character, waiting for the
// game loop, which is where the client takes in what the server sends.
struct Drop {
	RequestPtr request;
	std::vector<unsigned char> packet;
	DWORD id;
	unsigned int x, y;
};

// Far above any id the server gives a unit in a game, so a dropped item is never
// taken for one already in the world.
constexpr DWORD kFirstDropId = 0x7F000000;

bool serving = false;
bool rendererAnswered = false;
Agent::KeyState keys;

std::mutex incomingLock;
std::deque<RequestPtr> incoming;

// Touched only on the game's thread.
std::vector<RequestPtr> waitingForFrame;
std::vector<RequestPtr> waitingForExit;
std::vector<Drop> waitingForLoop;
DWORD nextDropId = kFirstDropId;

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

// The items on the ground in the rooms the client has loaded, named as the game
// names them on screen, which is after BH's item filter has had its say.
json GroundItems(UnitAny* player) {
	json items = json::array();
	for (Room1* room = player->pAct->pRoom1; room; room = room->pRoomNext) {
		for (UnitAny* unit = room->pUnitFirst; unit; unit = unit->pListNext) {
			if (unit->dwType != UNIT_ITEM || !unit->pItemPath ||
					(unit->dwMode != ITEM_MODE_ON_GROUND && unit->dwMode != ITEM_MODE_BEING_DROPPED))
				continue;
			wchar_t name[256] = L"";
			D2CLIENT_GetItemName(unit, name, 256);
			items.push_back({
				{"id", unit->dwUnitId},
				{"code", GetItemCode(unit->dwTxtFileNo)},
				{"position", {unit->pItemPath->dwPosX, unit->pItemPath->dwPosY}},
				{"name", ToUtf8(name)},
			});
		}
	}
	return items;
}

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
	game["items"] = GroundItems(player);
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
	return {
		{"ok", true},
		{"screen", Agent::NameScreen(controls)},
		{"size", {Drawing::Hook::GetScreenWidth(), Drawing::Hook::GetScreenHeight()}},
		{"controls", listed},
	};
}

// Asks the renderer for {"path": FILE}'s frame, or says why there will be none.
void RequestFrame(const RequestPtr& request) {
	const json& body = request->body;
	if (!body.contains("path") || !body["path"].is_string()) {
		request->reply.set_value(Failure("a shot's \"path\" is the file to write the frame to"));
		return;
	}
	if (!Agent::FrameCapture::Presenting()) {
		request->reply.set_value(Failure("the game presents nothing through OpenGL: it is not rendered by D2GL"));
		return;
	}
	Agent::FrameCapture::Request(body["path"].get<std::string>(), [request](const Agent::CapturedFrame& frame) {
		if (frame.error.empty())
			request->reply.set_value({{"ok", true}, {"size", {frame.width, frame.height}}});
		else
			request->reply.set_value(Failure(frame.error));
	});
}

json FrameRecord(bool inGame) {
	json items = json::array();
	for (const Agent::DrawRecord& record : Agent::DrawLog::End())
		items.push_back(Agent::ToJson(record));
	return {{"ok", true}, {"screen", inGame ? "game" : "menu"}, {"items", items}};
}

// Readies {"packet": HEX, "offset": [DX, DY]} to land that far from the
// character, or says why it cannot.
json PrepareDrop(const RequestPtr& request, Drop& drop) {
	const json& body = request->body;
	if (!body.contains("packet") || !body["packet"].is_string() ||
			!Agent::BytesFromHex(body["packet"].get<std::string>(), drop.packet))
		return Failure("a drop's \"packet\" is the item packet as hex");
	json offset = body.value("offset", json::array({0, 0}));
	if (!offset.is_array() || offset.size() != 2 || !offset[0].is_number_integer() ||
			!offset[1].is_number_integer())
		return Failure("a drop's \"offset\" is [dx, dy] from the character");

	UnitAny* player = D2CLIENT_GetPlayerUnit();
	drop.request = request;
	drop.id = nextDropId++;
	drop.x = (unsigned int)((int)player->pPath->xPos + offset[0].get<int>());
	drop.y = (unsigned int)((int)player->pPath->yPos + offset[1].get<int>());
	std::string wrong = Agent::PlaceDrop(drop.packet, drop.id, drop.x, drop.y);
	if (!wrong.empty())
		return Failure("cannot drop the packet: " + wrong);
	return json();
}

// Hands a packet to the client as though the server had sent it, through the
// same call the game loop makes for each batch it receives, BH's own packet
// hook included.
void ReceiveFromServer(std::vector<unsigned char>& packet) {
	BYTE* bytes = packet.data();
	DWORD byteCount = (DWORD)packet.size();
	DWORD receive = D2CLIENT_ReceivePackets_I;
	__asm {
		mov eax, bytes
		push byteCount
		call receive
	}
}

// Answers what can be answered now and parks what waits on a later frame or
// the game loop. On the game's thread.
void Dispatch(const RequestPtr& request, bool inGame) {
	std::string command = request->body.value("cmd", "");
	if (command == "state") {
		request->reply.set_value(inGame ? GameState() : MenuState());
	} else if (command == "drawlog") {
		waitingForFrame.push_back(request);
	} else if (command == "shot") {
		RequestFrame(request);
	} else if (command == "exit" || command == "drop") {
		if (!inGame || !IsGameReady()) {
			request->reply.set_value(Failure("not in a game"));
		} else if (command == "exit") {
			waitingForExit.push_back(request);
		} else {
			Drop drop;
			json failure = PrepareDrop(request, drop);
			if (failure.is_null())
				waitingForLoop.push_back(std::move(drop));
			else
				request->reply.set_value(failure);
		}
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

void AnswerKeyboardQueries(HMODULE module) {
	Agent::RedirectImport(module, "user32.dll", "GetKeyState", (void*)AgentGetKeyState);
	Agent::RedirectImport(module, "user32.dll", "GetAsyncKeyState", (void*)AgentGetAsyncKeyState);
	Agent::RedirectImport(module, "user32.dll", "GetKeyboardState", (void*)AgentGetKeyboardState);
	Agent::RedirectImport(module, "user32.dll", "TranslateMessage", (void*)AgentTranslateMessage);
}

// The modules that read the keyboard or translate keys into characters: every
// import of these from user32 in the game's DLLs and in BH. The renderer is
// answered apart, once it is there (AnswerRenderer).
void AnswerKeyboardQueries() {
	HMODULE modules[] = {
		LoadLibraryA("D2Client.dll"), LoadLibraryA("D2Win.dll"), LoadLibraryA("D2Launch.dll"),
		LoadLibraryA("Fog.dll"), LoadLibraryA("Storm.dll"), BH::instance,
	};
	for (HMODULE module : modules) {
		if (module)
			AnswerKeyboardQueries(module);
	}
}

// The Glide wrapper, which is the renderer when the game runs -3dfx: D2GL reads
// the keyboard for its own shortcuts, and presents the frames a shot is read
// from. False until the game has loaded it, which it has by the first frame of
// any game it renders.
bool AnswerRenderer() {
	HMODULE renderer = GetModuleHandleA("glide3x.dll");
	if (!renderer)
		return false;
	AnswerKeyboardQueries(renderer);
	Agent::FrameCapture::Install(renderer);
	return true;
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
	if (!serving)
		return;
	// Dropped first, so a drop asked for with an exit lands before the game ends.
	for (Drop& drop : waitingForLoop) {
		ReceiveFromServer(drop.packet);
		// The item filter hides an item by stopping its packet, and then there is
		// no unit.
		bool landed = D2CLIENT_FindServerSideUnit(drop.id, UNIT_ITEM) != NULL;
		drop.request->reply.set_value({{"ok", true}, {"id", drop.id},
			{"position", {drop.x, drop.y}}, {"landed", landed}});
	}
	waitingForLoop.clear();

	if (waitingForExit.empty())
		return;
	D2CLIENT_ExitGame();
	for (const RequestPtr& request : waitingForExit)
		request->reply.set_value({{"ok", true}});
	waitingForExit.clear();
}

void AgentDriver::FrameEnd(bool inGame) {
	if (!serving)
		return;
	if (!rendererAnswered)
		rendererAnswered = AnswerRenderer();
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

static void __stdcall RecordFrame(const RECT* rect) {
	Agent::DrawLog::Frame(rect->left, rect->top, rect->right, rect->bottom);
}

// The game's frame takes its RECT in eax, so this leaves every register as it
// found it for the jump.
__declspec(naked) void __fastcall D2CLIENT_DrawRectFrame(DWORD) {
	__asm {
		pushad
		push eax
		call RecordFrame
		popad
		jmp D2CLIENT_DrawRectFrame_Game
	}
}

#endif
