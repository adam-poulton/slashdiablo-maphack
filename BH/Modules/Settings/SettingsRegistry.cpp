#include "SettingsRegistry.h"
#include <map>
#include "../../BH.h"
#include "../Module.h"

namespace Settings {
	// Function local rather than at file scope: modules register while they are
	// being loaded, and a container built by static initialisation in a DLL is one
	// more ordering question nobody wants to have to answer.
	static std::vector<Descriptor>& Store() {
		static std::vector<Descriptor> store;
		return store;
	}

	static unsigned int& VersionRef() {
		static unsigned int version = 0;
		return version;
	}

	static void Add(Descriptor descriptor) {
		Store().push_back(descriptor);
		VersionRef()++;
	}

	// The fields every kind shares, so each of the registration calls below says
	// only what is different about its own kind.
	static Descriptor Common(Kind kind, const std::string& owner,
			const std::string& category, const std::string& key,
			const std::string& label, const std::string& help,
			const std::string& parent) {
		Descriptor descriptor;
		descriptor.kind = kind;
		descriptor.owner = owner;
		descriptor.category = category;
		descriptor.key = key;
		descriptor.label = label;
		descriptor.help = help;
		descriptor.parent = parent;
		return descriptor;
	}

	void AddBool(std::string owner, std::string category, std::string key,
			std::string label, bool* value, std::string help,
			std::string parent) {
		Descriptor descriptor = Common(KindBool, owner, category, key, label, help, parent);
		descriptor.boolValue = value;
		Add(descriptor);
	}

	void AddToggle(std::string owner, std::string category, std::string key,
			std::string label, Toggle* value, std::string help,
			std::string parent) {
		Descriptor descriptor = Common(KindToggle, owner, category, key, label, help, parent);
		descriptor.toggleValue = value;
		Add(descriptor);
	}

	void AddKey(std::string owner, std::string category, std::string key,
			std::string label, unsigned int* value, std::string help,
			std::string parent) {
		Descriptor descriptor = Common(KindKey, owner, category, key, label, help, parent);
		descriptor.intValue = value;
		Add(descriptor);
	}

	void AddEnum(std::string owner, std::string category, std::string key,
			std::string label, unsigned int* value,
			std::vector<std::string> options, std::string help,
			std::string parent) {
		Descriptor descriptor = Common(KindEnum, owner, category, key, label, help, parent);
		descriptor.intValue = value;
		descriptor.options = options;
		Add(descriptor);
	}

	void AddColor(std::string owner, std::string category, std::string key,
			std::string label, unsigned int* value, std::string help,
			std::string parent) {
		Descriptor descriptor = Common(KindColor, owner, category, key, label, help, parent);
		descriptor.intValue = value;
		Add(descriptor);
	}

	void AddNumber(std::string owner, std::string category, std::string key,
			std::string label, unsigned int* value, unsigned int max,
			std::string help, std::string parent) {
		Descriptor descriptor = Common(KindNumber, owner, category, key, label, help, parent);
		descriptor.intValue = value;
		descriptor.numberMax = max;
		Add(descriptor);
	}

	void AddSlider(std::string owner, std::string category, std::string key,
			std::string label, unsigned int* value, unsigned int min,
			unsigned int max, unsigned int step, std::string unit,
			std::string help, std::string parent) {
		Descriptor descriptor = Common(KindSlider, owner, category, key, label, help, parent);
		descriptor.intValue = value;
		descriptor.numberMin = min;
		descriptor.numberMax = max;
		descriptor.numberStep = step;
		descriptor.unit = unit;
		Add(descriptor);
	}

	void AddText(std::string owner, std::string category, std::string key,
			std::string label, std::string* value, unsigned int maxLength,
			std::string help, std::string parent) {
		Descriptor descriptor = Common(KindText, owner, category, key, label, help, parent);
		descriptor.textValue = value;
		descriptor.textMax = maxLength;
		Add(descriptor);
	}

	// The text is the label: there is no value for a key to name, and a search
	// still has something to match on.
	void AddNote(std::string owner, std::string category, std::string text) {
		Add(Common(KindNote, owner, category, "", text, "", ""));
	}

	void AddHeading(std::string owner, std::string category, std::string text) {
		Add(Common(KindHeading, owner, category, "", text, "", ""));
	}

	const std::vector<Descriptor>& All() {
		return Store();
	}

	std::vector<const Descriptor*> InCategory(const std::string& category) {
		const std::vector<Descriptor>& all = Store();
		std::vector<const Descriptor*> found;
		for (unsigned int i = 0; i < all.size(); i++) {
			if (all[i].category.compare(category) == 0)
				found.push_back(&all[i]);
		}
		return found;
	}

	std::vector<std::string> Categories() {
		const std::vector<Descriptor>& all = Store();
		std::vector<std::string> names;
		for (unsigned int i = 0; i < all.size(); i++) {
			bool known = false;
			for (unsigned int n = 0; n < names.size() && !known; n++)
				known = (names[n].compare(all[i].category) == 0);
			if (!known)
				names.push_back(all[i].category);
		}
		return names;
	}

	unsigned int Version() {
		return VersionRef();
	}

	// A setting's value, whatever kind it is, in a form that can be compared and
	// kept. The kinds that address an unsigned int need one number; a Toggle needs
	// two, because rebinding its hotkey is a change as much as flipping it is; a
	// text setting needs the string, which no number stands in for.
	struct Snapshot {
		unsigned int a, b;
		std::string text;
		Snapshot() : a(0), b(0) {};
		bool operator!=(const Snapshot& other) const {
			return a != other.a || b != other.b || text.compare(other.text) != 0;
		}
	};

