#include "Race.h"
#include "../../BH.h"
#include "../../Common.h"
#include "../../Constants.h"
#include "../../WholeFile.h"
#include "../ScreenInfo/ScreenInfo.h"
#include "../Settings/SettingsRegistry.h"
#include "../Tracking/PlayFormat.h"

using namespace PlayEvents;

namespace {
	// How often the tokens are rewritten. The fastest of them is a clock that
	// shows seconds.
	const unsigned long long kPublishEveryMs = 250;

	// The tokens this module keeps, for AutomapInfo lines and run tracker
	// columns.
	const char* const kTokens[] = { "RUNTIME", "RUNSPLIT" };

	// The routes there are to run, and the best each has been run, beside the
	// settings. Every client of an install shares them.
	const char* const kRoutesFile = "BH_routes.json";
	const char* const kBestsFile = "BH_runs.json";
	const char* const kBestsLock = "runs";

	// What a new character is: the level and experience the game starts one at.
	const unsigned int kFirstLevel = 1;

	const char* const kHeading = "Race";

	// Where the HUD's place is kept, and where it goes until it is moved: the
	// right of the screen, half way down.
	const char* const kHudSection = "RaceHud";
	const HudBlock::Place kHudPlace = { HudBlock::SideFar, HudBlock::SideMiddle, 3, 0 };

	const RunModel kNoRuns;

	std::string KeyFor(const GameProbe::Identity& who) {
		return who.account + "\n" + who.realm + "\n" + who.character;
	}

	std::vector<std::string> NamesOf(const Splits::Route& route) {
		std::vector<std::string> names;
		for (const Splits::Split& split : route.splits)
			names.push_back(split.name);
		return names;
	}
}

Race* Race::instance = NULL;
bool Race::enabled = true;
bool Race::hudOn = true;
unsigned int Race::hudView = RaceHudLayout::ViewSplits;
unsigned int Race::hudBackground = PlacedHud::BackgroundTranslucent;
bool Race::autoStart = false;
unsigned int Race::compareClock = RunStore::GameTime;
std::string Race::routeId;
unsigned int Race::startKey = 0;
unsigned int Race::stopKey = 0;
unsigned int Race::splitKey = 0;
unsigned int Race::undoKey = 0;
unsigned int Race::skipKey = 0;
unsigned int Race::cycleViewKey = 0;
unsigned int Race::placeHudKey = 0;

Race::Race() : Module("Race Timer"), current(NULL), hud(kHudSection, kHudPlace),
		routesEditable(true), wantsPlacing(false), wantsViewStep(0), wantsView(-1),
		wantsRun(RunNothing), screenInfo(NULL), publishedAt(0) {
	instance = this;
}

