#include "ForgeStore.h"
#include "ForgeCodec.h"
#include "../../WholeFile.h"

// Ordered, so a marker's fields are written in the order they are bound, which
// is the order they are edited in.
typedef ForgeCodec::Json Json;
using ForgeCodec::Lower;
using ForgeCodec::Trimmed;

namespace {
	// What the file says of itself. A later version's file is read for what this
	// one understands, and keeps its number when written back.
	const unsigned int kFileVersion = 1;
	const char* const kVersionKey = "version";

	// Where a byte of the text is, as an editor counts it: lines and columns from
	// one.
	std::string Position(const std::string& text, size_t at) {
		unsigned int line = 1, column = 1;
		for (size_t i = 0; i < at && i < text.size(); i++) {
			if (text[i] == '\n') {
				line++;
				column = 1;
			} else {
				column++;
			}
		}
		return "line " + std::to_string(line) + ", column " + std::to_string(column);
	}

	// Strict JSON, comments refused: a comment would be read and then lost the
	// next time the Forge saved.
	bool Parse(const std::string& text, Json& into, std::string* error) {
		into = Json::object();
		if (WholeFile::IsBlank(text))
			return true;
		Json parsed;
		try {
			parsed = Json::parse(text);
		} catch (const Json::parse_error& e) {
			// The byte is the one the parser stopped on, counted from one.
			if (error)
				*error = Position(text, e.byte > 0 ? e.byte - 1 : 0);
			return false;
		} catch (const Json::exception&) {
			if (error)
				*error = Position(text, 0);
			return false;
		}
		if (!parsed.is_object()) {
			if (error)
				*error = Position(text, text.find_first_not_of(" \t\r\n"));
			return false;
		}
		into = parsed;
		return true;
	}

	struct Field {
		std::string name;
		std::function<Json()> encode;
		// Given the stored value, or NULL where there is none, and sets the
		// variable from it or to its default.
		std::function<void(const Json*)> decode;
		std::function<bool()> isDefault;
		Json saved;		// what the value was read or last saved as
	};

	// A marker with no id holds the tab's own values, which are written beside
	// its markers rather than inside one.
	struct Marker {
		std::string tab;
		std::string id;
		std::vector<Field> fields;

		bool Changed() const {
			for (const Field& field : fields)
				if (field.encode() != field.saved)
					return true;
			return false;
		}

		bool AtDefaults() const {
			for (const Field& field : fields)
				if (!field.isDefault())
					return false;
			return true;
		}
	};

	template <typename T>
	Field MakeField(const std::string& name, T* value, T defaultValue,
			std::function<Json(T)> encode, std::function<bool(const Json&, T&)> decode) {
		Field field;
		field.name = name;
		field.encode = [value, encode]() { return encode(*value); };
		field.decode = [value, defaultValue, decode](const Json* stored) {
			T read = defaultValue;
			*value = (stored && decode(*stored, read)) ? read : defaultValue;
		};
		field.isDefault = [value, defaultValue]() { return *value == defaultValue; };
		return field;
	}

	const Json* Member(const Json& object, const std::string& key) {
		if (!object.is_object())
			return NULL;
		Json::const_iterator found = object.find(key);
		return found == object.end() ? NULL : &*found;
	}

	const Json* Stored(const Json& document, const std::string& tab,
			const std::string& marker, const std::string& field) {
		const Json* inTab = Member(document, tab);
		if (marker.empty())
			return inTab ? Member(*inTab, field) : NULL;
		const Json* inMarker = inTab ? Member(*inTab, marker) : NULL;
		return inMarker ? Member(*inMarker, field) : NULL;
	}

	Field ColorField(const std::string& name, unsigned int* value,
			unsigned int defaultValue, bool allowNone) {
		return MakeField<unsigned int>(name, value, defaultValue, ForgeCodec::EncodeColor,
			[allowNone](const Json& stored, unsigned int& color) {
				return ForgeCodec::DecodeColor(stored, allowNone, color);
			});
	}

	Field CountField(const std::string& name, unsigned int* value,
			unsigned int defaultValue, unsigned int min, unsigned int max) {
		return MakeField<unsigned int>(name, value, defaultValue,
			[](unsigned int count) { return Json(count); },
			[min, max](const Json& stored, unsigned int& count) {
				return ForgeCodec::DecodeCount(stored, min, max, count);
			});
	}

	Field BoolField(const std::string& name, bool* value, bool defaultValue) {
		return MakeField<bool>(name, value, defaultValue,
			[](bool on) { return Json(on); },
			[](const Json& stored, bool& on) {
				if (!stored.is_boolean())
					return false;
				on = stored.get<bool>();
				return true;
			});
	}

	Field NamedField(const std::string& name, unsigned int* value,
			unsigned int defaultValue, const std::vector<std::string>& names) {
		return MakeField<unsigned int>(name, value, defaultValue,
			[names, defaultValue](unsigned int index) {
				if (index < names.size())
					return Json(names[index]);
				return Json(defaultValue < names.size() ? names[defaultValue] : "");
			},
			[names](const Json& stored, unsigned int& index) {
				if (!stored.is_string())
					return false;
				std::string text = Lower(Trimmed(stored.get<std::string>()));
				for (unsigned int i = 0; i < names.size(); i++) {
					if (Lower(names[i]).compare(text) == 0) {
						index = i;
						return true;
					}
				}
				return false;
			});
	}

