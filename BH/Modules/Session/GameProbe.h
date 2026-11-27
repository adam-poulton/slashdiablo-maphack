#pragma once
#include <string>
#include <vector>
#include "SessionEvents.h"

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

	private:
		// How long an experience total is held back, so that a pack dying in a
		// second is one event and not twenty.
		static constexpr unsigned long long kExperienceEveryMs = 1000;

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

		void Reset();
		void TellExperience(unsigned long long tick, std::vector<Seen>& seen);
};
