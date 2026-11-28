#pragma once
#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include "../Module.h"
#include "../Tracking/GameProbe.h"
#include "../Tracking/PlacedHud.h"
#include "RaceHudLayout.h"
#include "RunEvaluator.h"
#include "RunModel.h"
#include "RunStore.h"
#include "SplitRoute.h"

class ScreenInfo;

// The race timer: a run timed against a route, its splits reached by what
// happens in the game, and set beside the best the route has been run.
//
// A run belongs to one character in this client. Joining a game on another
// character leaves it waiting, and coming back picks it up where it was.
//
// It reads the game for itself and keeps nothing with the session tracker: each
// works with the other switched off.
class Race : public Module {
	public:
		// The runs to show, as whoever shows them is handed them.
		struct View {
			const RunModel& model;						// empty until a character has been seen
			unsigned long long now;						// on its clock
			const RunStore& bests;
			const std::vector<Splits::Route>& routes;
			std::string route;							// the id of the route picked, which a run would start on
			bool editable;								// false where the routes' file is one BH could not read
			RunStore::Clock clock;
			unsigned int classId;
			bool hardcore;
			bool enabled;								// the timer is on
			bool inGame;								// a run could be started now
			bool autoStart;								// a new character's run starts by itself
		};

		// What can be asked of the run in hand.
		enum RunAction {
			RunNothing,
			RunStart,		// a run on the route picked, in place of any in hand
			RunStop,		// the run is given up
			RunSplit,		// the first open split is reached, on the player's word
			RunUndo,		// the split last reached is open again
			RunSkip			// the first open split is passed over
		};

	private:
		// A character's runs in this client.
		struct Timing {
			unsigned long long beganAt;				// tick its clock reads zero at
			unsigned int classId;
			bool hardcore;
			RunModel model;
			RunEvaluator evaluator;					// for the run in hand

			// What the run last ended was up against: the bests of its route
			// as they stood before it was taken into them. A run that has
			// just set a best is still read beside the one it beat.
			bool hadBest;
			RunStore::Best bestBefore;

			// Runs put up to be looked at: they take nothing from the game, and
			// their clock stands at their last event. They are set beside bests
			// of their own, made from the ones of them that are over.
			bool shown;
			unsigned long long shownAt;
			RunStore ownBests;
		};

		static Race* instance;

		static bool enabled;
		static bool hudOn;
		static unsigned int hudView;
		static unsigned int hudBackground;
		static bool autoStart;
		static unsigned int compareClock;
		static std::string routeId;				// the route picked: the one runs are timed on
		static unsigned int startKey;
		static unsigned int stopKey;
		static unsigned int splitKey;
		static unsigned int undoKey;
		static unsigned int skipKey;
		static unsigned int cycleViewKey;
		static unsigned int placeHudKey;

		// The game loop samples and the thread that notices a game ending says
		// so, which are different threads.
		std::mutex lock;
		GameProbe probe;
		std::map<std::string, std::unique_ptr<Timing>> timings;	// by whose they are
		std::unique_ptr<Timing> shown;
		Timing* current;

		PlacedHud hud;

		// The routes there are to run and the best each has been run.
		std::vector<Splits::Route> routes;
		bool routesEditable;
		RunStore bests;

		// What a key or a command asked for, done on the game loop.
		std::atomic<bool> wantsPlacing;			// to start placing the HUD, or to stop
		std::atomic<int> wantsViewStep;
		std::atomic<int> wantsView;				// a view, or -1
		std::atomic<int> wantsRun;				// a RunAction
		std::mutex askedLock;
		std::string askedRoute;					// a route to pick, by id or name

		ScreenInfo* screenInfo;
		unsigned long long publishedAt;

		void Take(const std::vector<GameProbe::Seen>& seen);
		Timing* TimingFor(const GameProbe::Identity& who, unsigned long long tick);
		unsigned long long NowOf(const Timing& timing, unsigned long long tick) const;

		// Takes in a fact the game gave, and whatever of the run in hand it
		// reaches.
		void TakeFact(Timing& timing, unsigned long long at, const PlayEvents::Fact& fact);

		static std::unique_ptr<Timing> Replayed(const std::vector<PlayEvents::Event>& events);

		void LoadRoutes();

		// The route picked, or the first there is where none has been or the
		// one that was has gone: there is always a route to time a run on.
		const Splits::Route* PickedRoute() const;
		void StartRun(Timing& timing, unsigned long long at);
		void EndRun(Timing& timing, unsigned long long at, bool finished);
		void ActOnRun(RunAction action, unsigned long long tick);
		void ChooseRoute(const std::string& asked);

		const RunStore& BestsFor(const Timing& timing) const;
		const RunStore::Best* BestOfRun(const Timing& timing) const;
		RaceHudLayout::Figures FiguresOf(const Timing& timing, unsigned long long now) const;

		// What a HUD being placed shows where there is no run to: the route
		// picked, as it stands before it is run.
		RaceHudLayout::Figures Sample() const;

		void ActOnRequests(unsigned long long tick);
		void PublishTokens(unsigned long long tick);
		void ClearTokens();

	public:
		Race();

		void OnLoad();
		void LoadConfig();
		void OnSettingsChanged(const vector<string>& keys);

		void OnLoop();
		void OnDraw();
		void OnGameExit();
		void OnKey(bool up, BYTE key, LPARAM lParam, bool* block);

		static bool IsEnabled() { return enabled; }

		// Hands the runs and the routes to whoever wants to read them.
		static bool Read(const std::function<void(const View&)>& reader);

		// Changes the routes and writes them to their file. False, having
		// changed nothing, where the file is one BH could not read: it is its
		// owner's to mend, and is not written over.
		static bool EditRoutes(const std::function<void(std::vector<Splits::Route>&)>& edit);

		// Picks the route runs are timed on, by its id. False where there is no
		// such route.
		static bool PickRoute(const std::string& id);

		// Whether a new character's run starts by itself.
		static void SetAutoStart(bool start);

		// Asked for from a key, a chat command or a window, and done on the
		// game loop.
		static void RequestRun(RunAction action);
		static void RequestRoute(const std::string& idOrName);
		static void RequestHudView(RaceHudLayout::View view);
		static void RequestHudPlacing(bool placing);

		// What each route is called, for saying what there is to choose from.
		static std::vector<std::string> RouteNames();

#ifdef BH_AGENT
		// Puts a log's runs up in place of the character's, until the next game
		// is joined. On the game's thread.
		static void Show(const std::vector<PlayEvents::Event>& events);
#endif
};
