#pragma once
#include "PlayEvents.h"

// What is made of a monster that died: whether it is a kill
// at all, and what kind of monster it was. Asked of what the game says about
// the monster, with no game needed to ask it.
namespace KillKinds {

// What MonStats.txt says of a monster's kind.
struct Kind {
	unsigned int classId;	// its row
	bool killable;
	bool npc;
	bool neverCount;		// not counted by the game itself: a wall, a bone prison
	unsigned int align;		// 0 for what fights the player
};

// Whether the death of one is a kill. The table settles nearly all of it, down
// to the barricades and the walls a Necromancer raises. Baal's tentacles and his
// clone are told apart by name: the table has them as monsters like any other,
// and they are part of the one fight, not kills of their own.
bool Counts(const Kind& kind);

// What the game marks an individual monster as.
struct Marks {
	bool primeEvil;			// of its kind, in MonStats.txt
	bool superUnique;
	bool unique;
	bool champion;
	bool minion;
};

// What it is. The game marks a champion unique as well as champion, and a super
// unique unique as well as super unique, so unique alone is what is left once
// neither of those is so.
PlayEvents::Tier TierOf(const Marks& marks);

}
