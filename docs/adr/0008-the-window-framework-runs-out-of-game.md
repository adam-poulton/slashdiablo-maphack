# The window framework runs out of game

`Drawing::UI` was drawn only from `GameDraw()`, and its clicks were taken only
while `D2CLIENT_GetPlayerUnit()` answered. The accounts panel is a window on the
login screen, which is neither, so the framework now runs out of game: `UI` is
drawn from `OOGDraw()` as well, and each window says for itself which of the two
it appears in rather than the handler deciding for all of them.

A window that says nothing appears in a game only, so the Info and settings
windows are where they were.

## What moved

The blanket `inGame` gate on `UI::LeftClick` and `UI::RightClick` in
`GameWindowEvent` is replaced by the window's own visibility. The gate was a
second mechanism saying what the draw path already said, and two mechanisms for
one idea is how the login screen came to be unreachable by anything with chrome.

The keyboard was gated the same way and worse: out of a game no key reached any
hook at all, because the whole block sits behind `D2CLIENT_GetPlayerUnit()`. It
now asks visibility instead. BH's window procedure runs before the game's, and
returns without calling it when a hook takes the key, which is what makes typing
into our own control on a screen full of the game's controls possible at all.

`Hook::KeyClick` offered a key to every active, enabled hook without asking
about visibility, unlike `Hook::Draw`, which has always filtered on it. In game
that is invisible, since a hook that is not drawn is not usually active either.
Out of game it would have fed login screen keystrokes to in-game hooks. It
filters on visibility now, and so do the click and the wheel, each told which
screen the message arrived on. That was a latent bug rather than a consequence
of this decision, and it is recorded here because this is what made it
reachable.

Filtering on the visibility a hook carries is not enough on its own, and getting
this wrong would silence every search box in the mod. A hook built into a group
carries `Group` and never a screen: it is the group that is shown or hidden, so
the group is what has to be asked. `HookGroup` therefore answers which screen it
is on, and a hook resolves its own screen through its group where it has one. The
five groups answer as they should: a window from itself, its chrome and its tabs
from their window, a scrolling box from whatever it is built into, and the stats
display from the fact that a character's stats are only ever shown in a game.
Making that answer a pure virtual was deliberate, so that a group added later
cannot quietly inherit the wrong screen.

Whether the game's own controls hold the keyboard is asked of
`p_D2WIN_FocusedControl`, which had been declared and never used. Our controls
stand down while a native box has focus, so nothing we draw can take a
keystroke meant for the account name box or the password box.

## Considered options

Building the panel from basic hooks, which already honour out-of-game visibility
and already take clicks there, was rejected. It works, and it is why the game
list filter is drawn the way it is. But the panel owes text input for naming a
roster, and a scrolling list of rows that fold, and a footer that acts on
whatever row is in hand. `Inputhook`, `Listhook`, `Scrollhook` and the window's
footer are each of those, already written and already tested by the windows that
use them. Rebuilding them beside the framework is a second way of drawing a
panel in a codebase that has one, and the two would not stay in step.

Building the panel from the game's own controls, with `D2WIN_CreateEditBox` and
`D2WIN_CreateControl`, was rejected. It is the only option that never competes
for the keyboard, because the game would own every box. It costs a control
lifecycle tied by hand to a screen coming and going, it looks like nothing else
BH draws, and it cannot show a list that folds. The keyboard problem it avoids
is answered instead by capturing credentials from the game's own login boxes,
which is decided in ADR 0009, so the only typing we do is a roster name.

## Consequences

Every window now carries a visibility, and a new one that forgets to say
appears in a game only, which is the behaviour every existing window wants.

Something drawn out of a game is drawn while no player unit exists. Anything a
panel reads from the world has to tolerate that, which is a constraint the
in-game windows never had and which the accounts panel meets by reading only its
own file.
