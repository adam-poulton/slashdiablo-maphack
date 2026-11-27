#include "Session.h"
#include <ctime>
#include <filesystem>
#include "../../BH.h"
#include "../../Common.h"
#include "../../Constants.h"
#include "../../WholeFile.h"
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
		"SESSIONKILLS", "SESSIONDROPS", "RUNTIME", "RUNSPLIT",
	};

	// The routes there are to run, and the best each has been run, beside the
	// settings. Every client of an install shares them.
	const char* const kRoutesFile = "BH_routes.json";
	const char* const kBestsFile = "BH_runs.json";
	const char* const kBestsLock = "runs";

	const char* const kDefaultRoute = "bosses-normal";

	// What a new character is: the level and experience the game starts one at.
	const unsigned int kFirstLevel = 1;

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
unsigned int Session::hudRunView = HudLayout::ViewSplits + 1;
unsigned int Session::hudBackground = SessionHud::BackgroundTranslucent;
bool Session::autoStart = true;
unsigned int Session::compareClock = RunStore::GameTime;
std::string Session::routeId = kDefaultRoute;
unsigned int Session::cycleViewKey = 0;
unsigned int Session::placeHudKey = 0;
unsigned int Session::newSessionKey = 0;
unsigned int Session::runStartKey = 0;
unsigned int Session::runSplitKey = 0;
unsigned int Session::runUndoKey = 0;
unsigned int Session::runSkipKey = 0;
unsigned int Session::runResetKey = 0;

Session::Session() : Module("Session Tracker"), current(NULL), wantsNew(false),
		wantsPlacing(false), wantsViewStep(0), wantsView(-1), wantsRun(RunNothing),
		screenInfo(NULL), publishedAt(0) {
	instance = this;
}

void Session::OnLoad() {
	LoadConfig();
	screenInfo = (ScreenInfo*)BH::moduleManager->Get("Screen Info");
	ClearTokens();
	hud.Load();
	LoadRoutes();
	std::string kept;
	if (WholeFile::Read(BH::path + kBestsFile, kept))
		bests.Read(kept);

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
	std::vector<std::string> runViews = views;
	runViews.insert(runViews.begin(), "Same");
	Settings::AddEnum(GetName(), Settings::Category::Display, "Session HUD Run View",
		"HUD view during a run", &hudRunView, runViews,
		"The view the HUD takes while a run is being timed, and goes back from "
		"when it is over.", "Session HUD");
	Settings::AddEnum(GetName(), Settings::Category::Display, "Session HUD Background",
		"HUD background", &hudBackground, { "None", "Translucent", "Solid" },
		"What the HUD's text is drawn on.", "Session HUD");
	Settings::AddBool(GetName(), Settings::Category::Display, "Session Auto Start Runs",
		"Start a run with a new character", &autoStart,
		"Times a run on the chosen route from the moment a new character joins "
		"its first game.", "Session Tracker");
	Settings::AddText(GetName(), Settings::Category::Display, "Session Route",
		"Run route", &routeId, 0,
		"The route runs are timed on, by its id in BH_routes.json. Type .route in "
		"chat for the ones there are.", "Session Tracker");
	Settings::AddEnum(GetName(), Settings::Category::Display, "Session Compare Clock",
		"Compare runs on", &compareClock, { "Real time", "Time in game" },
		"The clock a run is shown on and compared with its best by: all the time "
		"since it began, or only the time spent in a game.", "Session Tracker");

	Settings::AddHeading(GetName(), Settings::Category::Input, kHeading);
	Settings::AddKey(GetName(), Settings::Category::Input, "Session HUD Cycle View",
		"Cycle HUD view", &cycleViewKey, "Steps the session HUD on to its next view.");
	Settings::AddKey(GetName(), Settings::Category::Input, "Session HUD Position",
		"Position HUD", &placeHudKey,
		"Lets the session HUD be dragged to where you want it, and again to finish.");
	Settings::AddKey(GetName(), Settings::Category::Input, "Session New",
		"New session", &newSessionKey,
		"Ends the session and starts another from the game you are in.");
	Settings::AddKey(GetName(), Settings::Category::Input, "Session Run Start",
		"Start run", &runStartKey,
		"Starts timing a run on the chosen route, in place of any being timed.");
	Settings::AddKey(GetName(), Settings::Category::Input, "Session Run Split",
		"Split", &runSplitKey, "Marks the split the run is on as reached.");
	Settings::AddKey(GetName(), Settings::Category::Input, "Session Run Undo",
		"Undo split", &runUndoKey, "Takes back the split last reached.");
	Settings::AddKey(GetName(), Settings::Category::Input, "Session Run Skip",
		"Skip split", &runSkipKey, "Passes over the split the run is on.");
	Settings::AddKey(GetName(), Settings::Category::Input, "Session Run Reset",
		"Reset run", &runResetKey, "Gives up the run being timed.");
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

	BH::config->ReadInt("Session HUD Run View", hudRunView, HudLayout::ViewSplits + 1);
	if (hudRunView > HudLayout::ViewCount)
		hudRunView = HudLayout::ViewSplits + 1;
	BH::config->ReadBoolean("Session Auto Start Runs", autoStart);
	BH::config->ReadString("Session Route", routeId);
	if (routeId.empty())
		routeId = kDefaultRoute;
	BH::config->ReadInt("Session Compare Clock", compareClock, RunStore::GameTime);
	if (compareClock >= RunStore::Clocks)
		compareClock = RunStore::GameTime;
	BH::config->ReadKey("Session Run Start", "None", runStartKey);
	BH::config->ReadKey("Session Run Split", "None", runSplitKey);
	BH::config->ReadKey("Session Run Undo", "None", runUndoKey);
	BH::config->ReadKey("Session Run Skip", "None", runSkipKey);
	BH::config->ReadKey("Session Run Reset", "None", runResetKey);
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
	const HudLayout::Figures figures = FiguresOf(*current, NowOf(*current, BHGetTickCount()));
	hud.Draw(figures, ShownView(figures), hudBackground);
}

