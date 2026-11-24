#pragma once

// How long ago the player took a town portal, and whether that still holds the
// next one shut.
//
// The client refuses to take a town portal while the player carries state 102,
// just_portaled, which the server sets as a portal is taken. Times are
// GetTickCount() milliseconds, which wrap every 49.7 days; every comparison is a
// difference of two ticks, so a wrap between them changes nothing.
namespace PortalDelay {

// Whether a town portal is refused: only while the state is held and less than
// the delay has passed since it was gained.
bool Refuses(bool hasState, unsigned int msSinceGained, unsigned int delay);

// Milliseconds from one tick to a later one.
unsigned int Since(unsigned int then, unsigned int now);

// When the player gained the state, from two sources: the server's packet
// setting it, which is the moment the portal was taken, and the player's unit
// seen carrying it, for a state that arrives some other way.
class Clock {
	public:
		Clock();

		// The server set the state on the player. Restarts the clock even while the
		// state is held, since that is another portal taken.
		void Gained(unsigned int now);

		// Whether the player carries the state, checked once per game loop. A state
		// that appears without Gained() is timed from when it was first seen, and
		// one that is gone stops the clock.
		void Observe(bool hasState, unsigned int now);

		void Reset();

		// A state that is held but was never timed is refused, as the game would.
		bool Refuses(bool hasState, unsigned int now, unsigned int delay) const;

	private:
		bool timed;
		unsigned int gainedAt;
		bool seen;		// what the last Observe() was told
};

}
