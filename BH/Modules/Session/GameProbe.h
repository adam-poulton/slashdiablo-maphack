#pragma once
#include <map>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>
#include "SessionEvents.h"

struct UnitAny;

// The only part of the session tracker that reads the game. Looks at the player
// once per game loop and says what changed since it last looked.
//
// What it says is stamped with the tick it was seen at rather than with a
// session's time, because whose session a game belongs to is not known until the
// character in it is.
class GameProbe {
	public:
		struct Seen {
			unsigned long long tick;	// BHGetTickCount()
			SessionEvents::Fact fact;
		};

		// Who is playing, read as a game is joined.
		struct Identity {
			std::string account;
			std::string realm;
			std::string character;
			unsigned int classId;
			bool hardcore;
			bool expansion;
		};

		GameProbe();

		// Called from the game loop. The first thing seen of a game is Joined,
		// and GetIdentity() answers for that game from then on.
		void Sample(unsigned long long tick, std::vector<Seen>& seen);

		// The game is over. Reads nothing from the game, which may already be
		// gone: says what was last sampled and not yet said, then Left.
		void Leave(unsigned long long tick, std::vector<Seen>& seen);

		const Identity& GetIdentity() const { return identity; }

		// What the game calls an area. On the game's thread.
		static std::string AreaName(unsigned int area);

	private:
		// How long an experience total, or a count of kills, is held back, so
		// that a pack dying in a second is one event and not twenty.
		static constexpr unsigned long long kTellEveryMs = 1000;

		// A monster the probe has seen, for as long as the game lasts. A kill is
		// one seen alive and then dead, and only the once: what a shaman raises
		// again was already killed.
		struct Watched {
			bool alive;
			bool friendly;		// fought for the player when it was first seen
			bool counted;
		};

		// Kills not yet told: how many, of what kind of monster, where.
		struct Tally {
			std::string name;
			unsigned int count;
		};
		typedef std::tuple<unsigned int, unsigned int, SessionEvents::Tier> TallyKey;	// area, class, tier

		bool present;					// the player has been seen in this game
		bool joined;					// and Joined has been said
		unsigned long long firstSeenAt;
		Identity identity;

		unsigned int level;
		unsigned long long experience;	// as sampled
		unsigned long long toldExperience;
		unsigned long long toldExperienceAt;
		unsigned int area;
		unsigned int players;
		bool seenAlive;
		bool dead;

		std::unordered_map<unsigned int, Watched> monsters;		// by unit id
		std::map<TallyKey, Tally> kills;
		unsigned long long toldKillsAt;

		void Reset();
		void TellExperience(unsigned long long tick, std::vector<Seen>& seen);
		void TellKills(unsigned long long tick, std::vector<Seen>& seen);
		void WatchMonsters(UnitAny* player, unsigned long long tick, std::vector<Seen>& seen);
		void CountKill(UnitAny* monster, unsigned long long tick, std::vector<Seen>& seen);
};
