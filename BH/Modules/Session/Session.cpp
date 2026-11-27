#include "Session.h"
#include <ctime>
#include <filesystem>
#include "../../BH.h"
#include "../../Common.h"
#include "../../Constants.h"
#include "../ScreenInfo/ScreenInfo.h"
#include "../Settings/SettingsRegistry.h"
#include "SessionFormat.h"

using namespace SessionEvents;

namespace {
	// Where the logs are kept, beside the settings.
	const char* const kFolder = "BH_sessions\\";

	// How often the tokens are rewritten. The fastest of them is a clock that
	// shows seconds.
	const unsigned long long kPublishEveryMs = 250;

	// How far back "recent" looks.
	const unsigned long long kRecentWindowMs = 60 * 1000;

	// The tokens this module keeps, for AutomapInfo lines and run tracker
	// columns. Named apart from the run tracker's own, none of which they
	// replace.
	const char* const kTokens[] = {
		"SESSIONTIME", "SESSIONPLAYTIME", "SESSIONXPGAINED", "SESSIONXPPERSEC",
		"SESSIONXPPERSECLONG", "RECENTXPPERSEC", "SESSIONLEVELS", "SESSIONDEATHS",
		"SESSIONKILLS", "SESSIONDROPS",
	};

	const char* const kHeading = "Session";

	std::string KeyFor(const GameProbe::Identity& who) {
		return who.account + "\n" + who.realm + "\n" + who.character;
	}

	// A log's path, with a number after its name where that name is taken: two
	// sessions of one character can begin in the same second.
	std::string FreePath(const std::string& folder, const std::string& name) {
		const size_t dot = name.find_last_of('.');
		std::string path = folder + name;
		for (unsigned int n = 2; std::filesystem::exists(path) && n < 100; n++)
			path = folder + name.substr(0, dot) + "-" + std::to_string(n) + name.substr(dot);
		return path;
	}
}

Session* Session::instance = NULL;
bool Session::enabled = true;
bool Session::keepHistory = true;
bool Session::hudOn = true;
unsigned int Session::hudView = HudLayout::ViewLine;
unsigned int Session::hudBackground = SessionHud::BackgroundTranslucent;
unsigned int Session::cycleViewKey = 0;
unsigned int Session::placeHudKey = 0;
unsigned int Session::newSessionKey = 0;

Session::Session() : Module("Session Tracker"), current(NULL), wantsNew(false),
		wantsPlacing(false), wantsViewStep(0), wantsView(-1), screenInfo(NULL),
		publishedAt(0) {
	instance = this;
}

void Session::OnLoad() {
	LoadConfig();
	screenInfo = (ScreenInfo*)BH::moduleManager->Get("Screen Info");
	ClearTokens();
	hud.Load(&hudView);

	Settings::AddHeading(GetName(), Settings::Category::Display, kHeading);
	Settings::AddBool(GetName(), Settings::Category::Display, "Session Tracker",
		"Session tracker", &enabled,
		"Keeps track of a sitting on a character: time played, games, experience "
		"and how fast it is coming, and deaths.");
	Settings::AddBool(GetName(), Settings::Category::Display, "Keep Session History",
		"Keep session history", &keepHistory,
		"Writes each session to a file of its own in BH_sessions, beside the settings.",
		"Session Tracker");
	Settings::AddBool(GetName(), Settings::Category::Display, "Session HUD",
		"Session HUD", &hudOn,
		"Shows the session's figures over the game. It takes no clicks: type "
		".hud move, or use Position HUD, to place it.",
		"Session Tracker");
	std::vector<std::string> views;
	for (int view = 0; view < HudLayout::ViewCount; view++)
		views.push_back(HudLayout::NameOf((HudLayout::View)view));
	Settings::AddEnum(GetName(), Settings::Category::Display, "Session HUD View",
		"HUD view", &hudView, views,
		"How much the HUD shows, from one line upwards.", "Session HUD");
	Settings::AddEnum(GetName(), Settings::Category::Display, "Session HUD Background",
		"HUD background", &hudBackground, { "None", "Translucent", "Solid" },
		"What the HUD's text is drawn on.", "Session HUD");

	Settings::AddHeading(GetName(), Settings::Category::Input, kHeading);
	Settings::AddKey(GetName(), Settings::Category::Input, "Session HUD Cycle View",
		"Cycle HUD view", &cycleViewKey, "Steps the session HUD on to its next view.");
	Settings::AddKey(GetName(), Settings::Category::Input, "Session HUD Position",
		"Position HUD", &placeHudKey,
		"Lets the session HUD be dragged to where you want it, and again to finish.");
	Settings::AddKey(GetName(), Settings::Category::Input, "Session New",
		"New session", &newSessionKey,
		"Ends the session and starts another from the game you are in.");
}

