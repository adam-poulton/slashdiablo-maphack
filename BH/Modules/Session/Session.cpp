#include "Session.h"
#include <ctime>
#include <filesystem>
#include "../../BH.h"
#include "../../Common.h"
#include "../../Constants.h"
#include "../ScreenInfo/ScreenInfo.h"
#include "../Settings/SettingsRegistry.h"
#include "../Tracking/PlayFormat.h"

using namespace PlayEvents;

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

	// Where the HUD's place is kept, and where it goes until it is moved: the
	// top of the screen, in the middle.
	const char* const kHudSection = "SessionHud";
	const HudBlock::Place kHudPlace = { HudBlock::SideMiddle, HudBlock::SideNear, 0, 3 };

	const char* const kHeading = "Session";

	// How many logs are kept where nobody has said, and the most that can be
	// asked for.
	const unsigned int kDefaultKept = 200;
	const unsigned int kMostKept = 9999;

	// Logs not yet in the index are read a few at a time, each time the list of
	// sessions is asked for, and the folder is looked over again this often
	// once they all are.
	const unsigned int kLogsReadAtOnce = 4;
	const unsigned long long kHistoryCheckEveryMs = 2000;

	std::string FileNameOf(const std::string& path) {
		const size_t slash = path.find_last_of("\\/");
		return slash == std::string::npos ? path : path.substr(slash + 1);
	}

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
unsigned int Session::sessionsKept = kDefaultKept;
bool Session::hudOn = true;
unsigned int Session::hudView = HudLayout::ViewLine;
unsigned int Session::hudBackground = PlacedHud::BackgroundTranslucent;
unsigned int Session::cycleViewKey = 0;
unsigned int Session::placeHudKey = 0;
unsigned int Session::newSessionKey = 0;

Session::Session() : Module("Session Tracker"), current(NULL), hud(kHudSection, kHudPlace),
		historyCheckedAt(0), historyPending(0), wantsNew(false), wantsPlacing(false),
		wantsViewStep(0), wantsView(-1), screenInfo(NULL), publishedAt(0) {
	instance = this;
}

void Session::OnLoad() {
	LoadConfig();
	screenInfo = (ScreenInfo*)BH::moduleManager->Get("Screen Info");
	ClearTokens();
	hud.Load();
	history.reset(new SessionHistory(BH::path + kFolder));
	history->Load();
	Prune();

	Settings::AddHeading(GetName(), Settings::Category::Display, kHeading);
	Settings::AddBool(GetName(), Settings::Category::Display, "Session Tracker",
		"Session tracker", &enabled,
		"Keeps track of a sitting on a character: time played, games, experience "
		"and how fast it is coming, and deaths.");
	Settings::AddBool(GetName(), Settings::Category::Display, "Keep Session History",
		"Keep session history", &keepHistory,
		"Writes each session to a file of its own in BH_sessions, beside the settings. "
		"The History tab of the session window lists them.",
		"Session Tracker");
	Settings::AddNumber(GetName(), Settings::Category::Display, "Session History Kept",
		"Sessions kept", &sessionsKept, kMostKept,
		"How many session files are kept. Past that the oldest are deleted. 0 keeps "
		"them all.", "Keep Session History");
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
	BH::config->ReadInt("Session History Kept", sessionsKept, kDefaultKept);
	if (sessionsKept > kMostKept)
		sessionsKept = kMostKept;
	BH::config->ReadBoolean("Session HUD", hudOn);
	BH::config->ReadInt("Session HUD View", hudView, HudLayout::ViewLine);
	if (hudView >= HudLayout::ViewCount)
		hudView = HudLayout::ViewLine;
	BH::config->ReadInt("Session HUD Background", hudBackground,
		PlacedHud::BackgroundTranslucent);
	if (hudBackground > PlacedHud::BackgroundSolid)
		hudBackground = PlacedHud::BackgroundTranslucent;
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
	Prune();
}

