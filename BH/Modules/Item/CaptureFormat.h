#pragma once
#include <map>
#include <string>

/*
 * The line format an item capture is written in, and read back out of.
 *
 * A capture is a text file of one record per line, so that a record can be
 * appended as an item lands and a file cut short by a crash still holds every
 * record written before it.
 *
 * A line opens with the record's type and continues with named fields, all
 * separated by tabs:
 *
 *   drop<TAB>code=rin<TAB>blocked=0<TAB>name=\xffc4Ring
 *
 * A field's name is everything before its first equals sign, so a value is free
 * to contain one. Values are escaped, which is what keeps a record on one line
 * and keeps the colour codes carried by an item's name from being written into
 * the file as raw control bytes.
 *
 * Both the game and the tests use this: the game writes captures through it and
 * the tests read fixtures back through the same escaping, so the two cannot
 * drift apart.
 */
namespace CaptureFormat {
	// Renders a value so that it survives a tab separated line.
	std::string Escape(const std::string& value);

	// Recovers a value written by Escape. Text that is not valid escaping is
	// kept as it stands rather than being discarded.
	std::string Unescape(const std::string& value);

	// A record being composed.
	class Record {
	public:
		explicit Record(const std::string& type) : out(type) {}

		void Add(const std::string& key, const std::string& value);
		void Add(const std::string& key, long long value);
		void Add(const std::string& key, bool value);

		// The finished line, without a line ending.
		std::string Line() const { return out; }

	private:
		std::string out;
	};

	// A record read back. Fields are keyed by name; a name appearing twice
	// keeps the last value, which is the same rule the config parser follows.
	struct Fields {
		std::string type;
		std::map<std::string, std::string> values;

		bool Has(const std::string& key) const;

		// Absent fields read as an empty string, zero and false, so a fixture
		// written before a field existed still reads.
		std::string Text(const std::string& key) const;
		long long Number(const std::string& key) const;
		bool Boolean(const std::string& key) const;
	};

	// Reads one line. A line that is empty or holds only whitespace comes back
	// with an empty type.
	Fields ParseLine(const std::string& line);
}
