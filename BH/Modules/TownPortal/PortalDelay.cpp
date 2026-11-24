#include "PortalDelay.h"

namespace PortalDelay {

bool Refuses(bool hasState, unsigned int msSinceGained, unsigned int delay) {
	return hasState && msSinceGained < delay;
}

unsigned int Since(unsigned int then, unsigned int now) {
	return now - then;
}

Clock::Clock() : timed(false), gainedAt(0), seen(false) {}

void Clock::Gained(unsigned int now) {
	timed = true;
	gainedAt = now;
}

void Clock::Observe(bool hasState, unsigned int now) {
	// The packet is handled before the game applies the state it carries, so a
	// Gained() can come before the state shows. Only a state seen going away
	// stops the clock, never one not seen yet.
	if (hasState && !timed)
		Gained(now);
	else if (!hasState && seen)
		timed = false;
	seen = hasState;
}

void Clock::Reset() {
	timed = false;
	gainedAt = 0;
	seen = false;
}

bool Clock::Refuses(bool hasState, unsigned int now, unsigned int delay) const {
	if (!hasState)
		return false;
	if (!timed)
		return true;
	return PortalDelay::Refuses(hasState, Since(gainedAt, now), delay);
}

}
