#pragma once
#include <string>
#include <vector>
#include "../Tracking/PlayEvents.h"

// The runs a character has been timed on, worked out from events and from
// nothing else: it never asks the game or the clock anything. A run is a clock
// from when it began and the splits reached on the way, kept on two clocks at
// once: all the time since it began, and the part of that spent in a game.
//
// Of what happens in the game it takes only what a run's clocks and its splits'
// levels are read from: games joined and left, and levels reached. Which splits
// a kill or a quest reaches is the evaluator's to say, and comes here as a split
// reached.
class RunModel {
	public:
		struct Split {
			enum State { Open, Reached, Skipped };

			std::string name;
			State state;
			bool manual;
			unsigned long long realMs;		// from the run's start, once reached
			unsigned long long gameMs;		// the part of that spent in a game
			unsigned int level;				// the character's, as it was reached
		};

		struct Run {
			std::string route;
			std::string name;
			std::vector<Split> splits;
			unsigned long long startedAt;
			unsigned long long startedPlayedMs;	// time in games when it began
			bool ended;
			bool finished;
			unsigned long long realMs;		// how long it ran, once it has ended
			unsigned long long gameMs;
		};

		RunModel();

		void Apply(const PlayEvents::Event& event);

		bool InGame() const { return inGame; }

		// Of the game being played, or the one last played.
		unsigned int Difficulty() const { return difficulty; }
		unsigned int Level() const { return level; }

		// Every run there has been, the one in hand last if there is one.
		const std::vector<Run>& Runs() const { return runs; }
		const Run* ActiveRun() const;

		// How long the run in hand has gone, and the part of that spent in a
		// game. 0 where there is none.
		unsigned long long RunRealMs(unsigned long long now) const;
		unsigned long long RunGameMs(unsigned long long now) const;

		// A run on a route that has not begun: every split open and no time on
		// either clock. What a route looks like before it is run.
		static Run Unstarted(const std::string& route, const std::string& name,
			const std::vector<std::string>& splits);

	private:
		unsigned long long lastAt;			// of the latest event applied
		unsigned long long playedMs;		// of games that have ended
		bool inGame;
		unsigned long long joinedAt;
		unsigned int difficulty;
		unsigned int level;
		std::vector<Run> runs;

		Run* Active();
		unsigned long long PlayedMs(unsigned long long now) const;
		void CloseGame(unsigned long long at);
		void EndRun(unsigned long long at, bool finished);

		void On(const PlayEvents::Joined& fact, unsigned long long at);
		void On(const PlayEvents::Left& fact, unsigned long long at);
		void On(const PlayEvents::LevelReached& fact, unsigned long long at);
		void On(const PlayEvents::RunStarted& fact, unsigned long long at);
		void On(const PlayEvents::SplitReached& fact, unsigned long long at);
		void On(const PlayEvents::SplitUndone& fact, unsigned long long at);
		void On(const PlayEvents::SplitSkipped& fact, unsigned long long at);
		void On(const PlayEvents::RunEnded& fact, unsigned long long at);

		// Everything else that can happen is nothing to a run's clocks.
		template <typename Other>
		void On(const Other& fact, unsigned long long at) {}
};
