#pragma once
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "../Tracking/PlayEvents.h"
#include "SplitRoute.h"

// Watches what happens in the game for a run in hand and says which of the
// route's splits it reaches.
//
// A split is reached when what marks it becomes so: the event that makes it so
// is what reaches it, and it is reached once. What was already so when the run
// began, or is still so after the player takes a split back, reaches nothing.
// Splits are reached in whatever order the game reaches them.
class RunEvaluator {
	public:
		RunEvaluator();

		// Begins a run against the route, for a character of this level in a
		// game of this difficulty.
		void Start(const Splits::Route& route, unsigned int difficulty, unsigned int level);
		void Stop();
		bool Running() const { return running; }

		const Splits::Route& GetRoute() const { return route; }

		// What an event reaches, as indices into the route's splits, in order.
		std::vector<unsigned int> Feed(const PlayEvents::Fact& fact);

		// A split dealt with by the player is not then reached by the game, and
		// one taken back can be.
		void Close(unsigned int index);
		void Reopen(unsigned int index);

		// Whether a split is done with: reached, dealt with by the player, or
		// already so when the run began.
		bool IsClosed(unsigned int index) const { return index < closed.size() && closed[index]; }

		// The first split not done with, or the number of splits where every
		// one is.
		unsigned int FirstOpen() const;

	private:
		// A thing that happened, in the difficulty it happened in.
		typedef std::pair<unsigned int, unsigned int> InDifficulty;

		bool running;
		Splits::Route route;
		std::vector<bool> closed;

		unsigned int difficulty;
		unsigned int level;
		std::map<InDifficulty, unsigned int> quests;	// by quest, the bits it has had set
		std::set<InDifficulty> killed;					// kinds of monster
		std::set<InDifficulty> superUniques;
		std::set<InDifficulty> areas;
		std::set<std::pair<unsigned int, std::string>> items;

		void Take(const PlayEvents::Fact& fact);
		bool Met(const Splits::Split& split) const;
		bool MetIn(const Splits::Trigger& trigger, unsigned int where) const;
		bool AllQuestsDone(unsigned int where) const;
};
