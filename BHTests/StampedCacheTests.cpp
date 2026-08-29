#include "doctest.h"
#include <string>
#include "StampedCache.h"

/*
 * Keeping what was worked out about a thing.
 *
 * The cache is asked about numbers rather than about items, so what it does can
 * be checked without a game: a key, a stamp saying what state the thing was in,
 * and whatever was worked out about it.
 */

namespace {

// Enough of a value to tell whether the one handed back is the one held, and to
// tell a value that was filled in later from one that was not.
struct Verdict {
	std::string name;
	bool named;

	Verdict() : named(false) {}
};

}  // namespace

TEST_CASE("what was held comes back") {
	StampedCache<Verdict> cache(4);
	Verdict held;
	held.name = "Ort Rune";
	cache.Hold(7, 0x11, held);

	Verdict *found = cache.Find(7, 0x11);
	REQUIRE(found != NULL);
	CHECK(found->name == "Ort Rune");
}

TEST_CASE("nothing is held about a thing never seen") {
	StampedCache<Verdict> cache(4);
	CHECK(cache.Find(7, 0x11) == NULL);
}

TEST_CASE("what was worked out about another state of the thing is not held") {
	StampedCache<Verdict> cache(4);
	Verdict held;
	held.name = "Ring";
	cache.Hold(7, 0x11, held);

	// The item changed - it was identified, socketed, made into a runeword -
	// so what was worked out about it before says nothing about it now.
	CHECK(cache.Find(7, 0x12) == NULL);
}

TEST_CASE("holding again replaces what was held") {
	StampedCache<Verdict> cache(4);
	Verdict first;
	first.name = "Ring";
	cache.Hold(7, 0x11, first);

	Verdict second;
	second.name = "Nagelring";
	cache.Hold(7, 0x12, second);

	CHECK(cache.Size() == 1);
	CHECK(cache.Find(7, 0x11) == NULL);
	Verdict *found = cache.Find(7, 0x12);
	REQUIRE(found != NULL);
	CHECK(found->name == "Nagelring");
}

TEST_CASE("what is held can be filled in later") {
	StampedCache<Verdict> cache(4);
	cache.Hold(7, 0x11, Verdict());

	Verdict *found = cache.Find(7, 0x11);
	REQUIRE(found != NULL);
	REQUIRE(found->named == false);
	found->name = "Ort Rune";
	found->named = true;

	// The half that was worked out later is there the next time, so nothing
	// asks for it twice.
	Verdict *again = cache.Find(7, 0x11);
	REQUIRE(again != NULL);
	CHECK(again->named == true);
	CHECK(again->name == "Ort Rune");
}

TEST_CASE("the least recently asked for is dropped when there is no room") {
	StampedCache<Verdict> cache(2);
	cache.Hold(1, 0, Verdict());
	cache.Hold(2, 0, Verdict());
	cache.Hold(3, 0, Verdict());

	CHECK(cache.Size() == 2);
	CHECK(cache.Find(1, 0) == NULL);
	CHECK(cache.Find(2, 0) != NULL);
	CHECK(cache.Find(3, 0) != NULL);
}

TEST_CASE("asking about a thing keeps it") {
	StampedCache<Verdict> cache(2);
	cache.Hold(1, 0, Verdict());
	cache.Hold(2, 0, Verdict());

	// Asked about again, so the next one to be dropped is the other.
	REQUIRE(cache.Find(1, 0) != NULL);
	cache.Hold(3, 0, Verdict());

	CHECK(cache.Find(1, 0) != NULL);
	CHECK(cache.Find(2, 0) == NULL);
}

TEST_CASE("a thing asked about in a state it is no longer in is still dropped first") {
	// A miss on the stamp is not an ask: nothing was handed back, so nothing
	// about the entry is worth keeping over one that was read.
	StampedCache<Verdict> cache(2);
	cache.Hold(1, 0x11, Verdict());
	cache.Hold(2, 0x11, Verdict());

	CHECK(cache.Find(1, 0x99) == NULL);
	cache.Hold(3, 0x11, Verdict());

	CHECK(cache.Find(1, 0x11) == NULL);
	CHECK(cache.Find(2, 0x11) != NULL);
}