void Race::OnLoad() {
	LoadConfig();
	screenInfo = (ScreenInfo*)BH::moduleManager->Get("Screen Info");
	ClearTokens();
	hud.Load();
	LoadRoutes();
	std::string kept;
	if (WholeFile::Read(BH::path + kBestsFile, kept))
		bests.Read(kept);

	Settings::AddHeading(GetName(), Settings::Category::Display, kHeading);
	Settings::AddBool(GetName(), Settings::Category::Display, "Race Timer",
		"Race timer", &enabled,
		"Times runs against a route, marking its splits from what happens in the "
		"game. Type .race for its window, where a route is picked and a run started.");
	Settings::AddBool(GetName(), Settings::Category::Display, "Race HUD",
		"Race HUD", &hudOn,
		"Shows the run being timed over the game. It takes no clicks: type "
		".race hud move, or use Position HUD, to place it.",
		"Race Timer");
	std::vector<std::string> views;
	for (int view = 0; view < RaceHudLayout::ViewCount; view++)
		views.push_back(RaceHudLayout::NameOf((RaceHudLayout::View)view));
	Settings::AddEnum(GetName(), Settings::Category::Display, "Race HUD View",
		"HUD view", &hudView, views,
		"How much the HUD shows: the split the run is on, or every split.", "Race HUD");
	Settings::AddEnum(GetName(), Settings::Category::Display, "Race HUD Background",
		"HUD background", &hudBackground, { "None", "Translucent", "Solid" },
		"What the HUD's text is drawn on.", "Race HUD");
	Settings::AddBool(GetName(), Settings::Category::Display, "Race Auto Start",
		"Start a run with a new character", &autoStart,
		"Times a run on the route picked from the moment a new character joins "
		"its first game.", "Race Timer");
	Settings::AddText(GetName(), Settings::Category::Display, "Race Route",
		"Route", &routeId, 0,
		"The route runs are timed on, by its id in BH_routes.json: the one picked "
		"in the race window.", "Race Timer");
	Settings::AddEnum(GetName(), Settings::Category::Display, "Race Compare Clock",
		"Compare runs on", &compareClock, { "Real time", "Time in game" },
		"The clock a run is shown on and compared with its best by: all the time "
		"since it began, or only the time spent in a game.", "Race Timer");

	Settings::AddHeading(GetName(), Settings::Category::Input, kHeading);
	Settings::AddKey(GetName(), Settings::Category::Input, "Race Run Start",
		"Start run", &startKey,
		"Starts timing a run on the route picked, in place of any being timed.");
	Settings::AddKey(GetName(), Settings::Category::Input, "Race Run Stop",
		"Stop run", &stopKey, "Gives up the run being timed.");
	Settings::AddKey(GetName(), Settings::Category::Input, "Race Run Split",
		"Split", &splitKey, "Marks the split the run is on as reached.");
	Settings::AddKey(GetName(), Settings::Category::Input, "Race Run Undo",
		"Undo split", &undoKey, "Takes back the split last reached.");
	Settings::AddKey(GetName(), Settings::Category::Input, "Race Run Skip",
		"Skip split", &skipKey, "Passes over the split the run is on.");
	Settings::AddKey(GetName(), Settings::Category::Input, "Race HUD Cycle View",
		"Cycle HUD view", &cycleViewKey, "Steps the race HUD on to its next view.");
	Settings::AddKey(GetName(), Settings::Category::Input, "Race HUD Position",
		"Position HUD", &placeHudKey,
		"Lets the race HUD be dragged to where you want it, and again to finish.");
}

void Race::LoadConfig() {
	BH::config->ReadBoolean("Race Timer", enabled);
	BH::config->ReadBoolean("Race HUD", hudOn);
	BH::config->ReadInt("Race HUD View", hudView, RaceHudLayout::ViewSplits);
	if (hudView >= RaceHudLayout::ViewCount)
		hudView = RaceHudLayout::ViewSplits;
	BH::config->ReadInt("Race HUD Background", hudBackground, PlacedHud::BackgroundTranslucent);
	if (hudBackground > PlacedHud::BackgroundSolid)
		hudBackground = PlacedHud::BackgroundTranslucent;
	BH::config->ReadBoolean("Race Auto Start", autoStart);
	BH::config->ReadString("Race Route", routeId);
	BH::config->ReadInt("Race Compare Clock", compareClock, RunStore::GameTime);
	if (compareClock >= RunStore::Clocks)
		compareClock = RunStore::GameTime;
	BH::config->ReadKey("Race Run Start", "None", startKey);
	BH::config->ReadKey("Race Run Stop", "None", stopKey);
	BH::config->ReadKey("Race Run Split", "None", splitKey);
	BH::config->ReadKey("Race Run Undo", "None", undoKey);
	BH::config->ReadKey("Race Run Skip", "None", skipKey);
	BH::config->ReadKey("Race HUD Cycle View", "None", cycleViewKey);
	BH::config->ReadKey("Race HUD Position", "None", placeHudKey);
}

void Race::OnSettingsChanged(const vector<string>& keys) {
	if (enabled)
		return;
	// A timer switched off mid-game has seen the last of that game.
	std::lock_guard<std::mutex> held(lock);
	std::vector<GameProbe::Seen> seen;
	probe.Leave(BHGetTickCount(), seen);
	Take(seen);
	ClearTokens();
	hud.SetBeingPlaced(false);
}