HudLayout::View Session::ShownView(const HudLayout::Figures& figures) const {
	return (HudLayout::View)(figures.inRun && hudRunView > 0 ? hudRunView - 1 : hudView);
}

void Session::SetShownView(HudLayout::View view) {
	const bool inRun = current &&
		FiguresOf(*current, NowOf(*current, BHGetTickCount())).inRun;
	if (inRun && hudRunView > 0)
		hudRunView = (unsigned int)view + 1;
	else
		hudView = (unsigned int)view;
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
	if (key != cycleViewKey && key != placeHudKey && key != newSessionKey &&
			key != runStartKey && key != runSplitKey && key != runUndoKey &&
			key != runSkipKey && key != runResetKey)
		return;

	*block = true;
	if (up)
		return;
	if (key == cycleViewKey)
		wantsViewStep++;
	else if (key == placeHudKey)
		wantsPlacing = true;
	else if (key == newSessionKey)
		wantsNew = true;
	else if (key == runStartKey)
		wantsRun = RunStart;
	else if (key == runSplitKey)
		wantsRun = RunSplit;
	else if (key == runUndoKey)
		wantsRun = RunUndo;
	else if (key == runSkipKey)
		wantsRun = RunSkip;
	else
		wantsRun = RunReset;
}

void Session::ActOnRequests(unsigned long long tick) {
	if (wantsNew.exchange(false))
		StartNew();

	std::string asked;
	{
		std::lock_guard<std::mutex> held(askedLock);
		asked.swap(askedRoute);
	}
	if (!asked.empty())
		ChooseRoute(asked);

	const int action = wantsRun.exchange(RunNothing);
	if (action != RunNothing)
		ActOnRun((RunAction)action, tick);

	const int view = wantsView.exchange(-1);
	if (view >= 0 && view < HudLayout::ViewCount)
		SetShownView((HudLayout::View)view);
	// From the hotkey, and from the wheel over a HUD being placed.
	const int steps = wantsViewStep.exchange(0) + hud.TakeViewSteps();
	if (steps != 0 && current) {
		const HudLayout::View showing =
			ShownView(FiguresOf(*current, NowOf(*current, BHGetTickCount())));
		SetShownView(HudLayout::Next(showing, steps));
	}

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
	// A run does not outlast the session it was timed in.
	if (current && !current->shown && current->evaluator.Running()) {
		EndRun(*current, NowOf(*current, tick), false);
		Write(*current);
	}

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
		TakeFact(*current, NowOf(*current, one.tick), one.fact);
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
	held->hadBest = false;

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

	const HudLayout::Figures figures = FiguresOf(*current, now);
	screenInfo->SetAutomapToken("RUNTIME", figures.inRun ?
		SessionFormat::Clock((unsigned long long)figures.run.clock) : "");
	screenInfo->SetAutomapToken("RUNSPLIT", figures.inRun && figures.run.current >= 0 ?
		figures.run.splits[figures.run.current].name : "");
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
	if (!self->current)
		return false;
	const Live& session = *self->current;
	const Splits::Route* chosen = self->ChosenRoute();
	const View view = { session.model, self->NowOf(session, BHGetTickCount()),
		self->BestsFor(session), self->routes, chosen ? chosen->id : std::string(),
		(RunStore::Clock)compareClock };
	reader(view);
	return true;
}

