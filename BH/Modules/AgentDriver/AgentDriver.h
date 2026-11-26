#pragma once
#ifdef BH_AGENT

#include <Windows.h>
#include "../Module.h"

// Lets an agent's harness (AgentHarness/bhctl.py) ask a sandboxed game what it
// is showing and tell it what to do, over \\.\pipe\bhagent-<pid>. Built only
// into the agent build (BHAgent=true), and inert there unless the game was
// started by AgentLaunch, which is what puts BHAGENT_REGISTRY in its
// environment. ADR 0011 is the harness; ADR 0012 is this.
//
// One request per connection, as a line of JSON, answered with a line of JSON:
//   {"cmd": "ping"}     {"ok": true, "pid": n}, answered at once
//   {"cmd": "state"}    the screen, its controls out of a game, the character
//                       and the open panels in one
//   {"cmd": "drawlog"}  everything BH draws in the next whole frame
//   {"cmd": "exit"}     leaves the game the way Save and Exit does
// A failure is {"ok": false, "error": "..."}.
class AgentDriver : public Module {
	public:
		AgentDriver() : Module("Agent Driver") {};

		void OnLoad();
		void OnLoop();

		// Called by the draw handlers once BH has drawn a frame. Requests are
		// answered here, on the game's thread, and a draw log runs from one call
		// to the next, which takes in the automap's drawing as well as the
		// frame's whichever comes first.
		static void FrameEnd(bool inGame);

		// Every message the game window gets, before anything else sees it.
		static void ObserveMessage(UINT message, WPARAM wParam);
};

#endif
