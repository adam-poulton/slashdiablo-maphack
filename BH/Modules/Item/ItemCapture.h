#pragma once
#include <string>

struct ItemFacts;

/*
 * Records ground items along with the decision the item filter reached for
 * each one, appended to item-captures.txt beside the configuration files in
 * the line format CaptureFormat describes.
 *
 * Nothing is written unless the "Capture Item Drops" setting is on. That
 * setting has no entry in the settings window and no page in the documentation:
 * it exists so a capture can be asked for when someone reports a filter
 * behaving unexpectedly, and so the recorded decisions can be replayed against
 * the filter in the tests.
 *
 * Recording an item is preceded, once, by a header of what its decision rested
 * on that only a change of configuration can alter: the settings in force, the
 * rules, the condition groups and the skill lists. That is written again after
 * the configuration is reloaded, so the items following a header always belong
 * to it.
 *
 * Everything that moves while playing is recorded against the item instead. The
 * character walks between areas, gains levels, and a capture may run across more
 * than one game, so the area, the character and the difficulty are part of what
 * an item's line says rather than of the header's.
 *
 * A capture cannot pin the whole of what a rule may read. CHARSTAT reads
 * character stats as they are at the moment the item lands, and PRICE asks the
 * game a question that can only be asked of an item that already exists as a
 * unit. Rules using either are reported by the header's contextSensitive flag
 * rather than silently recorded as if they were reproducible.
 */
namespace ItemCapture {
	// What the filter concluded about one ground item.
	struct Outcome {
		unsigned int keepIndex;
		unsigned int ignoreIndex;
		bool blocked;
		bool showOnMap;
		bool noTracking;
		int color;
		int pingLevel;
	};

	// Reads the capture setting. Safe to call whenever configuration is loaded.
	void LoadConfig();

	/*
	 * Notes that what a header describes may no longer hold, so the next item
	 * recorded is preceded by a fresh one.
	 *
	 * The settings window writes through the addresses its widgets were given,
	 * so a setting can change without any configuration being read. Rereading
	 * it here would undo that, since what was parsed from the file is what a
	 * read returns until settings are saved.
	 */
	void SettingsChanged();

	bool IsEnabled();

	// Appends one item and the outcome it was given. Does nothing while
	// capture is off.
	void RecordDrop(const unsigned char* packet, const ItemFacts& item,
			const Outcome& outcome);
}