void Race::OnLoop() {
	if (!enabled)
		return;

	const unsigned long long tick = BHGetTickCount();
	std::lock_guard<std::mutex> held(lock);
	ActOnRequests(tick);
	std::vector<GameProbe::Seen> seen;
	probe.Sample(tick, seen);
	Take(seen);
	PublishTokens(tick);
}

void Race::OnDraw() {
	if (!enabled || (!hudOn && !hud.IsBeingPlaced()))
		return;

	std::lock_guard<std::mutex> held(lock);
	RaceHudLayout::Figures figures;
	figures.inRun = false;
	if (current)
		figures = FiguresOf(*current, NowOf(*current, BHGetTickCount()));
	// A HUD being placed has to be there to be taken hold of.
	if (!figures.inRun && hud.IsBeingPlaced())
		figures = Sample();
	hud.Draw(RaceHudLayout::Sections((RaceHudLayout::View)hudView, figures), hudBackground);
}

void Race::OnGameExit() {
	std::lock_guard<std::mutex> held(lock);
	std::vector<GameProbe::Seen> seen;
	probe.Leave(BHGetTickCount(), seen);
	Take(seen);
	hud.SetBeingPlaced(false);
}

void Race::OnKey(bool up, BYTE key, LPARAM lParam, bool* block) {
	if (!enabled || key == 0)
		return;
	if (key != startKey && key != stopKey && key != splitKey && key != undoKey &&
			key != skipKey && key != cycleViewKey && key != placeHudKey)
		return;

	*block = true;
	if (up)
		return;
	if (key == startKey)
		wantsRun = RunStart;
	else if (key == stopKey)
		wantsRun = RunStop;
	else if (key == splitKey)
		wantsRun = RunSplit;
	else if (key == undoKey)
		wantsRun = RunUndo;
	else if (key == skipKey)
		wantsRun = RunSkip;
	else if (key == cycleViewKey)
		wantsViewStep++;
	else
		wantsPlacing = true;
}

void Race::ActOnRequests(unsigned long long tick) {
	std::string named;
	{
		std::lock_guard<std::mutex> held(askedLock);
		named.swap(askedRoute);
	}
	if (!named.empty())
		ChooseRoute(named);

	const int action = wantsRun.exchange(RunNothing);
	if (action != RunNothing)
		ActOnRun((RunAction)action, tick);

	const int view = wantsView.exchange(-1);
	if (view >= 0 && view < RaceHudLayout::ViewCount)
		hudView = (unsigned int)view;
	// From the hotkey, and from the wheel over a HUD being placed.
	const int steps = wantsViewStep.exchange(0) + hud.TakeViewSteps();
	if (steps != 0)
		hudView = (unsigned int)RaceHudLayout::Next((RaceHudLayout::View)hudView, steps);

	if (wantsPlacing.exchange(false)) {
		hud.SetBeingPlaced(!hud.IsBeingPlaced());
		// A HUD is placed to be looked at.
		if (hud.IsBeingPlaced())
			hudOn = true;
	}
}

void Race::Take(const std::vector<GameProbe::Seen>& seen) {
	for (const GameProbe::Seen& one : seen) {
		if (std::holds_alternative<Joined>(one.fact))
			current = TimingFor(probe.GetIdentity(), one.tick);
		if (!current || current->shown)
			continue;
		TakeFact(*current, NowOf(*current, one.tick), one.fact);
	}
}

Race::Timing* Race::TimingFor(const GameProbe::Identity& who, unsigned long long tick) {
	std::unique_ptr<Timing>& held = timings[KeyFor(who)];
	if (held)
		return held.get();

	held.reset(new Timing());
	held->beganAt = tick;
	held->classId = who.classId;
	held->hardcore = who.hardcore;
	held->hadBest = false;
	held->shown = false;
	held->shownAt = 0;
	return held.get();
}

unsigned long long Race::NowOf(const Timing& timing, unsigned long long tick) const {
	if (timing.shown)
		return timing.shownAt;
	return tick > timing.beganAt ? tick - timing.beganAt : 0;
}

