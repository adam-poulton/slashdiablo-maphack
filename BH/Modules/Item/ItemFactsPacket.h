#pragma once
#include <string>

struct ItemAttributes;
struct ItemFacts;
struct ItemProperty;
struct StatProperties;
class BitReader;

/*
 * Reads an item out of an incoming 0x9c packet.
 *
 * A packet says what an item is in as few bits as it can, and how many bits
 * each part takes depends on the game's own data tables. Those tables and the
 * reporting of a packet that cannot be read through are asked for rather than
 * reached for, which is what lets a packet be read with no game running and no
 * archives loaded: a test supplies both.
 */
namespace ItemFactsPacket {

	// What reading a packet needs to know that the packet does not say.
	struct Tables {
		virtual ~Tables() {}

		// The attributes of the item with this code, or null when the code is
		// not one the tables describe.
		virtual ItemAttributes* Attributes(const char* code) const = 0;

		// How a stat is written into a packet, or null when the stat is beyond
		// what the tables describe.
		virtual StatProperties* Stat(unsigned int stat) const = 0;
	};

	// Where a packet that did not read cleanly reports itself. Whether any of
	// it is worth saying is the caller's to decide, so all of it is reported.
	struct Diagnostics {
		virtual ~Diagnostics() {}

		// An item code the tables do not describe.
		virtual void UnknownItemCode(const char* code) {}

		// A stat the tables cannot say the width of, so the rest of the packet
		// can no longer be placed.
		virtual void UnreadableStat(unsigned int stat, const char* code) {}

		// Reading gave out part way through.
		virtual void Failed(const char* code, const std::string& reason) {}
	};

	class Reader {
	public:
		/*
		 * stopOnUnreadableStat decides what a stat of unknown width does. Set,
		 * the item is abandoned, which is the honest answer: nothing after such
		 * a stat can be placed. Clear, the stat is kept as it was read and
		 * reading carries on, which is what the "Suppress Invalid Stats"
		 * setting asks for and why that setting changes what is filtered rather
		 * than only what is said about it.
		 */
		Reader(const Tables& tables, Diagnostics& diagnostics,
				bool stopOnUnreadableStat)
			: tables(tables), diagnostics(diagnostics),
			  stopOnUnreadableStat(stopOnUnreadableStat) {}

		// Reads one packet. False when it did not read cleanly, in which case
		// what was read before it gave out is still in item.
		bool Read(const unsigned char* packet, ItemFacts* item) const;

	private:
		bool ReadStat(unsigned int stat, BitReader& reader,
				ItemProperty& property) const;

		// The widths a stat is written in. A stat the tables cannot describe
		// leaves nothing to read, and throws rather than returning a width that
		// would silently put every later field in the wrong place.
		const StatProperties& Widths(unsigned int stat) const;

		const Tables& tables;
		Diagnostics& diagnostics;
		bool stopOnUnreadableStat;
	};
}
