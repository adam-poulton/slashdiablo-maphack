# Signing in happens only from a click

Filling the login boxes and pressing the game's own login button happens when
the player clicks a row of the accounts panel, and at no other time. Nothing on
the login screen watches for a state to be true and acts on it: not the screen
appearing, not a box being empty, not an account being marked as the one used
last.

This is a small rule with an expensive failure behind it. The realm throttles
and then locks an account after repeated failed sign-ins, so anything that
submits without being asked can submit a stale password more than once, and what
the player is left with is not an error message but an account they cannot reach
for a while. A click cannot repeat itself; a state check can, every frame, until
something notices.

## Considered options

Signing in automatically when the login screen appears, for an account the
player marks for it, was rejected for now. It is what the third-party tools do
and it is the shape of the request that started this. It is also where every
version of the failure above lives, and it multiplies badly: several clients
launched together would race to sign in as whichever account was marked, which
is the double sign-in the panel otherwise warns about. It is a coherent thing to
add later, and the way in is a click the player has already made rather than a
condition the panel notices.

Retrying a failed sign-in, with the password the panel holds, was rejected for
the same reason and more firmly. A wrong password retried is the lockout, not a
step towards avoiding it.

## Consequences

The panel does not know whether a sign-in it started succeeded, and does not
ask. What it would do with the answer is retry, which is the thing it must not
do.

An account already held by another instance is drawn as held and stays
clickable, because the mark can be wrong in the direction that matters: a client
that crashed leaves the realm believing the account is still signed in, and the
player's way out of that is to sign in again. A hard block would take away the
one action that fixes it.
