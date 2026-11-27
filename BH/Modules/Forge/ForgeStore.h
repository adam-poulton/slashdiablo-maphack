#pragma once
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "../../Drawing/MapIcon/MapIconStyle.h"

// What the Forge styles, as the text of the file it is kept in.
//
// The file is a JSON object holding an object per tab, an object per marker in
// each, and the marker's fields in that. A tab can hold values of its own beside
// its markers:
//
//   { "version": 1, "chests": { "locked": { "icon": "diamond", "fill": "0x97" } },
//     "monsters": { "boss": { "size": 5 }, "rules": [ { "minLevel": 85 } ] } }
//
// Only a marker that differs from its defaults is in the file, and it is written
// with all of its fields, so a marker that was set stays as it was set when a
// default changes. A colour is a palette entry, written "0x97", or "none".
//
// A module binds each value of each of its markers as it reads its config. The
// value is read from the document as it is bound, and is what is written back
// on a save. Whatever the document holds that nothing has bound, such as a tab
// or a field this version does not know, is kept as it was.
class ForgeStore {
	public:
		ForgeStore();
		~ForgeStore();

		// Replaces the document with the file's text, and forgets every binding,
		// since the modules bind again as they read their config again. Text of
		// nothing but whitespace is a file with nothing in it.
		//
		// Answers false where the text is not the file's shape, leaving the
		// document empty, so that everything reads as its default; error then
		// says where in the text it went wrong.
		bool Load(const std::string& text, std::string* error = NULL);

		// A marker's style, as fields named icon, size, fill, border,
		// borderWidth, line and lineColor.
		void BindStyle(const std::string& tab, const std::string& marker,
			MapIconStyle* style, const MapIconStyle& defaults);

		// One of a list, kept as its position and written as its name. Names are
		// matched whatever their case.
		void BindNamed(const std::string& tab, const std::string& marker,
			const std::string& field, unsigned int* value,
			const std::vector<std::string>& names, unsigned int defaultValue);

		// A value of the tab's own, written beside its markers, in whatever shape
		// its owner keeps it: a list, say. write gives it as JSON text, and read
		// takes it back as JSON text, or NULL where the file has none, which is
		// the value at its default. It is in the file only while isDefault says
		// it is not.
		void BindValue(const std::string& tab, const std::string& field,
			std::function<std::string()> write,
			std::function<void(const std::string*)> read,
			std::function<bool()> isDefault);

		// Whether any bound value differs from what it was read or last saved as.
		bool HasChanges() const;

		// What to write: the file's text as it is now, with each marker that
		// changed put in, or taken out where it is back to its defaults. Answers
		// false where that text is not the file's shape, which would be lost if
		// it were written over; error then says where in it it went wrong.
		bool SaveOnto(const std::string& onDisk, std::string& text,
			std::string* error = NULL) const;

		// Takes what SaveOnto() gave as the file, once it has been written.
		void MarkSaved(const std::string& text);

	private:
		struct Impl;
		std::unique_ptr<Impl> impl;
};
