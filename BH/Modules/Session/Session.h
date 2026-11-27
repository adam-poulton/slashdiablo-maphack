#pragma once
#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>
#include "../Module.h"
#include "GameProbe.h"
#include "HudLayout.h"
#include "RunEvaluator.h"
#include "RunStore.h"
#include "SessionFile.h"
#include "SessionHud.h"
#include "SessionModel.h"
#include "SplitRoute.h"

class ScreenInfo;

// The session tracker: what a sitting on one character has come to. Time played,
// games, experience and how fast it is coming, where it was earned, deaths, what
// died and what dropped, and a run timed against a route.
//
// A session belongs to one character in this client. Joining a game on another
// character starts or picks up that character's session, and coming back picks
// this one up where it was.
//
// It counts for itself and beside the run tracker in ScreenInfo, whose figures,
// tokens and file are no part of this and are not changed by it.
class Session : public Module {
	public:
		// An item on the ground that the item filter pinged, as whichever part
		// of the filter pinged it knows it.
		struct DropNote {
			unsigned int id;			// the item's unit id
			std::string name;
			std::string code;
			unsigned int quality;
			unsigned int color;			// the text colour it was, or would be, announced in
			unsigned int ping;
			unsigned int area;
		};

		// The session in hand, as whoever shows it is handed it.
		struct View {
			const SessionModel& model;
			unsigned long long now;						// on the session's clock
			const RunStore& bests;
			const std::vector<Splits::Route>& routes;
			std::string route;							// the id of the one a run would start on
			RunStore::Clock clock;
		};

		// What can be asked of the run in hand.
		enum RunAction {
			RunNothing,
			RunStart,		// a run on the chosen route, in place of any in hand
			RunSplit,		// the first open split is reached, on the player's word
			RunUndo,		// the split last reached is open again
			RunSkip,		// the first open split is passed over
			RunReset		// the run is given up
		};

	private:
		struct Live {
			unsigned long long beganAt;				// tick the session's clock reads zero at
			SessionModel model;
			std::vector<SessionEvents::Event> log;
			SessionFile file;
			unsigned int written;					// events of the log that are in the file
			bool unwritable;						// the file could not be opened
			RunEvaluator evaluator;					// for the run in hand

			// What the run last ended was up against: the bests of its route
			// as they stood before it was taken into them. A run that has
			// just set a best is still read beside the one it beat.
			bool hadBest;
			RunStore::Best bestBefore;

			// A log put up to be looked at: it takes nothing from the game, is
			// written nowhere, and its clock stands at the log's last event.
			bool shown;
			unsigned long long shownAt;
		};

		static Session* instance;

		static bool enabled;
		static bool keepHistory;
		static bool hudOn;
		static unsigned int hudView;
		static unsigned int hudRunView;			// 0 for the same, or a view and one
		static unsigned int hudBackground;
		static bool autoStart;
		static unsigned int compareClock;
		static std::string routeId;
		static unsigned int cycleViewKey;
		static unsigned int placeHudKey;
		static unsigned int newSessionKey;
		static unsigned int runStartKey;
		static unsigned int runSplitKey;
		static unsigned int runUndoKey;
		static unsigned int runSkipKey;
		static unsigned int runResetKey;

		// The game loop samples and the thread that notices a game ending says
		// so, which are different threads.
		std::mutex lock;
		GameProbe probe;
		std::map<std::string, std::unique_ptr<Live>> sessions;	// by whose they are
		std::unique_ptr<Live> shown;
		Live* current;

		SessionHud hud;

		// The routes there are to run and the best each has been run. A log put
		// up to be looked at is compared with bests of its own, made from the
		// runs it holds.
		std::vector<Splits::Route> routes;
		RunStore bests;
		RunStore shownBests;

		// Drops are noted from wherever the item filter runs and taken on the
		// game loop. An item is one drop however many times it is pinged, so
		// those already taken in this game are remembered.
		std::mutex dropLock;
		std::vector<DropNote> notedDrops;
		std::set<unsigned int> takenDrops;

		// What a key or a command asked for, done on the game loop.
		std::atomic<bool> wantsNew;
		std::atomic<bool> wantsPlacing;			// to start placing the HUD, or to stop
		std::atomic<int> wantsViewStep;
		std::atomic<int> wantsView;				// a view, or -1
		std::atomic<int> wantsRun;				// a RunAction
		std::mutex askedLock;
		std::string askedRoute;					// a route to choose, by id or name

		ScreenInfo* screenInfo;
		unsigned long long publishedAt;

		void Take(const std::vector<GameProbe::Seen>& seen);
		void TakeDrops(unsigned long long tick);
		Live* SessionFor(const GameProbe::Identity& who, unsigned long long tick);
		void Record(Live& session, const SessionEvents::Event& event);
		unsigned long long NowOf(const Live& session, unsigned long long tick) const;

		// Takes in a fact the game gave, and whatever of the run in hand it
		// reaches.
		void TakeFact(Live& session, unsigned long long at, const SessionEvents::Fact& fact);

		// Brings the file up to date with the log, opening it if history has
		// been asked for since the session began.
		void Write(Live& session);

		// Ends the session in hand. The game being played, if there is one, is
		// joined again as the first of the next.
		void StartNew();

		void LoadRoutes();
		const Splits::Route* ChosenRoute() const;
		void StartRun(Live& session, unsigned long long at);
		void EndRun(Live& session, unsigned long long at, bool finished);
		void ActOnRun(RunAction action, unsigned long long tick);
		void ChooseRoute(const std::string& asked);

		const RunStore& BestsFor(const Live& session) const;
		const RunStore::Best* BestOfRun(const Live& session) const;
		HudLayout::Figures FiguresOf(const Live& session, unsigned long long now) const;

		// The setting the HUD's view is being read from: the one for a run
		// while a run is showing and has a view of its own.
		void SetShownView(HudLayout::View view);
		HudLayout::View ShownView(const HudLayout::Figures& figures) const;

		void ActOnRequests(unsigned long long tick);
		void PublishTokens(unsigned long long tick);
		void ClearTokens();

	public:
		Session();

		void OnLoad();
		void LoadConfig();
		void OnSettingsChanged(const vector<string>& keys);

		void OnLoop();
		void OnDraw();
		void OnGameExit();
		void OnKey(bool up, BYTE key, LPARAM lParam, bool* block);

		static bool IsEnabled() { return enabled; }

		// Hands the session in hand to whoever wants to read it. False, having
		// called nothing, where there is none.
		static bool Read(const std::function<void(const View&)>& reader);

		// Called for each ground item the filter pings, from any thread, and as
		// often as the filter pings it.
		static void NoteDrop(const DropNote& drop);

		// Asked for from a chat command or a window, and done on the game loop.
		static void RequestNew();
		static void RequestHudView(HudLayout::View view);
		static void RequestHudPlacing(bool placing);
		static void RequestRun(RunAction action);
		static void RequestRoute(const std::string& idOrName);

		// What each route is called, for saying what there is to choose from.
		static std::vector<std::string> RouteNames();

#ifdef BH_AGENT
		// Puts a log up in place of the session in hand, until the next game is
		// joined. On the game's thread.
		static void Show(const std::vector<SessionEvents::Event>& events);
#endif
};
