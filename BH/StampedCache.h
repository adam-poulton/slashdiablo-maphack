#pragma once
#include <list>
#include <unordered_map>
#include <utility>
#include <windows.h>

/*
 * What was worked out about a thing, kept for as long as the thing has not
 * changed.
 *
 * Two numbers say what is held: which thing it was worked out about, and what
 * state that thing was in at the time. An item is the thing, its unit id is the
 * first and its flags are the second, so that identifying an item, socketing it
 * or making a runeword out of it throws away what was worked out about it
 * before. Neither number means anything here. Reading them off an item is the
 * caller's business, which is what keeps this able to be built and tested
 * without a game.
 *
 * What is held is handed back to be written to. A value only part of which was
 * wanted can have the rest filled in when something asks for it, rather than
 * everything being worked out the first time the item is seen: naming an item
 * on the ground and drawing it on the automap want different halves of the same
 * answer, and the automap asks about far more items.
 *
 * What is dropped when there is no room is the least recently asked for of the
 * ones not being guarded. Guarding is how a caller says that what it just
 * filled in was dear to work out: what the automap wants about an item is a
 * short walk, where a name is a walk of every rule that names anything and a
 * trip out to the game for a price and a required level. There are far more
 * items on a map than there are on a screen, so without this the cheap and
 * numerous would push out the dear and few, and the names on screen would be
 * worked out again on every frame. Guarded entries are dropped only once there
 * is nothing else left to drop, which is what a cache with no such notion does
 * to everything.
 */
template <typename Value>
class StampedCache {
public:
	explicit StampedCache(unsigned int capacity)
		: capacity(capacity < 1 ? 1 : capacity) {}

	/*
	 * What is held about key, or null.
	 *
	 * Null both when nothing is held and when what is held was worked out about
	 * a different state of the thing. The caller has the same answer to both:
	 * work it out.
	 */
	Value *Find(DWORD key, DWORD stamp) {
		typename Index::iterator found = index.find(key);
		if (found == index.end() || found->second->stamp != stamp)
			return NULL;
		Entries &list = ListOf(*found->second);
		list.splice(list.begin(), list, found->second);
		return &found->second->value;
	}

	// Holds value about key, in place of anything held about it before. What is
	// newly held is not guarded, whatever was held about key before.
	Value &Hold(DWORD key, DWORD stamp, Value value) {
		Forget(key);
		while (index.size() >= capacity)
			DropOne();
		unguarded.push_front(Entry{ key, stamp, false, std::move(value) });
		index[key] = unguarded.begin();
		return unguarded.begin()->value;
	}

	/*
	 * Says that what is held about key was dear to work out.
	 *
	 * Called once the caller has filled in the expensive half, not when the
	 * entry is made, since until then there is nothing dear about it.
	 */
	void Protect(DWORD key) {
		typename Index::iterator found = index.find(key);
		if (found == index.end() || found->second->guarded)
			return;
		found->second->guarded = true;
		guarded.splice(guarded.begin(), unguarded, found->second);
	}

	// Drops what is held about key, if anything is.
	void Forget(DWORD key) {
		typename Index::iterator found = index.find(key);
		if (found == index.end())
			return;
		ListOf(*found->second).erase(found->second);
		index.erase(found);
	}

	void Clear() {
		unguarded.clear();
		guarded.clear();
		index.clear();
	}

	unsigned int Size() const { return (unsigned int)index.size(); }
	unsigned int GuardedSize() const { return (unsigned int)guarded.size(); }

private:
	struct Entry {
		DWORD key;
		DWORD stamp;
		bool guarded;
		Value value;
	};

	typedef std::list<Entry> Entries;
	typedef std::unordered_map<DWORD, typename Entries::iterator> Index;

	Entries &ListOf(const Entry &entry) {
		return entry.guarded ? guarded : unguarded;
	}

	// Only ever called with something to drop, since holding drops before it
	// makes room rather than after.
	void DropOne() {
		Entries &list = unguarded.empty() ? guarded : unguarded;
		index.erase(list.back().key);
		list.pop_back();
	}

	// Each most recently asked for first, so that what is dropped is at the
	// back of whichever is being drawn from.
	Entries unguarded;
	Entries guarded;
	Index index;
	unsigned int capacity;
};