void Session::TakeFact(Live& session, unsigned long long at, const Fact& fact) {
	Record(session, { at, fact });

	const Joined* joined = std::get_if<Joined>(&fact);
	if (joined && autoStart && !session.evaluator.Running() &&
			joined->level == kFirstLevel && joined->experience == 0) {
		StartRun(session, at);
		return;
	}

	if (!session.evaluator.Running())
		return;
	const size_t last = session.evaluator.GetRoute().splits.size() - 1;
	for (unsigned int reached : session.evaluator.Feed(fact)) {
		Record(session, { at, SplitReached{ reached, false } });
		if (reached == last) {
			EndRun(session, at, true);
			return;
		}
	}
}

// Read from their file, which is written with the routes BH comes with where
// there is none. A file that cannot be read is left as it is for its owner to
// mend, and the routes BH comes with stand in for it.
void Session::LoadRoutes() {
	const std::string path = BH::path + kRoutesFile;
	std::string text;
	if (!WholeFile::Read(path, text) || WholeFile::IsBlank(text)) {
		routes = Splits::Presets();
		WholeFile::Write(path, Splits::Write(routes));
	} else if (!Splits::Read(text, routes, NULL) || routes.empty()) {
		routes = Splits::Presets();
	}
	probe.Watch(Splits::ItemCodes(routes));
}

const Splits::Route* Session::ChosenRoute() const {
	const Splits::Route* chosen = Splits::Find(routes, routeId);
	return chosen || routes.empty() ? chosen : &routes.front();
}

void Session::ChooseRoute(const std::string& asked) {
	const Splits::Route* found = Splits::Find(routes, asked);
	if (!found) {
		PrintText(Red, "\377c4Session:\377c1 there is no route called %s.", asked.c_str());
		return;
	}
	routeId = found->id;
	PrintText(White, "\377c4Session:\377c0 runs are timed on %s.", found->name.c_str());
}

void Session::StartRun(Live& session, unsigned long long at) {
	const Splits::Route* route = ChosenRoute();
	if (!route || route->splits.empty() || !session.model.InGame())
		return;
	if (session.evaluator.Running())
		EndRun(session, at, false);

	const SessionModel::Game& game = session.model.Games().back();
	session.evaluator.Start(*route, game.difficulty, game.level);

	RunStarted fact;
	fact.route = route->id;
	fact.name = route->name;
	for (const Splits::Split& split : route->splits)
		fact.splits.push_back(split.name);
	Record(session, { at, fact });

	// What the character had already done when the run began is not the run's.
	for (unsigned int i = 0; i < route->splits.size(); i++) {
		if (session.evaluator.IsClosed(i))
			Record(session, { at, SplitSkipped{ i } });
	}
}

// The run is taken into the bests kept for its route, from the file as it is
// at that moment: another client may have written to it since it was read.
void Session::EndRun(Live& session, unsigned long long at, bool finished) {
	Record(session, { at, RunEnded{ finished } });
	session.evaluator.Stop();
	if (session.shown || session.model.Runs().empty())
		return;

	const SessionModel::Run& run = session.model.Runs().back();
	const Started& header = session.model.Header();
	const std::string key = RunStore::KeyFor(run.route, header.classId, header.hardcore);
	const std::string path = BH::path + kBestsFile;

	WholeFile::Lock held(kBestsLock, path);
	std::string kept;
	if (held.Taken() && WholeFile::Read(path, kept) && !WholeFile::IsBlank(kept))
		bests.Read(kept);
	const RunStore::Best* before = bests.Find(key);
	session.hadBest = before != NULL;
	if (before)
		session.bestBefore = *before;
	bests.Record(key, run);
	if (held.Taken())
		WholeFile::Write(path, bests.Write());
}