void Session::LoadConfig() {
	BH::config->ReadBoolean("Session Tracker", enabled);
	BH::config->ReadBoolean("Keep Session History", keepHistory);
	BH::config->ReadBoolean("Session HUD", hudOn);
	BH::config->ReadInt("Session HUD View", hudView, HudLayout::ViewLine);
	if (hudView >= HudLayout::ViewCount)
		hudView = HudLayout::ViewLine;
	BH::config->ReadInt("Session HUD Background", hudBackground,
		SessionHud::BackgroundTranslucent);
	if (hudBackground > SessionHud::BackgroundSolid)
		hudBackground = SessionHud::BackgroundTranslucent;
	BH::config->ReadKey("Session HUD Cycle View", "None", cycleViewKey);
	BH::config->ReadKey("Session HUD Position", "None", placeHudKey);
	BH::config->ReadKey("Session New", "None", newSessionKey);
}

void Session::OnSettingsChanged(const vector<string>& keys) {
	std::lock_guard<std::mutex> held(lock);
	if (!enabled) {
		// A tracker switched off mid-game has seen the last of that game.
		std::vector<GameProbe::Seen> seen;
		probe.Leave(BHGetTickCount(), seen);
		Take(seen);
		ClearTokens();
		hud.SetBeingPlaced(false);
	}
	if (current)
		Write(*current);
}

void Session::OnLoop() {
	if (!enabled)
		return;

	const unsigned long long tick = BHGetTickCount();
	std::lock_guard<std::mutex> held(lock);
	ActOnRequests();
	std::vector<GameProbe::Seen> seen;
	probe.Sample(tick, seen);
	Take(seen);
	TakeDrops(tick);
	PublishTokens(tick);
}

void Session::OnDraw() {
	if (!enabled || (!hudOn && !hud.IsBeingPlaced()))
		return;

	std::lock_guard<std::mutex> held(lock);
	if (current)
		hud.Draw(current->model, NowOf(*current, BHGetTickCount()), hudBackground);
}

void Session::OnGameExit() {
	std::lock_guard<std::mutex> held(lock);
	std::vector<GameProbe::Seen> seen;
	probe.Leave(BHGetTickCount(), seen);
	Take(seen);
	hud.SetBeingPlaced(false);
	{
		std::lock_guard<std::mutex> dropsHeld(dropLock);
		notedDrops.clear();
	}
}

void Session::OnKey(bool up, BYTE key, LPARAM lParam, bool* block) {
	if (!enabled || key == 0)
		return;
	if (key != cycleViewKey && key != placeHudKey && key != newSessionKey)
		return;

	*block = true;
	if (up)
		return;
	if (key == cycleViewKey)
		wantsViewStep++;
	else if (key == placeHudKey)
		wantsPlacing = true;
	else
		wantsNew = true;
}

void Session::ActOnRequests() {
	if (wantsNew.exchange(false))
		StartNew();

	const int view = wantsView.exchange(-1);
	if (view >= 0 && view < HudLayout::ViewCount)
		hudView = (unsigned int)view;
	const int steps = wantsViewStep.exchange(0);
	if (steps != 0)
		hudView = HudLayout::Next((HudLayout::View)hudView, steps);

	if (wantsPlacing.exchange(false)) {
		hud.SetBeingPlaced(!hud.IsBeingPlaced());
		// A HUD is placed to be looked at.
		if (hud.IsBeingPlaced())
			hudOn = true;
	}
}

void Session::StartNew() {
	std::vector<GameProbe::Seen> seen;
	probe.Leave(BHGetTickCount(), seen);
	Take(seen);

	for (auto held = sessions.begin(); held != sessions.end(); ++held) {
		if (held->second.get() == current) {
			sessions.erase(held);
			break;
		}
	}
	if (shown.get() == current)
		shown.reset();
	current = NULL;
	ClearTokens();
}

void Session::Take(const std::vector<GameProbe::Seen>& seen) {
	if (seen.empty())
		return;

	for (const GameProbe::Seen& one : seen) {
		if (std::holds_alternative<Joined>(one.fact)) {
			current = SessionFor(probe.GetIdentity(), one.tick);
			takenDrops.clear();
		}
		if (!current || current->shown)
			continue;
		Record(*current, { NowOf(*current, one.tick), one.fact });
	}
	if (current)
		Write(*current);
}

void Session::TakeDrops(unsigned long long tick) {
	// Kept until there is a game to put them in: the first of a game's drops
	// can be pinged before the player's stats have appeared.
	if (!current || current->shown || !current->model.InGame())
		return;
	std::vector<DropNote> noted;
	{
		std::lock_guard<std::mutex> held(dropLock);
		noted.swap(notedDrops);
	}
	if (noted.empty())
		return;

	for (const DropNote& note : noted) {
		if (!takenDrops.insert(note.id).second)
			continue;
		Dropped fact;
		fact.id = note.id;
		fact.name = SessionFormat::Plain(note.name);
		fact.code = note.code;
		fact.quality = note.quality;
		fact.color = note.color;
		fact.ping = note.ping;
		fact.area = note.area;
		// The area a drop lands in is the item's, and need not be one the
		// character has reached, so its name is asked for here.
		fact.areaName = note.area ? GameProbe::AreaName(note.area) : std::string();
		Record(*current, { NowOf(*current, tick), fact });
	}
	Write(*current);
}

