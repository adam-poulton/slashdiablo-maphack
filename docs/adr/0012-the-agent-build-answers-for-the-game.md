# The agent build answers for the game

A sandboxed game (ADR 0011) can be driven and photographed from outside, but a
screenshot does not say what screen the game is on, a posted key never reaches
the state the game reads Alt and Shift from, and a picture of BH's windows can
only be compared by eye. The agent build of BH closes those gaps from inside the
game: it says what is on screen, answers the game's questions about the keyboard,
records what BH draws, and leaves a game cleanly.

It is Release with `BH_AGENT` defined, built with `/p:BHAgent=true` into
`AgentRelease\`, and nothing more. The Release DLL players get does not contain
it. Inside the agent build it does nothing unless the game was started by
`AgentLaunch`, which is what puts `BHAGENT_REGISTRY` in the game's environment,
so an agent build copied anywhere else behaves as Release does.

## One request, one answer, on the game's thread

The driver serves `\\.\pipe\bhagent-<pid>`. A request is a line of JSON and so is
its answer, one pair to a connection, which keeps the client a dozen lines of
Python and leaves no session to get out of step.

The pipe has its own thread, and that thread touches nothing of the game's. It
queues the request and waits; the game's thread answers it after BH has drawn a
frame, where game memory is the game's to read. A game that is loading draws no
frames, so a request then is answered as not taken rather than read half-built.

## A screen is named by its buttons

Out of a game the driver reports the game's own controls, each with its rectangle
and the centre a click should aim at, and names the screen when the buttons are
exactly one it knows: the main menu, the character list, the class choice for a
new character, the difficulty choice.
Exactly, because a popup over a screen adds a button of its own, and a screen
with a popup over it is not one that takes the click a harness is about to send.
The measurements come from control lists dumped from a 1.13c client, and the
tests hold them.

This replaced a reading of two buttons' brightness in a screenshot, which knew
the main menu from the title screen and nothing else.

## The keyboard is what was posted

The game and BH ask Windows whether a key is down (`GetKeyState`,
`GetAsyncKeyState`, `GetKeyboardState`), and the game's message loops ask it what
character a key types (`TranslateMessage`). Windows answers all four from input
that went through the keyboard, which posted messages never do: held keys read
as up, and every key types as though Shift were up. So the agent build points
the imports of all four, in `D2Client`, `D2Win`, `D2Launch`, `Fog`, `Storm` and
BH, at its own answers, kept from the key and button messages the game window
receives. On a desktop no keyboard is attached to, every one of those messages
is the agent's.

The answers match Windows to the bit: `0xFF80` for a key down in `GetKeyState`,
which code testing `& 0x80` relies on, BH's own included, and `0x8000` in
`GetAsyncKeyState`. Its `TranslateMessage` posts the character `ToAscii` makes of
the key from the agent's keyboard, as Windows' would from the real one. That is
what lets the harness type the way a keyboard does, keys only: the game's edit
boxes take the characters the loop makes, and BH's boxes read the keys. Posting
characters as well types everything twice into the game's boxes.

The imports are found by the address they hold rather than by name, because the
game's DLLs were linked with bound imports, whose slots keep no names.

## What BH draws is recorded where it is drawn

Every line, box and piece of text BH draws goes through three of the game's
functions, `D2GFX_DrawLine`, `D2GFX_DrawRectangle` and `D2WIN_DrawText`, called
from some thirty places, and the ornate frames around BH's windows through a
fourth, `D2CLIENT_DrawRectFrame`. In the agent build those names are BH's own
functions, which record the call and make it through the game's pointer, now
named with `_Game`. Nothing that draws had to change, and the game's own drawing
is not recorded, since it never calls through BH.

The frame function takes its rectangle in `eax`, which no C calling convention
passes, so its stand-in is written in assembly and keeps every register for the
jump. Its art does not land on that rectangle, so a frame is logged as the
pixels the art covers (`FrameArt.h`), which is what a check of it can see.

A recording runs from the end of one frame to the end of the next, which takes in
the automap's drawing as well as the frame's, whichever the game does first.
Text is placed by its top, as the window framework places it, using the one table
of baseline offsets the framework now shares (`TextMetrics.h`).

## Leaving a game

`exit` calls `D2CLIENT_ExitGame` from the game loop. At D2Client 1.13c `0x42850`
it only raises the flags the game loop leaves a game on, and it has one caller in
the DLL, at `0x89E64`. Called this way it saves the character and, in single
player, returns to the main menu, as Save and Exit does.

## Considered options

Compiling the driver into every build and switching it on by the environment
alone was rejected. It would let agents test the exact DLL players get, but it
would ship a pipe server and rewritten imports in that DLL, inert or not.

Recording at BH's drawing primitives, `Texthook::Draw` and the like, was tried
first and rejected: most of what BH draws calls the game directly, and the log
missed it.

## Consequences

Agents run the agent build and players get Release, and the difference between
them is the `BH_AGENT` blocks. Anything else that differs is a bug.

A module that reads the keyboard some other way, or one of the game's DLLs not
in the list, still does not see posted keys.
