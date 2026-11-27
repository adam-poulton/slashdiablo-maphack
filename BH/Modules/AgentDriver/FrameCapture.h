#pragma once
#ifdef BH_AGENT

#include <Windows.h>
#include <functional>
#include <string>

namespace Agent {

// What became of a request for a frame: its size in pixels, or why there is none.
struct CapturedFrame {
	int width = 0;
	int height = 0;
	std::string error;
};

// Reads back what an OpenGL renderer (D2GL) is about to show. The window of a
// game on a desktop nobody is looking at is not composed anywhere, so nothing
// outside the process can read its pixels: a BitBlt from its DC returns what is
// on the player's screen instead. The renderer's own back buffer is whole
// whatever the desktop, and is read here, on the renderer's thread, as it
// presents the frame.
namespace FrameCapture {

// Puts the capture in the way of the renderer's SwapBuffers. False when the
// module presents nothing through OpenGL, which is any renderer but D2GL.
bool Install(HMODULE renderer);

// Whether a frame has been presented through OpenGL yet. Until one has, a
// request would wait for a frame that may never come.
bool Presenting();

// Writes a frame to the file as top-down BGRA, four bytes a pixel, and then
// calls done on the renderer's thread. The frame is the second presented after
// the call: the renderer runs a frame behind the game, so the first may have
// been drawn before whatever the caller has just seen.
void Request(const std::string& path, std::function<void(const CapturedFrame&)> done);

}

}

#endif
