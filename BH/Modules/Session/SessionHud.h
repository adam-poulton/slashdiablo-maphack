#pragma once
#include "../../Drawing.h"
#include "HudLayout.h"
#include "SessionModel.h"

// The session HUD: a session's figures drawn over the game, small enough to be
// read in a fight and inert so that it cannot be clicked in one. It draws text
// and nothing that hears the mouse, and a click on it goes to the game.
//
// It answers the mouse only while it is being placed, which is asked for and
// not stumbled into. Then it is outlined, follows a drag anywhere on the
// canvas, and steps through its views under the wheel, until Escape.
class SessionHud {
	public:
		enum Background {
			BackgroundNone,
			BackgroundTranslucent,
			BackgroundSolid
		};

		SessionHud();

		// Called once, before anything is drawn: reads where the HUD was left.
		void Load();

		// How many views on the wheel was turned over a HUD being placed since
		// this was last asked: towards the user is on to the next.
		int TakeViewSteps();

		// On the game's draw.
		void Draw(const HudLayout::Figures& figures, HudLayout::View view,
			unsigned int background);

		bool IsBeingPlaced() const { return placing; }
		void SetBeingPlaced(bool place);

	private:
		// What hears the mouse and the keyboard while the HUD is being placed.
		// Inactive the rest of the time, when it hears nothing and draws nothing.
		class Handle : public Drawing::Hook {
			private:
				SessionHud* hud;

			public:
				explicit Handle(SessionHud* hud);

				unsigned int GetXSize();
				unsigned int GetYSize();
				void OnDraw();
				bool OnLeftClick(bool up, unsigned int x, unsigned int y);
				bool OnMouseWheel(int notches, unsigned int x, unsigned int y);
				bool OnKey(bool up, BYTE key, LPARAM lParam);
		};

		Handle* handle;
		int viewSteps;

		HudLayout::Place place;
		HudLayout::Point at;		// where the block was last drawn
		HudLayout::Size size;		// and how large

		bool placing;
		bool dragging;
		HudLayout::Point grabbed;	// from the block's corner to where it was taken hold of

		void Follow();
		void DrawOutline();
		void Save();
};
