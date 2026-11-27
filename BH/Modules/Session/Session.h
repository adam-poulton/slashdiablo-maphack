#pragma once
#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include "../Module.h"
#include "GameProbe.h"
#include "HudLayout.h"
#include "SessionFile.h"
#include "SessionHud.h"
#include "SessionModel.h"

class ScreenInfo;

// The session tracker: what a sitting on one character has come to. Time played,
// games, experience and how fast it is coming, where it was earned, deaths.
//
// A session belongs to one character in this client. Joining a game on another
// character starts or picks up that character's session, and coming back picks
// this one up where it was.
//
// It counts for itself and beside the run tracker in ScreenInfo, whose figures,
// tokens and file are no part of this and are not changed by it.
class Session : public Module {
	private:
		struct Live {
			unsigned long long beganAt;				// tick the session's clock reads zero at
			SessionModel model;
			std::vector<SessionEvents::Event> log;
			SessionFile file;
			unsigned int written;					// events of the log that are in the file
			bool unwritable;						// the file could not be opened

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
		static unsigned int hudBackground;
		static unsigned int cycleViewKey;
		static unsigned int placeHudKey;
		static unsigned int newSessionKey;

		// The game loop samples and the thread that notices a game ending says
		// so, which are different threads.
		std::mutex lock;
		GameProbe probe;
		std::map<std::string, std::unique_ptr<Live>> sessions;	// by whose they are
		std::unique_ptr<Live> shown;
		Live* current;

		SessionHud hud;

		// What a key or a command asked for, done on the game loop.
		std::atomic<bool> wantsNew;
		std::atomic<bool> wantsPlacing;			// to start placing the HUD, or to stop
		std::atomic<int> wantsViewStep;
		std::atomic<int> wantsView;				// a view, or -1

		ScreenInfo* screenInfo;
		unsigned long long publishedAt;

		void Take(const std::vector<GameProbe::Seen>& seen);
		Live* SessionFor(const GameProbe::Identity& who, unsigned long long tick);
		void Record(Live& session, const SessionEvents::Event& event);
		unsigned long long NowOf(const Live& session, unsigned long long tick) const;

		// Brings the file up to date with the log, opening it if history has
		// been asked for since the session began.
		void Write(Live& session);

		// Ends the session in hand. The game being played, if there is one, is
		// joined again as the first of the next.
		void StartNew();

		void ActOnRequests();
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

		// Hands the session in hand, and the moment on its clock, to whoever
		// wants to read it. False, having called nothing, where there is none.
		static bool Read(const std::function<void(const SessionModel&, unsigned long long)>& reader);

		// Asked for from a chat command or a window, and done on the game loop.
		static void RequestNew();
		static void RequestHudView(HudLayout::View view);
		static void RequestHudPlacing(bool placing);

#ifdef BH_AGENT
		// Puts a log up in place of the session in hand, until the next game is
		// joined. On the game's thread.
		static void Show(const std::vector<SessionEvents::Event>& events);
#endif
};
