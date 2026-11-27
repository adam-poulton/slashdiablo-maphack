# A run drawn by D2GL is photographed from inside

Slash Diablo players do not see the game its GDI renderer draws. They see it
through D2GL: OpenGL, scaled to a window of their choosing, with D2GL's own
text, cursor and map on top. ADR 0011 left that picture to the player, on two
grounds: D2GL refuses `-w`, and OpenGL on a desktop nobody is looking at was
not something to depend on. Both were tried, and neither holds. A run can now
ask for D2GL as its renderer, and its screenshots are what D2GL presents.

GDI stays the default. It is the same on every machine, pixel for pixel, costs
no GPU, and needs nothing but the game.

## D2GL runs on a desktop nobody is looking at

D2GL refuses `-w` because it makes its own window: started `-3dfx` and nothing
else, with `fullscreen=false`, the game gets an ordinary window of the size its
config names. It creates its OpenGL context on a hidden desktop as it does on
the player's, and renders to it.

Each run gets a `d2gl.ini` of its own, written by the harness: what a sandbox
needs, the window's size, and whatever the run asks for. D2GL fills in the rest
with its own defaults, so a run sees D2GL's defaults and not the player's, as
it does the game's and BH's (ADR 0011). What a sandbox needs is not a run's to
change:

- The window is borderless, at the top left of the desktop.
- The cursor is unlocked. D2GL confines the cursor to its window with
  `ClipCursor`, and the cursor is the player's. The build the spike ran
  (1.3.3.2, from the adam-poulton/d2gl fork) only does so while its window is the
  foreground one, which a window on another desktop never is, and the clip
  rectangle was the whole screen before, during and after a run. Unlocking
  keeps that true of a build that does not look first.
- Vertical sync is off, since nothing refreshes a desktop that is not shown,
  and the frame rate is capped at 30 whether or not the window counts as
  focused, so a run costs the player's own game little.

## The picture is read from the back buffer

A `BitBlt` from the game window's DC, which is how a GDI run is photographed,
does not return the game under D2GL. A hidden desktop is not composed, so an
OpenGL window on it has nothing behind its DC, and the read came back with the
player's own screen. That is worse than failing: the harness would have put
the player's desktop in a report. `bhctl` never reads the DC of a run D2GL
draws.

The agent build takes the picture instead. It points D2GL's import of
`SwapBuffers` at its own function, which runs on D2GL's render thread with the
context current, and reads the default framebuffer's back buffer with
`glReadPixels` just before the frame is presented. That buffer is whole
whatever the desktop, and holds everything the player would see: the scaled
game, HD text, the HD cursor, D2GL's own menu. The driver writes it to a file
the harness names, top row first.

The game runs a frame ahead of D2GL's render thread, so the first frame
presented after a request may have been drawn before whatever the harness has
just seen. The capture takes the second.

## One set of coordinates, the canvas's

D2GL scales the canvas, 800x600 in the menus and the in-game resolution in a
game, to fit its window, and centres it. Left alone, a scenario would aim
clicks at one set of pixels under GDI and another under D2GL, and a draw log's
rectangles would not be where a screenshot shows them.

Everything a scenario or a command names stays in canvas pixels. `viewport.py`
does D2GL's arithmetic, in the single precision D2GL does it in: a click is
posted at the window pixel D2GL turns back into the canvas pixel meant, and a
rectangle of the canvas is placed on the screenshot where D2GL drew it. The
driver reports the canvas's size with every `state`, since it changes between
the menus and a game.

By default the window is the in-game resolution, so in a game a screenshot's
pixels are the canvas's and a check's outline is exact. A run that wants the
picture at a player's scale names a larger window.

## The harness sees the window as the window sees itself

D2GL makes the game aware of DPI, which the game alone is not. Windows scales
a window's size, and the position in a mouse message posted to it, between a
caller and a window whose awareness differs, so on a display at 125% the
harness measured a 1344x700 window as 1075x560 and its clicks landed a quarter
further out than aimed. Every thread that touches the game's windows now takes
the game window's own awareness first.

## D2GL's keyboard is the agent's too

D2GL asks Windows whether Ctrl is down for its own shortcuts, which on a
hidden desktop is asking about the player's keyboard. The agent build answers
`glide3x.dll`'s keyboard queries as it does the game's (ADR 0012), so Ctrl+O
opens D2GL's menu for an agent, and a key the player happens to hold is not a
run's business.

## Considered options

D2GL's own screenshot, which it takes when the game's screenshot key is
pressed, was used to prove the read works and then set aside. It reads part
way through a frame, keeps only the viewport, names its own files, and says
nothing when it is done.

Asking D2GL for the picture through an export of its own was not pursued. It
would tie the harness to one build of D2GL, and the import of `SwapBuffers` is
there in every build.

Carrying the player's `d2gl.ini` into a run was rejected for the reason their
BH settings are not carried: a run that depends on what one player chose does
not run the same anywhere else.

## Consequences

A scenario can be run under either renderer, and the same one under both
(`scenario --renderer d2gl`). What differs is real: with D2GL's HD text on, the
game measures text in D2GL's fonts, so text BH draws is a pixel or two wider or
narrower than under GDI, and a check of an exact `rect` holds under one and not
the other.

A run drawn by D2GL needs the agent build for its screenshots, where a GDI run
needs it only for what it is asked. It also needs D2GL in the install the base
is copied from, and a GPU.

The clipboard belongs to the window station, not the desktop, so D2GL's menu,
like the game's own chat box, can read and write the player's. Nothing a run
does by default goes near it.