void Race::TakeFact(Timing& timing, unsigned long long at, const Fact& fact) {
	timing.model.Apply({ at, fact });

	const Joined* joined = std::get_if<Joined>(&fact);
	if (joined && autoStart && !timing.evaluator.Running() &&
			joined->level == kFirstLevel && joined->experience == 0) {
		StartRun(timing, at);
		return;
	}

	if (!timing.evaluator.Running())
		return;
	const size_t last = timing.evaluator.GetRoute().splits.size() - 1;
	for (unsigned int reached : timing.evaluator.Feed(fact)) {
		timing.model.Apply({ at, SplitReached{ reached, false } });
		if (reached == last) {
			EndRun(timing, at, true);
			return;
		}
	}
}

// Read from their file, which is written with the routes BH comes with where
// there is none. A file that cannot be read is left as it is for its owner to
// mend, and the routes BH comes with stand in for it.
void Race::LoadRoutes() {
	const std::string path = BH::path + kRoutesFile;
	std::string text;
	routesEditable = true;
	if (!WholeFile::Read(path, text) || WholeFile::IsBlank(text)) {
		routes = Splits::Presets();
		WholeFile::Write(path, Splits::Write(routes));
	} else if (!Splits::Read(text, routes, NULL)) {
		routes = Splits::Presets();
		routesEditable = false;
	} else if (routes.empty()) {
		routes = Splits::Presets();
	}
	probe.Watch(Splits::ItemCodes(routes));
}

const Splits::Route* Race::PickedRoute() const {
	for (const Splits::Route& route : routes) {
		if (route.id == routeId)
			return &route;
	}
	return routes.empty() ? NULL : &routes.front();
}

void Race::ChooseRoute(const std::string& named) {
	const Splits::Route* found = Splits::Find(routes, named);
	if (!found) {
		PrintText(Red, "\377c4Race:\377c1 there is no route called %s.", named.c_str());
		return;
	}
	routeId = found->id;
	PrintText(White, "\377c4Race:\377c0 runs are timed on %s.", found->name.c_str());
}

void Race::StartRun(Timing& timing, unsigned long long at) {
	const Splits::Route* route = PickedRoute();
	if (!route || !timing.model.InGame())
		return;
	if (route->splits.empty()) {
		PrintText(White, "\377c4Race:\377c0 %s has no splits to time a run by.",
			route->name.c_str());
		return;
	}
	if (timing.evaluator.Running())
		EndRun(timing, at, false);

	timing.evaluator.Start(*route, timing.model.Difficulty(), timing.model.Level());

	RunStarted fact;
	fact.route = route->id;
	fact.name = route->name;
	fact.splits = NamesOf(*route);
	timing.model.Apply({ at, fact });

	// What the character had already done when the run began is not the run's.
	for (unsigned int i = 0; i < route->splits.size(); i++) {
		if (timing.evaluator.IsClosed(i))
			timing.model.Apply({ at, SplitSkipped{ i } });
	}
}

// The run is taken into the bests kept for its route, from the file as it is
// at that moment: another client may have written to it since it was read.
void Race::EndRun(Timing& timing, unsigned long long at, bool finished) {
	timing.model.Apply({ at, RunEnded{ finished } });
	timing.evaluator.Stop();
	if (timing.shown || timing.model.Runs().empty())
		return;

	const RunModel::Run& run = timing.model.Runs().back();
	const std::string key = RunStore::KeyFor(run.route, timing.classId, timing.hardcore);
	const std::string path = BH::path + kBestsFile;

	WholeFile::Lock held(kBestsLock, path);
	std::string kept;
	if (held.Taken() && WholeFile::Read(path, kept) && !WholeFile::IsBlank(kept))
		bests.Read(kept);
	const RunStore::Best* before = bests.Find(key);
	timing.hadBest = before != NULL;
	if (before)
		timing.bestBefore = *before;
	bests.Record(key, run);
	if (held.Taken())
		WholeFile::Write(path, bests.Write());
}

