# A scenario is checks with their evidence

An agent checking a change in a sandboxed game (ADRs 0011 and 0012) drives it a
command at a time, looks at what comes back, and says what it saw. Nothing of
that can be run again, and what reaches the user is a claim. A scenario writes
the claims down as checks, and running it produces a report that puts the
evidence for each check beside it: what was asked, what the game and BH showed,
and a screenshot.

## A scenario is data

A scenario is a JSON file of steps: input, waits, checks, screenshots, leaving
and entering a game. It is read whole and refused with the step and the reason
before a game is started, so a mistake in the last step costs nothing to find.
JSON because the harness needs nothing beyond Python's standard library.

## A check judges what BH reports, not pixels

A check holds on the driver's state, as in "the game is in the Rogue
Encampment", and on the draw log, as in "BH drew the text Codex at exactly
238,569". A draw log's rectangles are exact and the same every run, where the
pixels of the same frame change with the light, the weather and every
animation. So pixels are never the judge: the screenshots are evidence, for
whoever reads the report.

A check is tried again until it holds or its time runs out, because the game
moves on a frame at a time and a fixed wait is either slower than needed or,
some runs, too short. That also makes "nothing like it was drawn" (a count of
0) pass as soon as it is true, so a check that something has gone needs the
step that removes it to have had its effect first.

## Every check carries its evidence

Every check saves a screenshot, whatever its outcome, and a draw log check saves
the frame it judged. What it matched is outlined one pixel outside the item's
rectangle, so the outline covers none of it. A text check that matched nothing
lists the texts that read most like it and outlines them in another colour,
because the usual failure is the right text a pixel or a colour away.

The report names the BH.dll it ran by its hash and the checkout it ran from,
says which of BH's settings the scenario changed, and leaves a step that never
ran marked as not run.

## The run is torn down whatever happens

A scenario creates its own run and tears it down at the end, after an error as
well, so a failed scenario leaves nothing behind. `--keep` is there for looking
closer at what went wrong.

## Considered options

Scenarios as Python scripts against `bhctl` were rejected. A script can do
anything, but each would check in its own way, report what it chose to print,
and need reading as a program before it could be trusted. Data limits what a
scenario can say to what the report knows how to show.

Judging screenshots against saved ones was left out. It is what catches the
game drawing over BH or BH's art going wrong, but needs a still scene to compare
against, which a game that rains and lights torches does not give.

## Consequences

The draw log records frames, the border art the game draws around a rectangle
BH gives it, so a check can find a framed window by its frame. Scenarios worth
running again live in `AgentHarness/scenarios/`; `smoke.json` checks that BH
loads, draws its overlays and opens its settings.
