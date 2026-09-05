#pragma once
#include <list>
#include <Windows.h>
#include <string>

namespace Drawing {
	// HookGroups allow use of the basic hooks(Line,Text,Box,Frame)
	//	in more advanced hooks(Input,UI,Button) that require
	//	multiple basic hooks.
	enum HookVisibility {InGame,OutOfGame,Automap,Perm,Group};

	class Hook;
	class HookGroup {
		public:
			std::list<Hook*> Hooks;
			virtual unsigned int GetX() = 0;
			virtual unsigned int GetY() = 0;
			virtual unsigned int GetXSize() = 0;
			virtual unsigned int GetYSize() = 0;
			virtual bool IsActive() = 0;

			// Which screen the group is on, which is the screen every hook in it
			// is on: a hook in a group carries Group rather than a screen of its
			// own, so this is the only thing that can answer for it.
			virtual HookVisibility GetVisibility() = 0;
	};

	// What a control draws itself in while it is switched off. One value, so that
	// a panel full of controls disabled together looks disabled together.
	#define DISABLED_TEXT_COLOR	Grey

	enum BoxTrans {BTThreeFourths, BTOneHalf, BTOneFourth, BTWhite, BTBlack, BTNormal, BTScreen, BTHighlight, BTFull};
	enum {None=0, Center=1, Right=2, Top=4};


	typedef std::list<Hook*> HookList;
	typedef std::list<Hook*>::iterator HookIterator;
	typedef bool (__cdecl *OnClick)(bool,Hook*,void*);

	class Hook {
		private:
			static HookList Hooks;//Holds a list of every basic hook used.

			//The hook that took the left button down, until it comes back up. A
			//click belongs to one control for the length of the gesture, so the
			//capture is held here rather than in each hook: it is a property of
			//the gesture and not of any one control.
			static Hook* pressedHook;

			// The cursor, as the window procedure last reported it. Only read
			// outside a game, where nothing else keeps it.
			static int oogMouseX;
			static int oogMouseY;
			HookVisibility visibility;//When we should show the hook.
			unsigned int x, y, z;//Hooks screen coordinates and the z-order.
			CRITICAL_SECTION crit;//Critical Section so we don't have race conditions.
			bool active;//Boolean to hold if we should draw the hook or not.

			//Whether the hook will answer input. Separate from active, which is
			//whether it is drawn at all
			bool enabled;
			int alignment;//Holds what type of alignment(if any) we should use.
			HookGroup* group;//Holds the group this hook is associated with.
			OnClick left;//Click callback handler for left clicking
			void* leftVoid;//Holds data to give back to the callback for things like knowing your proper class.
			OnClick right;//Click callback handler for right clicking
			void* rightVoid;//Holds data to give back to the callback for things like knowing your proper class.
		public:
			//Two Hook Initializations; one for basic hooks, one for grouped hooks.
			Hook(HookVisibility visibility, unsigned int x, unsigned int y);
			Hook(HookGroup* group, unsigned int x, unsigned int y);
			//Unregisters the hook from the dispatch list and from its group, so a
			//hook can be destroyed while the game is running.
			virtual ~Hook();

			//Critical Section Helpers.
			void Lock();
			void Unlock();

			//Returns the x position of where the hook will be drawn.
			unsigned int GetX();

			//Returns the base x position not calculating in groups or alignment.
			unsigned int GetBaseX();

			//Sets the base x position, or offset from groups x position.
			void SetBaseX(unsigned int xPos);

			//Returns the width of the hook, determine by super-class.
			virtual unsigned int GetXSize() = 0;


			//Returns the y position of where the hook will be drawn.
			unsigned int GetY();

			//Returns the base y position not calculating in groups or alignment.
			unsigned int GetBaseY();

			//Sets the base y position, or offset from groups y position.
			void SetBaseY(unsigned int yPos);

			//Returns the height of the hook, determine by super-class.
			virtual unsigned int GetYSize() = 0;


			//Returns when the hook will be drawn compared to other hooks.
			int GetZOrder();

			//Sets when the hook will be drawn compared to the other hooks.
			void SetZOrder(int zPos);


