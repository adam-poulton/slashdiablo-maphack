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
 * The least recently asked for is dropped once there are more than there is
 * room for.
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
		entries.splice(entries.begin(), entries, found->second);
		return &found->second->value;
	}

	// Holds value about key, in place of anything held about it before.
	Value &Hold(DWORD key, DWORD stamp, Value value) {
		Forget(key);
		entries.push_front(Entry{ key, stamp, std::move(value) });
		index[key] = entries.begin();
		while (index.size() > capacity) {
			index.erase(entries.back().key);
			entries.pop_back();
		}
		return entries.begin()->value;
	}

	// Drops what is held about key, if anything is.
	void Forget(DWORD key) {
		typename Index::iterator found = index.find(key);
		if (found == index.end())
			return;
		entries.erase(found->second);
		index.erase(found);
	}

	void Clear() {
		entries.clear();
		index.clear();
	}

	unsigned int Size() const { return (unsigned int)index.size(); }

private:
	struct Entry {
		DWORD key;
		DWORD stamp;
		Value value;
	};

	typedef std::list<Entry> Entries;
	typedef std::unordered_map<DWORD, typename Entries::iterator> Index;

	// Most recently asked for first, so that what is dropped is at the back.
	Entries entries;
	Index index;
	unsigned int capacity;
};
