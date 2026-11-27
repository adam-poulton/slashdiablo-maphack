#pragma once
#include <deque>
#include <map>
#include <string>
#include <vector>
#include "SessionEvents.h"

// A session's totals, worked out from its events and from nothing else: it never
// asks the game or the clock anything. Whatever is still running when a figure is
// asked for (the session, the game, the stay in an area) is measured to the
// moment the caller gives, in the events' own milliseconds.
//
// An event log is not trusted to be well formed. A game left open by a client
// that died is closed at the last event the log holds, an event that belongs
// inside a game is dropped outside one, and time never runs backwards.
class SessionModel {
	public:
		// Monsters that died in view, by what kind of monster each was.
		struct KillCounts {
			unsigned int byTier[SessionEvents::TierCount];

			KillCounts() : byTier() {}
			unsigned int Total() const;

			// Unique monsters of every sort: unique, super unique and act boss.
			unsigned int Uniques() const;
		};

		struct Game {
			std::string name;
			unsigned int difficulty;
			unsigned long long joinedAt;
			unsigned long long leftAt;		// once it is no longer open
			bool open;
			unsigned int startLevel;
			unsigned int level;
			unsigned long long startExperience;
			unsigned long long experience;
			unsigned long long gained;
			unsigned long long lost;
			unsigned int deaths;
			unsigned int players;			// the most there were at once
			unsigned int kills;
			unsigned int drops;
		};

		struct Area {
			std::string name;				// empty where the log never gave one
			bool town;
			unsigned long long ms;			// of stays that have ended
			unsigned long long gained;
			unsigned int entries;
			KillCounts kills;
		};

		struct Monster {
			std::string name;
			KillCounts kills;
		};

		struct NotableKill {
			unsigned long long at;
			unsigned int game;				// its place in Games()
			unsigned int area;
			unsigned int classId;
			std::string name;
			SessionEvents::Tier tier;
			int superUnique;
		};

		struct Drop {
			unsigned long long at;
			unsigned int game;
			SessionEvents::Dropped item;
		};

		struct RunSplit {
			enum State { Open, Reached, Skipped };

			std::string name;
			State state;
			bool manual;
			unsigned long long realMs;		// from the run's start, once reached
			unsigned long long gameMs;		// the part of that spent in a game
			unsigned int level;				// the character's, as it was reached
		};

		// A run against a route: a clock from when it began and the splits
		// reached on the way.
		struct Run {
			std::string route;
			std::string name;
			std::vector<RunSplit> splits;
			unsigned long long startedAt;
			unsigned long long startedPlayedMs;	// time in games when it began
			bool ended;
			bool finished;
			unsigned long long realMs;		// how long it ran, once it has ended
			unsigned long long gameMs;
		};

		// How far back RecentRate() can be asked to look.
		static constexpr unsigned long long kRecentMs = 5 * 60 * 1000;

		SessionModel();

		void Apply(const SessionEvents::Event& event);

		bool HasStarted() const { return started; }
		const SessionEvents::Started& Header() const { return header; }

		const std::vector<Game>& Games() const { return games; }
		bool InGame() const { return !games.empty() && games.back().open; }

		// Time spent in games, and in the game being played. Time between games
		// is the session's but not the character's.
		unsigned long long PlayedMs(unsigned long long now) const;
		unsigned long long GameMs(unsigned long long now) const;

		unsigned long long Gained() const { return gained; }
		unsigned long long Lost() const { return lost; }
		unsigned int LevelsGained() const { return levelsGained; }
		unsigned int Deaths() const { return deaths; }

		// Experience per second, net of what deaths took: over the game being
		// played, over all the time spent in games, and over the session
		// including the time between them. 0 where no time has passed.
		double GameRate(unsigned long long now) const;
		double PlayedRate(unsigned long long now) const;
		double SessionRate(unsigned long long now) const;

		// The same over the last stretch of play, counting only the part of the
		// window that was spent in a game.
		double RecentRate(unsigned long long now, unsigned long long windowMs) const;

		const std::map<unsigned int, Area>& Areas() const { return areas; }

		// Time in an area, with the stay in progress if the character is there.
		unsigned long long AreaMs(unsigned int area, unsigned long long now) const;

		const KillCounts& KillTotals() const { return kills; }

		// By the monster's row in MonStats.txt.
		const std::map<unsigned int, Monster>& Monsters() const { return monsters; }

		// In the order they happened.
		const std::vector<NotableKill>& Notables() const { return notables; }
		const std::vector<Drop>& Drops() const { return drops; }

		// Every run of the session, the one in hand last if there is one.
		const std::vector<Run>& Runs() const { return runs; }
		const Run* ActiveRun() const;

		// How long the run in hand has gone, and the part of that spent in a
		// game. 0 where there is none.
		unsigned long long RunRealMs(unsigned long long now) const;
		unsigned long long RunGameMs(unsigned long long now) const;

		// The character's level as the session last had it.
		unsigned int Level() const { return games.empty() ? 0 : games.back().level; }

	private:
		struct Change {
			unsigned long long at;
			long long amount;
		};

		bool started;
		SessionEvents::Started header;
		std::vector<Game> games;
		std::map<unsigned int, Area> areas;

		unsigned long long lastAt;			// of the latest event applied
		unsigned long long playedMs;		// of games that have ended
		unsigned long long gained;
		unsigned long long lost;
		unsigned int levelsGained;
		unsigned int deaths;

		bool inArea;
		unsigned int area;
		unsigned long long enteredAt;

		std::deque<Change> recent;			// experience changes within kRecentMs

		KillCounts kills;
		std::map<unsigned int, Monster> monsters;
		std::vector<NotableKill> notables;
		std::vector<Drop> drops;
		std::vector<Run> runs;

		Run* Active();
		void EndRun(unsigned long long at, bool finished);

		void CountKills(unsigned int area, unsigned int classId, const std::string& name,
			SessionEvents::Tier tier, unsigned int count);
		void LeaveArea(unsigned long long at);
		void CloseGame(unsigned long long at);

		// Milliseconds of [from, to) that fell inside a game.
		unsigned long long PlayedBetween(unsigned long long from,
			unsigned long long to) const;

		void On(const SessionEvents::Started& fact, unsigned long long at);
		void On(const SessionEvents::Joined& fact, unsigned long long at);
		void On(const SessionEvents::Left& fact, unsigned long long at);
		void On(const SessionEvents::AreaEntered& fact, unsigned long long at);
		void On(const SessionEvents::Experience& fact, unsigned long long at);
		void On(const SessionEvents::LevelReached& fact, unsigned long long at);
		void On(const SessionEvents::Died& fact, unsigned long long at);
		void On(const SessionEvents::Players& fact, unsigned long long at);
		void On(const SessionEvents::Killed& fact, unsigned long long at);
		void On(const SessionEvents::Notable& fact, unsigned long long at);
		void On(const SessionEvents::Dropped& fact, unsigned long long at);
		void On(const SessionEvents::Quest& fact, unsigned long long at);
		void On(const SessionEvents::ItemHeld& fact, unsigned long long at);
		void On(const SessionEvents::RunStarted& fact, unsigned long long at);
		void On(const SessionEvents::SplitReached& fact, unsigned long long at);
		void On(const SessionEvents::SplitUndone& fact, unsigned long long at);
		void On(const SessionEvents::SplitSkipped& fact, unsigned long long at);
		void On(const SessionEvents::RunEnded& fact, unsigned long long at);
};
