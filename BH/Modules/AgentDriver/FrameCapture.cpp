#ifdef BH_AGENT

#include "FrameCapture.h"

#include <gl/GL.h>

#include <atomic>
#include <cstdio>
#include <mutex>
#include <vector>

#include "ImportRedirect.h"

#pragma comment(lib, "opengl32.lib")

namespace Agent {
namespace FrameCapture {

namespace {

// Past OpenGL 1.1, which is all the Windows header declares.
constexpr GLenum kReadFramebuffer = 0x8CA8;
constexpr GLenum kReadFramebufferBinding = 0x8CAA;
constexpr GLenum kPixelPackBuffer = 0x88EB;
constexpr GLenum kPixelPackBufferBinding = 0x88ED;
typedef void (APIENTRY* BindFramebuffer)(GLenum target, GLuint framebuffer);
typedef void (APIENTRY* BindBuffer)(GLenum target, GLuint buffer);

constexpr int kBytesPerPixel = 4;
constexpr int kFramesToSkip = 1;
constexpr int kMaxUnreadErrors = 16;

struct Pending {
	std::string path;
	std::function<void(const CapturedFrame&)> done;
	int framesToSkip;
};

typedef BOOL (WINAPI* SwapBuffersFunction)(HDC);
SwapBuffersFunction swapBuffers = nullptr;
std::atomic<bool> presenting(false);
std::mutex pendingLock;
std::vector<Pending> pending;

// The window's whole client area from the back buffer of the context current
// on this thread, bottom row first, as OpenGL keeps it. Every piece of state
// the read depends on is the renderer's, so each is set for the read and put
// back after it.
bool ReadBackBuffer(HDC dc, std::vector<unsigned char>& pixels, CapturedFrame& frame) {
	RECT client = {};
	if (!GetClientRect(WindowFromDC(dc), &client) || client.right <= 0 || client.bottom <= 0) {
		frame.error = "the game window has no client area to read";
		return false;
	}
	auto bindFramebuffer = (BindFramebuffer)wglGetProcAddress("glBindFramebuffer");
	auto bindBuffer = (BindBuffer)wglGetProcAddress("glBindBuffer");
	if (!bindFramebuffer || !bindBuffer) {
		frame.error = "the renderer's OpenGL has no framebuffer objects";
		return false;
	}
	frame.width = client.right;
	frame.height = client.bottom;
	pixels.resize((size_t)frame.width * frame.height * kBytesPerPixel);

	GLint framebuffer = 0, packBuffer = 0, readBuffer = 0, rowLength = 0, skipRows = 0, skipPixels = 0;
	glGetIntegerv(kReadFramebufferBinding, &framebuffer);
	glGetIntegerv(kPixelPackBufferBinding, &packBuffer);
	glGetIntegerv(GL_PACK_ROW_LENGTH, &rowLength);
	glGetIntegerv(GL_PACK_SKIP_ROWS, &skipRows);
	glGetIntegerv(GL_PACK_SKIP_PIXELS, &skipPixels);
	bindFramebuffer(kReadFramebuffer, 0);
	bindBuffer(kPixelPackBuffer, 0);
	glGetIntegerv(GL_READ_BUFFER, &readBuffer);
	glReadBuffer(GL_BACK);
	glPixelStorei(GL_PACK_ROW_LENGTH, 0);
	glPixelStorei(GL_PACK_SKIP_ROWS, 0);
	glPixelStorei(GL_PACK_SKIP_PIXELS, 0);

	// An error the renderer left unread would be taken for the read's own.
	for (int unread = 0; unread < kMaxUnreadErrors && glGetError() != GL_NO_ERROR; unread++) {}
	glReadPixels(0, 0, frame.width, frame.height, GL_BGRA_EXT, GL_UNSIGNED_BYTE, pixels.data());
	bool read = glGetError() == GL_NO_ERROR;

	glPixelStorei(GL_PACK_ROW_LENGTH, rowLength);
	glPixelStorei(GL_PACK_SKIP_ROWS, skipRows);
	glPixelStorei(GL_PACK_SKIP_PIXELS, skipPixels);
	glReadBuffer((GLenum)readBuffer);
	bindBuffer(kPixelPackBuffer, (GLuint)packBuffer);
	bindFramebuffer(kReadFramebuffer, (GLuint)framebuffer);
	if (!read)
		frame.error = "OpenGL refused the read of the back buffer";
	return read;
}

CapturedFrame Capture(HDC dc, const std::string& path) {
	CapturedFrame frame;
	std::vector<unsigned char> pixels;
	if (!ReadBackBuffer(dc, pixels, frame))
		return frame;
	FILE* file = nullptr;
	if (fopen_s(&file, path.c_str(), "wb") != 0 || !file) {
		frame.error = "cannot write the frame to " + path;
		return frame;
	}
	size_t rowBytes = (size_t)frame.width * kBytesPerPixel;
	bool written = true;
	for (int row = frame.height - 1; row >= 0 && written; row--)
		written = fwrite(pixels.data() + row * rowBytes, 1, rowBytes, file) == rowBytes;
	if (fclose(file) != 0 || !written)
		frame.error = "cannot write the frame to " + path;
	return frame;
}

BOOL WINAPI CapturingSwapBuffers(HDC dc) {
	presenting = true;
	std::vector<Pending> due;
	{
		std::lock_guard<std::mutex> lock(pendingLock);
		for (auto request = pending.begin(); request != pending.end();) {
			if (request->framesToSkip-- > 0) {
				++request;
				continue;
			}
			due.push_back(std::move(*request));
			request = pending.erase(request);
		}
	}
	for (const Pending& request : due)
		request.done(Capture(dc, request.path));
	return swapBuffers(dc);
}

}  // namespace

bool Install(HMODULE renderer) {
	swapBuffers = (SwapBuffersFunction)GetProcAddress(GetModuleHandleA("gdi32.dll"), "SwapBuffers");
	return swapBuffers && RedirectImport(renderer, "gdi32.dll", "SwapBuffers", (void*)CapturingSwapBuffers) > 0;
}

bool Presenting() {
	return presenting;
}

void Request(const std::string& path, std::function<void(const CapturedFrame&)> done) {
	std::lock_guard<std::mutex> lock(pendingLock);
	pending.push_back({path, std::move(done), kFramesToSkip});
}

}  // namespace FrameCapture
}  // namespace Agent

#endif
