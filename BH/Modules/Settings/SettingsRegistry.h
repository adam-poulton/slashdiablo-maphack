#pragma once
#include <string>
#include <vector>
#include "../../Config.h"

// Where modules say what their settings are, rather than drawing them.
//
// A module registers from OnLoad; the settings window reads the registry the
// first time it lays itself out. That is why this is a registry and not a call
// into the window: modules are loaded in alphabetical order, so a window that
// had to exist before the first registration would have to be named to sort
// first, and adding a module could silently reorder the settings.
//
// A descriptor says what a setting is, never what it looks like. Where it goes,
// how wide it is, what its control is and how it reflows are the window's to
// decide - that is what makes the settings searchable and resizable at all.
namespace Settings {
	// The tabs, named once.
	namespace Category {
		const char* const Map = "Map";
		const char* const Items = "Items";
		const char* const Filter = "Filter";
		const char* const Input = "Input";
		const char* const Lobby = "Lobby";
	}

	enum Kind {
		KindBool,		// a bool on its own, checkbox
		KindToggle,		// a Toggle: a checkbox and the hotkey that flips it
		KindKey,		// a hotkey with nothing to check
		KindEnum,		// an index into a list of options
		KindColor,
		KindNumber,		// a number typed into a box
		KindText,		// a line of text typed into a box
		KindNote,		// no value at all: something the panel has to say
		KindHeading,	// a rule across the panel, naming the settings below it
	};

	// Which value pointer means anything follows from the kind. The hotkey, enum,
	// colour and number kinds all address an unsigned int, which is what the
	// controls behind them take, so they share one pointer rather than having
	// four that are the same type.
	struct Descriptor {
		std::string owner;		// module name, for telling it what changed
		std::string category;	// which tab the setting lands under
		std::string key;		// the config key, and what a search matches on
		std::string label;
		std::string help;		// optional, shown on hover
		std::string parent;		// key of the setting this one depends on
		Kind kind;

		bool* boolValue;
		Toggle* toggleValue;
		unsigned int* intValue;
		std::string* textValue;				// KindText
		std::vector<std::string> options;	// KindEnum
		unsigned int numberMax;				// KindNumber, 0 for no ceiling
		unsigned int textMax;				// KindText, characters, 0 for no limit

		Descriptor() : kind(KindBool), boolValue(NULL), toggleValue(NULL),
			intValue(NULL), textValue(NULL), numberMax(0), textMax(0) {};
	};

	// Registration. The order settings are registered in is the order they are
	// shown in within their category, so a module lists its settings in the order
	// it wants them read.
	//
	// A parent names another setting's key: this one belongs under it and means
	// nothing while it is off. Left empty for a setting that stands alone.
	void AddBool(std::string owner, std::string category, std::string key,
		std::string label, bool* value, std::string help = "",
		std::string parent = "");

	void AddToggle(std::string owner, std::string category, std::string key,
		std::string label, Toggle* value, std::string help = "",
		std::string parent = "");

	void AddKey(std::string owner, std::string category, std::string key,
		std::string label, unsigned int* value, std::string help = "",
		std::string parent = "");

	void AddEnum(std::string owner, std::string category, std::string key,
		std::string label, unsigned int* value, std::vector<std::string> options,
		std::string help = "", std::string parent = "");

	void AddColor(std::string owner, std::string category, std::string key,
		std::string label, unsigned int* value, std::string help = "",
		std::string parent = "");

	// A ceiling of 0 means there is none. The window clamps to it as the box is
	// typed into, so a module does not have to watch its own setting for a value
	// it cannot use.
	void AddNumber(std::string owner, std::string category, std::string key,
		std::string label, unsigned int* value, unsigned int max = 0,
		std::string help = "", std::string parent = "");

	// A line of text, of the kind Config::ReadString() reads: a game name, a
	// password, anything a module keeps as a std::string rather than a number. The
	// limit is in characters, 0 for none, and the window enforces it as the box is
	// typed into so a module never sees a value longer than it can use.
	//
	// The string the module already reads its config into, so saving is the config
	// writing back through the same pointer it read through.
	void AddText(std::string owner, std::string category, std::string key,
		std::string label, std::string* value, unsigned int maxLength = 0,
		std::string help = "", std::string parent = "");

	// Something to say rather than something to set: a caveat, or what a gesture
	// does. A note carries no value and nothing can be bound to it, but it is
	// still a descriptor so that it wraps to the window and can be found by a
	// search, neither of which loose text drawn into a tab can do.
	void AddNote(std::string owner, std::string category, std::string text);

	// Names the run of settings that follows it, for a category long enough that
	// it wants breaking up. Structure rather than prose, which is why it is not a
	// note: it is never wrapped and never carries help.
	void AddHeading(std::string owner, std::string category, std::string text);

	const std::vector<Descriptor>& All();
	std::vector<const Descriptor*> InCategory(const std::string& category);

	// Categories in the order they were first registered. The window imposes its
	// own order on top of this - a taxonomy that followed module load order would
	// be alphabetical by class name, which is how the old settings window ended up
	// with tabs called Misc, Interaction and Gamble.
	std::vector<std::string> Categories();

	// Bumped by every registration, so a panel that has already laid itself out
	// can tell it needs to again without comparing the whole registry.
	unsigned int Version();

	// Notices settings changing and tells the module that owns them.
	//
	// Polling rather than the controls reporting for themselves, because there are
	// three ways a setting changes and only one of them goes through a control:
	// the settings window, a hotkey pressed outside it, and a reload reading new
	// values into the same variables. One shadow copy per setting catches all
	// three; a callback on the control would catch the first only.
	//
	// Call from the game loop, never from drawing: what modules do in response
	// installs patches and resets caches, and that belongs on the thread the rest
	// of the game logic runs on.
	void Poll();

	// Whether anything differs from what was last written to the file, for saying
	// so in the window. A second shadow, kept apart from the one Poll() uses:
	// "changed since anyone was told" and "changed since it was saved" are
	// different questions.
	bool IsDirty();

	// Writes the settings out and takes that as the new baseline.
	void Persist();

	// Takes the current values as matching the file without writing anything, for
	// after a reload when the file is what was just read.
	void Rebaseline();

	// Treats everything as changed, so the next poll tells every module. Used
	// once at startup, so a module applies its settings without having to do it
	// every frame in case they moved, and again after a reload.
	void MarkAllChanged();

	// Puts every registered setting back to what it was when it was last saved,
	// and has the modules told so they act on it. Only the settings registered
	// here: a value read from the config but never registered is not something
	// this knows how to put back.
	void Revert();
};