void Race::ActOnRun(RunAction action, unsigned long long tick) {
	if (!current || current->shown || !current->model.InGame())
		return;
	Timing& timing = *current;
	const unsigned long long at = NowOf(timing, tick);
	RunEvaluator& evaluator = timing.evaluator;

	if (action == RunStart) {
		StartRun(timing, at);
		return;
	}
	if (!evaluator.Running()) {
		PrintText(White, "\377c4Race:\377c0 no run is being timed. Type .run start.");
		return;
	}

	const unsigned int splits = (unsigned int)evaluator.GetRoute().splits.size();
	const unsigned int open = evaluator.FirstOpen();
	if (action == RunSplit && open < splits) {
		evaluator.Close(open);
		timing.model.Apply({ at, SplitReached{ open, true } });
		if (open == splits - 1)
			EndRun(timing, at, true);
	} else if (action == RunSkip && open < splits) {
		// The last split is the end of the run, which is reached or it is not.
		if (open == splits - 1) {
			PrintText(White, "\377c4Race:\377c0 the last split cannot be skipped.");
			return;
		}
		evaluator.Close(open);
		timing.model.Apply({ at, SplitSkipped{ open } });
	} else if (action == RunUndo) {
		const RunModel::Run* run = timing.model.ActiveRun();
		int last = -1;
		for (unsigned int i = 0; run && i < run->splits.size(); i++) {
			const RunModel::Split& split = run->splits[i];
			if (split.state == RunModel::Split::Reached &&
					(last < 0 || split.realMs >= run->splits[last].realMs))
				last = (int)i;
		}
		if (last < 0)
			return;
		evaluator.Reopen((unsigned int)last);
		timing.model.Apply({ at, SplitUndone{ (unsigned int)last } });
	} else if (action == RunStop) {
		EndRun(timing, at, false);
	}
}

const RunStore& Race::BestsFor(const Timing& timing) const {
	return timing.shown ? timing.ownBests : bests;
}

// The best of the route the character's latest run is on, for a character of
// its class.
const RunStore::Best* Race::BestOfRun(const Timing& timing) const {
	if (timing.model.Runs().empty())
		return NULL;
	const RunModel::Run& run = timing.model.Runs().back();
	if (run.ended && !timing.shown)
		return timing.hadBest ? &timing.bestBefore : NULL;
	return BestsFor(timing).Find(RunStore::KeyFor(run.route, timing.classId, timing.hardcore));
}

RaceHudLayout::Figures Race::FiguresOf(const Timing& timing, unsigned long long now) const {
	return RaceHudLayout::FiguresOf(timing.model, now, BestOfRun(timing),
		(RunStore::Clock)compareClock);
}

RaceHudLayout::Figures Race::Sample() const {
	const Splits::Route* route = PickedRoute();
	for (size_t i = 0; (!route || route->splits.empty()) && i < routes.size(); i++)
		route = &routes[i];
	if (!route || route->splits.empty()) {
		RaceHudLayout::Figures none;
		none.inRun = false;
		return none;
	}
	const RunStore::Best* best = current ? BestsFor(*current).Find(
		RunStore::KeyFor(route->id, current->classId, current->hardcore)) : NULL;
	return RaceHudLayout::FiguresOfRun(RunModel::Unstarted(route->id, route->name, NamesOf(*route)),
		0, best, (RunStore::Clock)compareClock);
}

void Race::PublishTokens(unsigned long long tick) {
	if (!screenInfo || tick - publishedAt < kPublishEveryMs)
		return;
	publishedAt = tick;

	RaceHudLayout::Figures figures;
	figures.inRun = false;
	if (current)
		figures = FiguresOf(*current, NowOf(*current, tick));
	screenInfo->SetAutomapToken("RUNTIME", figures.inRun ?
		PlayFormat::Clock((unsigned long long)figures.run.clock) : "");
	screenInfo->SetAutomapToken("RUNSPLIT", figures.inRun && figures.run.current >= 0 ?
		figures.run.splits[figures.run.current].name : "");
}