Session::Live* Session::SessionFor(const GameProbe::Identity& who,
		unsigned long long tick) {
	std::unique_ptr<Live>& held = sessions[KeyFor(who)];
	if (held)
		return held.get();

	held.reset(new Live());
	held->beganAt = tick;
	held->written = 0;
	held->unwritable = false;
	held->shown = false;
	held->shownAt = 0;

	Started fact;
	// The wall clock at the tick the session's clock starts from, which is a
	// moment ago by the time the player's stats have appeared.
	fact.startedAt = (long long)time(NULL) - (long long)((BHGetTickCount() - tick) / 1000);
	fact.account = who.account;
	fact.realm = who.realm;
	fact.character = who.character;
	fact.classId = who.classId;
	fact.hardcore = who.hardcore;
	fact.expansion = who.expansion;
	fact.version = VERSION;
	Record(*held, { 0, fact });
	return held.get();
}

void Session::Record(Live& session, const Event& event) {
	session.model.Apply(event);
	session.log.push_back(event);
}

unsigned long long Session::NowOf(const Live& session, unsigned long long tick) const {
	if (session.shown)
		return session.shownAt;
	return tick > session.beganAt ? tick - session.beganAt : 0;
}

void Session::Write(Live& session) {
	if (!keepHistory || session.shown || session.unwritable ||
			session.written == session.log.size())
		return;

	if (!session.file.IsOpen()) {
		const Started& header = session.model.Header();
		const time_t began = (time_t)header.startedAt;
		std::tm local = {};
		localtime_s(&local, &began);
		const std::string path = FreePath(BH::path + kFolder,
			SessionFile::NameFor(local, header.character, GetCurrentProcessId()));
		if (!session.file.Open(path)) {
			session.unwritable = true;
			return;
		}
	}

	for (; session.written < session.log.size(); session.written++)
		session.file.Append(session.log[session.written]);
	session.file.Flush();
}

void Session::PublishTokens(unsigned long long tick) {
	if (!screenInfo || !current || tick - publishedAt < kPublishEveryMs)
		return;
	publishedAt = tick;

	const SessionModel& model = current->model;
	const unsigned long long now = NowOf(*current, tick);
	const long long net = (long long)model.Gained() - (long long)model.Lost();

	screenInfo->SetAutomapToken("SESSIONTIME", SessionFormat::Clock(now));
	screenInfo->SetAutomapToken("SESSIONPLAYTIME", SessionFormat::Clock(model.PlayedMs(now)));
	screenInfo->SetAutomapToken("SESSIONXPGAINED", std::to_string(net));
	screenInfo->SetAutomapToken("SESSIONXPPERSEC", SessionFormat::Rate(model.PlayedRate(now)));
	screenInfo->SetAutomapToken("SESSIONXPPERSECLONG", std::to_string(model.PlayedRate(now)));
	screenInfo->SetAutomapToken("RECENTXPPERSEC",
		SessionFormat::Rate(model.RecentRate(now, kRecentWindowMs)));
	screenInfo->SetAutomapToken("SESSIONLEVELS", std::to_string(model.LevelsGained()));
	screenInfo->SetAutomapToken("SESSIONDEATHS", std::to_string(model.Deaths()));
	screenInfo->SetAutomapToken("SESSIONKILLS", std::to_string(model.KillTotals().Total()));
	screenInfo->SetAutomapToken("SESSIONDROPS", std::to_string(model.Drops().size()));
}

// A token with nothing in it is written as nothing, where one nobody has set is
// left on screen as its own name.
void Session::ClearTokens() {
	if (!screenInfo)
		return;
	for (const char* token : kTokens)
		screenInfo->SetAutomapToken(token, "");
	publishedAt = 0;
}

bool Session::Read(const std::function<void(const SessionModel&, unsigned long long)>& reader) {
	Session* self = instance;
	if (!self)
		return false;
	std::lock_guard<std::mutex> held(self->lock);
	if (!self->current)
		return false;
	reader(self->current->model, self->NowOf(*self->current, BHGetTickCount()));
	return true;
}

void Session::NoteDrop(const DropNote& drop) {
	Session* self = instance;
	if (!self || !enabled)
		return;
	std::lock_guard<std::mutex> held(self->dropLock);
	self->notedDrops.push_back(drop);
}

void Session::RequestNew() {
	if (instance)
		instance->wantsNew = true;
}

void Session::RequestHudView(HudLayout::View view) {
	if (instance)
		instance->wantsView = (int)view;
}

void Session::RequestHudPlacing(bool placing) {
	if (instance && instance->hud.IsBeingPlaced() != placing)
		instance->wantsPlacing = true;
}

#ifdef BH_AGENT
void Session::Show(const std::vector<Event>& events) {
	Session* self = instance;
	if (!self)
		return;
	std::lock_guard<std::mutex> held(self->lock);

	self->shown.reset(new Live());
	Live& log = *self->shown;
	log.beganAt = 0;
	log.written = 0;
	log.unwritable = false;
	log.shown = true;
	log.shownAt = events.empty() ? 0 : events.back().at;
	for (const Event& event : events)
		log.model.Apply(event);
	self->current = &log;
	self->publishedAt = 0;
}
#endif