			//Returns when the hook will be visible
			HookVisibility GetVisibility();

			// Which screen the hook is really on: its own, or its group's where it
			// is in one. A hook built into a group carries Group and nothing that
			// names a screen, so this is what has to be asked instead.
			HookVisibility GetScreen();

			// Whether the hook answers input that arrived on that screen. A hook
			// that is drawn everywhere answers everywhere.
			bool AnswersOn(HookVisibility screen);

			//Sets when the hook will be visible
			void SetVisibility(HookVisibility newVisibility);


			//Returns if we are drawing the hook currently.
			bool IsActive();

			//Sets if we should be drawing the hook.
			void SetActive(bool newActive);


			//Returns whether the hook answers input. A disabled hook is still
			//drawn, and draws itself dimmed to say so.
			bool IsEnabled();

			//Sets whether the hook answers input.
			void SetEnabled(bool newEnabled);


			//Returns how we are going to align the hook.
			int GetAlignment();

			//Sets how we are to align the hook.
			void SetAlignment(int newAlign);

			
			//Returns the hook's group.
			HookGroup* GetGroup();

			//Sets the hook's group.
			void SetGroup(HookGroup* newGroup);

			//Returns the callback handler for left clicks
			OnClick GetLeftClickHandler();

			//Return the callback void handler for left clicks
			void* GetLeftClickVoid();

			//Set the callback for left clicks
			void SetLeftCallback(OnClick leftHandler, void* voidVar);


			//Returns the callback handler for right clicks
			OnClick GetRightClickHandler();

			//Return the callback void handler for right clicks
			void* GetRightClickVoid();

			//Set the callback for right clicks
			void SetRightCallback(OnClick rightHandler, void* voidVar);

			//Determine if the given x/y set is within the hooks drawing area.
			bool InRange(unsigned int x, unsigned int y);


			// How far below the hook's top its text is drawn.
			// Only the hook knows where its own text goes.
			virtual unsigned int GetTextInset() { return 0; };

			//This is the function in super-class we actually draw the function.
			virtual void OnDraw() = 0;

			//Function gets called when someone clicks, return true to block the click.
			virtual bool OnLeftClick(bool up, unsigned int x, unsigned int y) { return false; };
			virtual bool OnRightClick(bool up, unsigned int x, unsigned int y) { return false; };

			//Function gets called when someone types, return true to block the input.
			virtual bool OnKey(bool up, BYTE key, LPARAM lParam) { return false; };

			//Function gets called when the mouse wheel turns, return true to block it.
			//notches is signed: positive is a turn away from the user, ie scroll up.
			virtual bool OnMouseWheel(int notches, unsigned int x, unsigned int y) { return false; };


			//Static function to draw all the hooks with the given visibility.
			static void Draw(HookVisibility type);

			//Static function to check if we interacted with any hooks.
			// Every dispatcher is told which screen the input arrived on and
			// offers it only to the hooks that are on that screen. Without it a
			// hook laid out for a game answers a click on the login screen, where
			// it is not drawn and cannot be seen.
			static bool LeftClick(HookVisibility screen, bool up, unsigned int x, unsigned int y);
			static bool RightClick(HookVisibility screen, bool up, unsigned int x, unsigned int y);
			static bool KeyClick(HookVisibility screen, bool bUp, BYTE bKey, LPARAM lParam);
			static bool MouseWheel(HookVisibility screen, int notches, unsigned int x, unsigned int y);

			//Misc Hook Functions needed
			static unsigned int GetScreenHeight();
			static unsigned int GetScreenWidth();

			// Where the cursor is, in the coordinates everything here is drawn
			// and hit tested in.
			//
			// In a game that is the game's own position, which the drag code
			// writes to as well as reads. Out of a game D2Client is not running
			// its input loop and never updates it, so the position is tracked
			// from the window's own mouse messages instead. Asking here rather
			// than reading the game's variable is what lets one control work on
			// both screens.
			static int GetMouseX();
			static int GetMouseY();
			static void SetMousePosition(int x, int y);
			static void ScreenToAutomap(POINT* ptPos, int x, int y);
			static void AutomapToScreen(POINT* ptPos, int x, int y);

	};
};