void Session::ActOnRun(RunAction action, unsigned long long tick) {
	if (!current || current->shown || !current->model.InGame())
		return;
	Live& session = *current;
	const unsigned long long at = NowOf(session, tick);
	RunEvaluator& evaluator = session.evaluator;

	if (action == RunStart) {
		StartRun(session, at);
		Write(session);
		return;
	}
	if (!evaluator.Running()) {
		PrintText(White, "\377c4Session:\377c0 no run is being timed. Type .run start.");
		return;
	}

	const unsigned int splits = (unsigned int)evaluator.GetRoute().splits.size();
	const unsigned int open = evaluator.FirstOpen();
	if (action == RunSplit && open < splits) {
		evaluator.Close(open);
		Record(session, { at, SplitReached{ open, true } });
		if (open == splits - 1)
			EndRun(session, at, true);
	} else if (action == RunSkip && open < splits) {
		// The last split is the end of the run, which is reached or it is not.
		if (open == splits - 1) {
			PrintText(White, "\377c4Session:\377c0 the last split cannot be skipped.");
			return;
		}
		evaluator.Close(open);
		Record(session, { at, SplitSkipped{ open } });
	} else if (action == RunUndo) {
		const SessionModel::Run* run = session.model.ActiveRun();
		int last = -1;
		for (unsigned int i = 0; run && i < run->splits.size(); i++) {
			const SessionModel::RunSplit& split = run->splits[i];
			if (split.state == SessionModel::RunSplit::Reached &&
					(last < 0 || split.realMs >= run->splits[last].realMs))
				last = (int)i;
		}
		if (last < 0)
			return;
		evaluator.Reopen((unsigned int)last);
		Record(session, { at, SplitUndone{ (unsigned int)last } });
	} else if (action == RunReset) {
		EndRun(session, at, false);
	}
	Write(session);
}

const RunStore& Session::BestsFor(const Live& session) const {
	return session.shown ? shownBests : bests;
}

// The best of the route the session's latest run is on, for a character of its
// class.
const RunStore::Best* Session::BestOfRun(const Live& session) const {
	if (session.model.Runs().empty())
		return NULL;
	const SessionModel::Run& run = session.model.Runs().back();
	if (run.ended && !session.shown)
		return session.hadBest ? &session.bestBefore : NULL;
	const Started& header = session.model.Header();
	return BestsFor(session).Find(RunStore::KeyFor(run.route, header.classId, header.hardcore));
}

HudLayout::Figures Session::FiguresOf(const Live& session, unsigned long long now) const {
	return HudLayout::FiguresOf(session.model, now, BestOfRun(session),
		(RunStore::Clock)compareClock);
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

void Session::RequestRun(RunAction action) {
	if (instance)
		instance->wantsRun = action;
}

void Session::RequestRoute(const std::string& idOrName) {
	if (!instance)
		return;
	std::lock_guard<std::mutex> held(instance->askedLock);
	instance->askedRoute = idOrName;
}

std::vector<std::string> Session::RouteNames() {
	std::vector<std::string> names;
	if (!instance)
		return names;
	std::lock_guard<std::mutex> held(instance->lock);
	for (const Splits::Route& route : instance->routes)
		names.push_back(route.id + " (" + route.name + ")");
	return names;
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
	log.hadBest = false;
	for (const Event& event : events)
		log.model.Apply(event);

	// The runs the log holds that are over are what its run in hand is set
	// beside: nothing of the player's own bests is shown against it.
	self->shownBests = RunStore();
	const Started& header = log.model.Header();
	for (const SessionModel::Run& run : log.model.Runs()) {
		if (run.ended)
			self->shownBests.Record(RunStore::KeyFor(run.route, header.classId, header.hardcore), run);
	}
	self->current = &log;
	self->publishedAt = 0;
}
#endif
