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
		// Map is the automap and nothing else. What the game draws outside it -
		// the light the scene is lit by, the weather over it, what is written on
		// top of it - is Display, which is the line between the two: a setting
		// belongs to Map only if turning the automap off would make it moot.
		const char* const Map = "Map";
		const char* const Display = "Display";
		const char* const Filter = "Filter";
		const char* const Input = "Input";
		const char* const Lobby = "Lobby";
	}

	// Headings that more than one module registers under. Two modules naming the
	// same heading make one section of it, which holds only while they spell it
	// identically.
	namespace Heading {
		const char* const PanelHotkeys = "Panel hotkeys";
		const char* const Notifications = "Notifications";
	}

	enum Kind {
		KindBool,		// a bool on its own, checkbox
		KindToggle,		// a Toggle: a checkbox and the hotkey that flips it
		KindKey,		// a hotkey with nothing to check
		KindEnum,		// an index into a list of options
		KindColor,
		KindNumber,		// a number typed into a box
		KindSlider,		// a number dragged along a rail
		KindText,		// a line of text typed into a box
		KindNote,		// no value at all: something the panel has to say
		KindHeading,	// a rule across the panel, naming the settings below it
	};

	// Which value pointer means anything follows from the kind. The hotkey, enum,
	// colour, number and slider kinds all address an unsigned int, which is what
	// the controls behind them take, so they share one pointer rather than having
	// five that are the same type.
	struct Descriptor {
		std::string owner;		// module name, for telling it what changed
		std::string category;	// which tab the setting lands under
		std::string key;		// the config key, and what a search matches on
		std::string label;
		std::string help;		// optional, shown on hover
		std::string parent;		// key of the setting this one depends on
		Kind kind;

		// The value of a KindBool, and on a KindSlider the switch, which is a
		// second value of the same setting rather than one of its own.
		bool* boolValue;
		Toggle* toggleValue;
		unsigned int* intValue;
		std::string* textValue;				// KindText
		std::vector<std::string> options;	// KindEnum
		unsigned int numberMax;				// KindNumber/KindSlider ceiling, 0 for none
		unsigned int numberMin;				// KindSlider floor
		unsigned int numberStep;			// KindSlider, what one notch moves
		std::string unit;					// KindSlider, written after the value
		unsigned int textMax;				// KindText, characters, 0 for no limit

		Descriptor() : kind(KindBool), boolValue(NULL), toggleValue(NULL),
			intValue(NULL), textValue(NULL), numberMax(0), numberMin(0),
			numberStep(1), textMax(0) {};
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

	// Replaces what a registered enum can be set to, for options that are not
	// known until they are looked for - the files in a folder, say. The value is
	// left where it is: what a position in the list means is the module's to say,
	// not the registry's.
	void SetOptions(std::string owner, std::string key,
		std::vector<std::string> options);

	void AddColor(std::string owner, std::string category, std::string key,
		std::string label, unsigned int* value, std::string help = "",
		std::string parent = "");

	// A ceiling of 0 means there is none. The window clamps to it as the box is
	// typed into, so a module does not have to watch its own setting for a value
	// it cannot use.
	void AddNumber(std::string owner, std::string category, std::string key,
		std::string label, unsigned int* value, unsigned int max = 0,
		std::string help = "", std::string parent = "");

	// A number that can only ever be one of the values on its rail, which is what
	// makes it the kind to reach for wherever a value outside the range would
	// leave the setting unusable: a box finds out what was typed after the fact,
	// a slider cannot be given a value the module cannot act on.
	//
	// The step is what one notch of the wheel, one arrow key and one position on
	// the rail are all worth. A range that does not divide evenly by it still
	// reaches its maximum: the last notch is a short one rather than the rail
	// stopping below the top of the range.
	//
	// The unit is written after the value, so the number on screen says what it
	// is without the label having to carry it.
	//
	// A slider can carry a switch: a bool holding whether the setting applies at
	// all, with the rail inert and the readout saying Off while it is off. That is
	// one setting and so one row - a checkbox on a row of its own would leave the
	// rail beside it showing a number that is not in force - and the switch is what
	// names the row, so its box shares a column with every plain on/off setting.
	// The module still reads and writes the bool under a config key of its own;
	// the registry only draws it.
	void AddSlider(std::string owner, std::string category, std::string key,
		std::string label, unsigned int* value, unsigned int min,
		unsigned int max, unsigned int step, std::string unit = "",
		std::string help = "", std::string parent = "",
		bool* onOff = NULL);

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