// A token with nothing in it is written as nothing, where one nobody has set is
// left on screen as its own name.
void Race::ClearTokens() {
	if (!screenInfo)
		return;
	for (const char* token : kTokens)
		screenInfo->SetAutomapToken(token, "");
	publishedAt = 0;
}

bool Race::Read(const std::function<void(const View&)>& reader) {
	Race* self = instance;
	if (!self)
		return false;
	std::lock_guard<std::mutex> held(self->lock);
	const Timing* timing = self->current;
	const Splits::Route* picked = self->PickedRoute();
	const View view = {
		timing ? timing->model : kNoRuns,
		timing ? self->NowOf(*timing, BHGetTickCount()) : 0,
		timing ? self->BestsFor(*timing) : self->bests,
		self->routes,
		picked ? picked->id : std::string(),
		self->routesEditable,
		(RunStore::Clock)compareClock,
		timing ? timing->classId : 0,
		timing ? timing->hardcore : false,
		enabled,
		enabled && timing && !timing->shown && timing->model.InGame(),
		autoStart,
	};
	reader(view);
	return true;
}

bool Race::EditRoutes(const std::function<void(std::vector<Splits::Route>&)>& edit) {
	Race* self = instance;
	if (!self)
		return false;
	std::lock_guard<std::mutex> held(self->lock);
	if (!self->routesEditable)
		return false;

	edit(self->routes);
	// There is always a route to pick.
	if (self->routes.empty())
		self->routes = Splits::Presets();
	WholeFile::Write(BH::path + kRoutesFile, Splits::Write(self->routes));
	self->probe.Watch(Splits::ItemCodes(self->routes));
	return true;
}

bool Race::PickRoute(const std::string& id) {
	Race* self = instance;
	if (!self)
		return false;
	std::lock_guard<std::mutex> held(self->lock);
	for (const Splits::Route& route : self->routes) {
		if (route.id == id) {
			routeId = id;
			return true;
		}
	}
	return false;
}

void Race::SetAutoStart(bool start) {
	autoStart = start;
}

void Race::RequestRun(RunAction action) {
	if (instance)
		instance->wantsRun = action;
}

void Race::RequestRoute(const std::string& idOrName) {
	if (!instance)
		return;
	std::lock_guard<std::mutex> held(instance->askedLock);
	instance->askedRoute = idOrName;
}

void Race::RequestHudView(RaceHudLayout::View view) {
	if (instance)
		instance->wantsView = (int)view;
}

void Race::RequestHudPlacing(bool placing) {
	if (instance && instance->hud.IsBeingPlaced() != placing)
		instance->wantsPlacing = true;
}

std::vector<std::string> Race::RouteNames() {
	std::vector<std::string> names;
	if (!instance)
		return names;
	std::lock_guard<std::mutex> held(instance->lock);
	for (const Splits::Route& route : instance->routes)
		names.push_back(route.id + " (" + route.name + ")");
	return names;
}

std::unique_ptr<Race::Timing> Race::Replayed(const std::vector<Event>& events) {
	std::unique_ptr<Timing> timing(new Timing());
	timing->beganAt = 0;
	timing->classId = 0;
	timing->hardcore = false;
	timing->hadBest = false;
	timing->shown = true;
	timing->shownAt = events.empty() ? 0 : events.back().at;
	for (const Event& event : events) {
		if (const Started* header = std::get_if<Started>(&event.fact)) {
			timing->classId = header->classId;
			timing->hardcore = header->hardcore;
		}
		timing->model.Apply(event);
	}

	for (const RunModel::Run& run : timing->model.Runs()) {
		if (run.ended)
			timing->ownBests.Record(RunStore::KeyFor(run.route, timing->classId, timing->hardcore), run);
	}
	return timing;
}

#ifdef BH_AGENT
void Race::Show(const std::vector<Event>& events) {
	Race* self = instance;
	if (!self)
		return;
	std::lock_guard<std::mutex> held(self->lock);

	self->shown = Replayed(events);
	self->current = self->shown.get();
	self->publishedAt = 0;
}
#endif