	void Put(Json& file, const Marker& marker) {
		Json& tab = file[marker.tab];
		if (!tab.is_object())
			tab = Json::object();
		Json* entry = &tab;
		if (!marker.id.empty()) {
			entry = &tab[marker.id];
			if (!entry->is_object())
				*entry = Json::object();
		}
		for (const Field& field : marker.fields)
			(*entry)[field.name] = field.encode();
	}

	// Only the fields bound here: one this version does not know is kept, and
	// keeps the marker in the file with it.
	void Remove(Json& file, const Marker& marker) {
		Json::iterator tab = file.find(marker.tab);
		if (tab == file.end() || !tab->is_object())
			return;
		if (marker.id.empty()) {
			for (const Field& field : marker.fields)
				tab->erase(field.name);
			if (tab->empty())
				file.erase(marker.tab);
			return;
		}
		Json::iterator entry = tab->find(marker.id);
		if (entry == tab->end() || !entry->is_object())
			return;
		for (const Field& field : marker.fields)
			entry->erase(field.name);
		if (entry->empty())
			tab->erase(marker.id);
		if (tab->empty())
			file.erase(marker.tab);
	}
}

struct ForgeStore::Impl {
	Json document;
	std::vector<Marker> markers;

	Impl() : document(Json::object()) {}

	// A field bound again replaces the one bound before it, as a reload binds
	// every field again.
	void Bind(const std::string& tab, const std::string& id, Field field) {
		field.decode(Stored(document, tab, id, field.name));
		field.saved = field.encode();

		Marker* marker = NULL;
		for (Marker& existing : markers)
			if (existing.tab.compare(tab) == 0 && existing.id.compare(id) == 0)
				marker = &existing;
		if (!marker) {
			markers.push_back(Marker{ tab, id, {} });
			marker = &markers.back();
		}
		for (Field& existing : marker->fields) {
			if (existing.name.compare(field.name) == 0) {
				existing = field;
				return;
			}
		}
		marker->fields.push_back(field);
	}
};

ForgeStore::ForgeStore() : impl(new Impl()) {
}

ForgeStore::~ForgeStore() {
}

bool ForgeStore::Load(const std::string& text, std::string* error) {
	impl->markers.clear();
	return Parse(text, impl->document, error);
}

void ForgeStore::BindStyle(const std::string& tab, const std::string& marker,
		MapIconStyle* style, const MapIconStyle& defaults) {
	std::vector<std::string> icons;
	for (int i = 0; i < MapIcon::IconCount; i++)
		icons.push_back(MapIcon::Name((MapIcon::Icon)i));

	impl->Bind(tab, marker, NamedField("icon", &style->icon, defaults.icon, icons));
	impl->Bind(tab, marker, CountField("size", &style->size, defaults.size,
		MapIcon::MinSize, MapIcon::MaxSize));
	impl->Bind(tab, marker, ColorField("fill", &style->fillColor, defaults.fillColor, true));
	impl->Bind(tab, marker, ColorField("border", &style->borderColor, defaults.borderColor, true));
	impl->Bind(tab, marker, CountField("borderWidth", &style->borderWidth,
		defaults.borderWidth, MapIcon::MinBorder, MapIcon::MaxBorder));
	impl->Bind(tab, marker, BoolField("line", &style->line, defaults.line));
	impl->Bind(tab, marker, ColorField("lineColor", &style->lineColor, defaults.lineColor, false));
}

void ForgeStore::BindNamed(const std::string& tab, const std::string& marker,
		const std::string& field, unsigned int* value,
		const std::vector<std::string>& names, unsigned int defaultValue) {
	impl->Bind(tab, marker, NamedField(field, value, defaultValue, names));
}

void ForgeStore::BindValue(const std::string& tab, const std::string& field,
		std::function<std::string()> write,
		std::function<void(const std::string*)> read,
		std::function<bool()> isDefault) {
	Field bound;
	bound.name = field;
	bound.encode = [write]() {
		Json value = Json::parse(write(), NULL, false);
		return value.is_discarded() ? Json() : value;
	};
	bound.decode = [read](const Json* stored) {
		if (!stored) {
			read(NULL);
			return;
		}
		std::string text = stored->dump();
		read(&text);
	};
	bound.isDefault = isDefault;
	impl->Bind(tab, std::string(), bound);
}

bool ForgeStore::HasChanges() const {
	for (const Marker& marker : impl->markers)
		if (marker.Changed())
			return true;
	return false;
}

bool ForgeStore::SaveOnto(const std::string& onDisk, std::string& text,
		std::string* error) const {
	Json file;
	if (!Parse(onDisk, file, error))
		return false;

	// Only what changed here: a marker this client left alone may be one another
	// client has just saved.
	for (const Marker& marker : impl->markers) {
		if (!marker.Changed())
			continue;
		if (marker.AtDefaults())
			Remove(file, marker);
		else
			Put(file, marker);
	}

	// The version first, where it reads as what the rest of the file is.
	unsigned int version = kFileVersion;
	const Json* stored = Member(file, kVersionKey);
	if (stored && stored->is_number_unsigned() &&
			stored->get<unsigned long long>() > kFileVersion)
		version = (unsigned int)stored->get<unsigned long long>();
	Json written = Json::object();
	written[kVersionKey] = version;
	for (Json::iterator it = file.begin(); it != file.end(); ++it)
		if (it.key().compare(kVersionKey) != 0)
			written[it.key()] = *it;

	text = written.dump(2) + "\n";
	return true;
}

void ForgeStore::MarkSaved(const std::string& text) {
	Parse(text, impl->document, NULL);
	for (Marker& marker : impl->markers)
		for (Field& field : marker.fields)
			field.saved = field.encode();
}
