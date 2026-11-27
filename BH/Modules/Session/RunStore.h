#pragma once
#include <map>
#include <string>
#include <vector>
#include "SessionModel.h"

// The best a route has been run: the fastest finished run, and the fastest each
// stretch between two splits has ever been done, kept for each of the two clocks
// a run can be timed by.
//
// Nothing here opens a file. A store is read from text and written back to it,
// and whoever owns the file says when.
class RunStore {
	public:
		// The clock a run is compared on.
		enum Clock {
			RealTime,		// from the run's start, whatever happened in between
			GameTime,		// the part of that spent in a game
			Clocks
		};

		// -1 throughout for a time there is none of.
		struct Times {
			std::vector<long long> splits;		// the best finished run's, from its start
			long long total;
			std::vector<long long> segments;	// the best each stretch has been done in
		};

		struct Best {
			unsigned int attempts;
			unsigned int finished;
			Times by[Clocks];
		};

		// What a route's bests are kept under: the route, the class playing it
		// and whether the character is hardcore.
		static std::string KeyFor(const std::string& route, unsigned int classId, bool hardcore);

		const Best* Find(const std::string& key) const;

		// Takes a run that has ended into the bests it is kept under. A run that
		// was given up still counts as an attempt, and still sets a best for any
		// stretch it did faster than before.
		void Record(const std::string& key, const SessionModel::Run& run);

		bool Read(const std::string& text);
		std::string Write() const;

	private:
		std::map<std::string, Best> bests;
};

// A run set beside the best its route has been run, split by split.
namespace RunCompare {

struct Line {
	std::string name;
	SessionModel::RunSplit::State state;
	long long time;			// when it was reached, or -1
	long long best;			// when the best run reached it, or -1
	bool compared;			// whether there is a difference to give
	long long delta;		// time less best: negative is ahead
	bool gold;				// the stretch up to it was the fastest it has been done
	unsigned int level;
};

struct Lines {
	std::vector<Line> splits;
	int current;			// the first split still open, or -1
	long long clock;		// how long the run has gone, or went
	long long bestTotal;	// the best finished run, or -1
	long long sumOfBest;	// every stretch at its best, or -1 where one is not known
	long long bestPossible;	// what is left at its best, after what is done, or -1
};

// The stretch that ends at a split: from the split before it, or from the
// start for the first. -1 where either end was not reached, or they were
// reached out of order.
long long Segment(const SessionModel::Run& run, unsigned int index, RunStore::Clock clock);

// `now` is how long the run has gone on the clock asked for, which is what its
// own total is once it has ended.
Lines Compare(const SessionModel::Run& run, const RunStore::Best* best, RunStore::Clock clock,
	unsigned long long now);

}