void Session::OnLoop() {
	if (!enabled)
		return;

	const unsigned long long tick = BHGetTickCount();
	std::lock_guard<std::mutex> held(lock);
	ActOnRequests(tick);
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
	if (!current)
		return;
	const HudLayout::Figures figures = HudLayout::FiguresOf(current->model,
		NowOf(*current, BHGetTickCount()));
	hud.Draw(HudLayout::Sections((HudLayout::View)hudView, figures), hudBackground);
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

void Session::ActOnRequests(unsigned long long tick) {
	if (wantsNew.exchange(false))
		StartNew();

	const int view = wantsView.exchange(-1);
	if (view >= 0 && view < HudLayout::ViewCount)
		hudView = (unsigned int)view;
	// From the hotkey, and from the wheel over a HUD being placed.
	const int steps = wantsViewStep.exchange(0) + hud.TakeViewSteps();
	if (steps != 0)
		hudView = (unsigned int)HudLayout::Next((HudLayout::View)hudView, steps);

	if (wantsPlacing.exchange(false)) {
		hud.SetBeingPlaced(!hud.IsBeingPlaced());
		// A HUD is placed to be looked at.
		if (hud.IsBeingPlaced())
			hudOn = true;
	}
}

void Session::StartNew() {
	const unsigned long long tick = BHGetTickCount();
	std::vector<GameProbe::Seen> seen;
	probe.Leave(tick, seen);
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
		fact.name = PlayFormat::Plain(note.name);
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
		// One more log than there was.
		Prune();
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

	screenInfo->SetAutomapToken("SESSIONTIME", PlayFormat::Clock(now));
	screenInfo->SetAutomapToken("SESSIONPLAYTIME", PlayFormat::Clock(model.PlayedMs(now)));
	screenInfo->SetAutomapToken("SESSIONXPGAINED", std::to_string(net));
	screenInfo->SetAutomapToken("SESSIONXPPERSEC", PlayFormat::Rate(model.PlayedRate(now)));
	screenInfo->SetAutomapToken("SESSIONXPPERSECLONG", std::to_string(model.PlayedRate(now)));
	screenInfo->SetAutomapToken("RECENTXPPERSEC",
		PlayFormat::Rate(model.RecentRate(now, kRecentWindowMs)));
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

bool Session::Read(const std::function<void(const View&)>& reader) {
	Session* self = instance;
	if (!self)
		return false;
	std::lock_guard<std::mutex> held(self->lock);
	const Live* shown = self->viewed ? self->viewed.get() : self->current;
	if (!shown)
		return false;
	const Live& session = *shown;
	const View view = { session.model, self->NowOf(session, BHGetTickCount()),
		self->viewed ? self->viewedFile : std::string() };
	reader(view);
	return true;
}

bool Session::ReadHistory(const std::function<void(const HistoryView&)>& reader) {
	Session* self = instance;
	if (!self || !self->history)
		return false;
	std::lock_guard<std::mutex> held(self->lock);

	const unsigned long long tick = BHGetTickCount();
	if (self->historyPending > 0 || self->historyCheckedAt == 0 ||
			tick - self->historyCheckedAt >= kHistoryCheckEveryMs) {
		// The session being played is listed as itself, and not as one that is
		// over.
		std::set<std::string> playing;
		if (self->current && self->current->file.IsOpen())
			playing.insert(FileNameOf(self->current->file.GetPath()));
		self->historyPending = self->history->Reconcile(kLogsReadAtOnce, playing);
		self->historyCheckedAt = tick;
	}

	const HistoryView view = { self->history->Entries(), self->current != NULL,
		self->viewed ? self->viewedFile : std::string(), self->historyPending };
	reader(view);
	return true;
}

bool Session::ViewPast(const std::string& file) {
	Session* self = instance;
	if (!self)
		return false;
	std::lock_guard<std::mutex> held(self->lock);
	if (file.empty()) {
		self->viewed.reset();
		self->viewedFile.clear();
		return true;
	}

	std::vector<Event> events;
	if (!self->history || !self->history->ReadLog(file, events) || events.empty())
		return false;
	self->viewed = Replayed(events);
	self->viewedFile = file;
	return true;
}

std::unique_ptr<Session::Live> Session::Replayed(const std::vector<Event>& events) {
	std::unique_ptr<Live> log(new Live());
	log->beganAt = 0;
	log->written = 0;
	log->unwritable = false;
	log->shown = true;
	log->shownAt = events.empty() ? 0 : events.back().at;
	for (const Event& event : events)
		log->model.Apply(event);
	return log;
}

std::set<std::string> Session::OpenLogs() const {
	std::set<std::string> open;
	for (const auto& held : sessions) {
		if (held.second->file.IsOpen())
			open.insert(FileNameOf(held.second->file.GetPath()));
	}
	return open;
}

void Session::Prune() {
	if (history && keepHistory && sessionsKept > 0)
		history->Prune(sessionsKept, OpenLogs());
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

	self->shown = Replayed(events);
	self->current = self->shown.get();
	self->publishedAt = 0;
}
#endif