TEST_CASE("forgetting one thing leaves the rest") {
	StampedCache<Verdict> cache(4);
	cache.Hold(1, 0, Verdict());
	cache.Hold(2, 0, Verdict());

	cache.Forget(1);

	CHECK(cache.Size() == 1);
	CHECK(cache.Find(1, 0) == NULL);
	CHECK(cache.Find(2, 0) != NULL);
}

TEST_CASE("what is guarded is not dropped for what is not") {
	StampedCache<Verdict> cache(2);
	cache.Hold(1, 0, Verdict());
	cache.Protect(1);
	cache.Hold(2, 0, Verdict());

	// Nothing was asked about 1 since, so on recency alone it would go first.
	cache.Hold(3, 0, Verdict());

	CHECK(cache.Find(1, 0) != NULL);
	CHECK(cache.Find(2, 0) == NULL);
	CHECK(cache.Find(3, 0) != NULL);
}

TEST_CASE("a flood of what is cheap leaves what is dear alone") {
	// The automap asks about far more items than are ever named, and this is
	// what stops it working every name out again on every frame.
	StampedCache<Verdict> cache(8);
	cache.Hold(1, 0, Verdict());
	cache.Protect(1);

	for (DWORD other = 100; other < 200; other++)
		cache.Hold(other, 0, Verdict());

	CHECK(cache.Find(1, 0) != NULL);
	CHECK(cache.GuardedSize() == 1);
}

TEST_CASE("what is guarded is dropped once nothing else is left") {
	StampedCache<Verdict> cache(2);
	cache.Hold(1, 0, Verdict());
	cache.Protect(1);
	cache.Hold(2, 0, Verdict());
	cache.Protect(2);

	cache.Hold(3, 0, Verdict());

	// Which is what a cache with no notion of this does to everything.
	CHECK(cache.Size() == 2);
	CHECK(cache.Find(1, 0) == NULL);
	CHECK(cache.Find(2, 0) != NULL);
	CHECK(cache.Find(3, 0) != NULL);
}

TEST_CASE("asking about a guarded thing keeps it over another guarded one") {
	StampedCache<Verdict> cache(2);
	cache.Hold(1, 0, Verdict());
	cache.Protect(1);
	cache.Hold(2, 0, Verdict());
	cache.Protect(2);

	REQUIRE(cache.Find(1, 0) != NULL);
	cache.Hold(3, 0, Verdict());

	CHECK(cache.Find(1, 0) != NULL);
	CHECK(cache.Find(2, 0) == NULL);
}

TEST_CASE("holding again stops guarding what was held before") {
	// The item changed, so the dear half has to be worked out again, and until
	// it is there is nothing dear about what is held.
	StampedCache<Verdict> cache(4);
	cache.Hold(1, 0x11, Verdict());
	cache.Protect(1);
	REQUIRE(cache.GuardedSize() == 1);

	cache.Hold(1, 0x12, Verdict());

	CHECK(cache.GuardedSize() == 0);
	CHECK(cache.Size() == 1);
}

TEST_CASE("guarding a thing not held does nothing") {
	StampedCache<Verdict> cache(4);
	cache.Protect(7);
	CHECK(cache.Size() == 0);
	CHECK(cache.GuardedSize() == 0);
}

TEST_CASE("forgetting a guarded thing leaves the rest") {
	StampedCache<Verdict> cache(4);
	cache.Hold(1, 0, Verdict());
	cache.Protect(1);
	cache.Hold(2, 0, Verdict());

	cache.Forget(1);

	CHECK(cache.Size() == 1);
	CHECK(cache.GuardedSize() == 0);
	CHECK(cache.Find(2, 0) != NULL);
}

TEST_CASE("clearing holds nothing") {
	StampedCache<Verdict> cache(4);
	cache.Hold(1, 0, Verdict());
	cache.Hold(2, 0, Verdict());

	cache.Protect(1);
	cache.Clear();

	CHECK(cache.Size() == 0);
	CHECK(cache.GuardedSize() == 0);
	CHECK(cache.Find(1, 0) == NULL);
	CHECK(cache.Find(2, 0) == NULL);
}