	static bool HasValue(const Descriptor& descriptor) {
		return descriptor.kind != KindNote && descriptor.kind != KindHeading;
	}

	static Snapshot Read(const Descriptor& descriptor) {
		Snapshot snapshot;
		switch (descriptor.kind) {
			case KindBool:
				if (descriptor.boolValue)
					snapshot.a = *descriptor.boolValue ? 1 : 0;
				break;
			case KindToggle:
				if (descriptor.toggleValue) {
					snapshot.a = descriptor.toggleValue->state ? 1 : 0;
					snapshot.b = descriptor.toggleValue->toggle;
				}
				break;
			case KindKey:
			case KindEnum:
			case KindColor:
			case KindNumber:
			case KindSlider:
				if (descriptor.intValue)
					snapshot.a = *descriptor.intValue;
				break;
			case KindText:
				if (descriptor.textValue)
					snapshot.text = *descriptor.textValue;
				break;
			default:
				break;
		}
		return snapshot;
	}

	// The inverse of Read, for putting a value back where it was.
	static void Restore(const Descriptor& descriptor, const Snapshot& snapshot) {
		switch (descriptor.kind) {
			case KindBool:
				if (descriptor.boolValue)
					*descriptor.boolValue = (snapshot.a != 0);
				break;
			case KindToggle:
				if (descriptor.toggleValue) {
					descriptor.toggleValue->state = (snapshot.a != 0);
					descriptor.toggleValue->toggle = snapshot.b;
				}
				break;
			case KindKey:
			case KindEnum:
			case KindColor:
			case KindNumber:
			case KindSlider:
				if (descriptor.intValue)
					*descriptor.intValue = snapshot.a;
				break;
			case KindText:
				if (descriptor.textValue)
					*descriptor.textValue = snapshot.text;
				break;
			default:
				break;
		}
	}

	static std::vector<Snapshot>& Notified() {
		static std::vector<Snapshot> notified;
		return notified;
	}

	static std::vector<Snapshot>& Persisted() {
		static std::vector<Snapshot> persisted;
		return persisted;
	}

	// Settings registered since the last poll, which count as changed so that a
	// module is told about its own settings once without having to ask.
	static std::vector<bool>& Pending() {
		static std::vector<bool> pending;
		return pending;
	}

	void Poll() {
		std::vector<Descriptor>& all = Store();
		std::vector<Snapshot>& notified = Notified();
		std::vector<bool>& pending = Pending();

		if (notified.size() != all.size()) {
			notified.resize(all.size());
			pending.resize(all.size(), true);
		}

		// Gathered per module rather than reported one at a time, so a module that
		// answers a dozen of its settings with one action does that action once.
		std::map<std::string, std::vector<std::string>> changed;
		for (unsigned int i = 0; i < all.size(); i++) {
			if (!HasValue(all[i]))
				continue;
			Snapshot now = Read(all[i]);
			if (!pending[i] && !(now != notified[i]))
				continue;
			notified[i] = now;
			pending[i] = false;
			changed[all[i].owner].push_back(all[i].key);
		}

		for (std::map<std::string, std::vector<std::string>>::iterator it = changed.begin();
				it != changed.end(); ++it) {
			Module* module = BH::moduleManager->Get(it->first);
			if (module)
				module->OnSettingsChanged(it->second);
		}
	}

	// A setting registered after the baseline was taken is not unsaved work, so it
	// starts out matching itself rather than counting as a change.
	static void SizePersisted() {
		std::vector<Descriptor>& all = Store();
		std::vector<Snapshot>& persisted = Persisted();
		while (persisted.size() < all.size())
			persisted.push_back(Read(all[persisted.size()]));
	}

	bool IsDirty() {
		SizePersisted();
		std::vector<Descriptor>& all = Store();
		std::vector<Snapshot>& persisted = Persisted();
		for (unsigned int i = 0; i < all.size(); i++) {
			if (!HasValue(all[i]))
				continue;
			if (Read(all[i]) != persisted[i])
				return true;
		}
		return false;
	}

	void Rebaseline() {
		std::vector<Descriptor>& all = Store();
		std::vector<Snapshot>& persisted = Persisted();
		persisted.clear();
		for (unsigned int i = 0; i < all.size(); i++)
			persisted.push_back(Read(all[i]));
	}

	void Persist() {
		// Written whether or not the shadows say anything changed. Config::Write()
		// does nothing when nothing differs from the file, and it also covers the
		// settings that are read from the config but never registered here, which
		// the shadows know nothing about.
		BH::config->Write();
		Rebaseline();
	}

	void MarkAllChanged() {
		std::vector<bool>& pending = Pending();
		pending.assign(Store().size(), true);
	}

	void Revert() {
		SizePersisted();
		std::vector<Descriptor>& all = Store();
		std::vector<Snapshot>& persisted = Persisted();
		for (unsigned int i = 0; i < all.size(); i++) {
			if (!HasValue(all[i]))
				continue;
			Restore(all[i], persisted[i]);
		}
		// Told rather than left to be noticed: putting a value back is as much a
		// change as moving it was, and the modules have to act on it. Which happens
		// on the next poll, on the game loop, rather than here on the input thread.
		MarkAllChanged();
	}
};
