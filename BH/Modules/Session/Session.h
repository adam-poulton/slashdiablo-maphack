#pragma once
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include "../Module.h"
#include "GameProbe.h"
#include "SessionFile.h"
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
		};

		static bool enabled;
		static bool keepHistory;

		// The game loop samples and the thread that notices a game ending says
		// so, which are different threads.
		std::mutex lock;
		GameProbe probe;
		std::map<std::string, std::unique_ptr<Live>> sessions;	// by whose they are
		Live* current;

		ScreenInfo* screenInfo;
		unsigned long long publishedAt;

		void Take(const std::vector<GameProbe::Seen>& seen);
		Live* SessionFor(const GameProbe::Identity& who, unsigned long long tick);
		void Record(Live& session, const SessionEvents::Event& event);

		// Brings the file up to date with the log, opening it if history has
		// been asked for since the session began.
		void Write(Live& session);

		void PublishTokens(unsigned long long tick);
		void ClearTokens();

	public:
		Session() : Module("Session"), current(NULL), screenInfo(NULL), publishedAt(0) {};

		void OnLoad();
		void LoadConfig();
		void OnSettingsChanged(const vector<string>& keys);

		void OnLoop();
		void OnGameExit();
};
