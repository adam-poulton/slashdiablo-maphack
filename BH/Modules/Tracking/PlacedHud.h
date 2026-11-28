#pragma once
#include <vector>
#include "../../Drawing.h"
#include "HudBlock.h"

// A HUD: a block of figures drawn over the game, small enough to be read in a
// fight and inert so that it cannot be clicked in one. It draws text and nothing
// that hears the mouse, and a click on it goes to the game.
//
// It answers the mouse only while it is being placed, which is asked for and
// not stumbled into. Then it is outlined, follows a drag anywhere on the
// canvas, and counts the turns of the wheel over it, until Escape.
class PlacedHud {
	public:
		enum Background {
			BackgroundNone,
			BackgroundTranslucent,
			BackgroundSolid
		};

		// The section of UI.ini its place is kept in, and where it goes until it
		// is moved.
		PlacedHud(const char* section, const HudBlock::Place& unmoved);

		// Called once, before anything is drawn: reads where the HUD was left.
		void Load();

		// How many notches the wheel was turned over a HUD being placed since
		// this was last asked: towards the user counts up.
		int TakeViewSteps();

		// On the game's draw. Nothing to say draws nothing.
		void Draw(const std::vector<HudBlock::Section>& sections, unsigned int background);

		bool IsBeingPlaced() const { return placing; }
		void SetBeingPlaced(bool place);

	private:
		// What hears the mouse and the keyboard while the HUD is being placed.
		// Inactive the rest of the time, when it hears nothing and draws nothing.
		class Handle : public Drawing::Hook {
			private:
				PlacedHud* hud;

			public:
				explicit Handle(PlacedHud* hud);

				unsigned int GetXSize();
				unsigned int GetYSize();
				void OnDraw();
				bool OnLeftClick(bool up, unsigned int x, unsigned int y);
				bool OnMouseWheel(int notches, unsigned int x, unsigned int y);
				bool OnKey(bool up, BYTE key, LPARAM lParam);
		};

		const char* section;
		HudBlock::Place unmoved;

		Handle* handle;
		int viewSteps;

		HudBlock::Place place;
		HudBlock::Point at;			// where the block was last drawn
		HudBlock::Size size;		// and how large

		bool placing;
		bool dragging;
		HudBlock::Point grabbed;	// from the block's corner to where it was taken hold of

		int StoredInt(const char* key, int otherwise) const;
		HudBlock::Side StoredSide(const char* key, HudBlock::Side otherwise) const;
		void Follow();
		void DrawOutline();
		void Save();
};
