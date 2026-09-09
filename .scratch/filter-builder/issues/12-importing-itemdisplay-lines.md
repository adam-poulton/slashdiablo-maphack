# Importing ItemDisplay lines from BH.cfg, and what happens to one that will not convert

Type: grilling
Status: open
Blocked by: 02, 04

## Question

A player with many hand-written lines in `BH.cfg` should be able to bring them
into the layer rather than retyping them as rows.

This reintroduces on purpose the round-trip problem the rows-only decision was
meant to avoid. Some lines will not become rows: nested rules, operator
combinations the registry has no row for, tokens the action editor does not
model.

Decide what import does with a line it cannot convert - refuses it and says so,
imports it as an opaque rule the builder can toggle but not edit, or something
else. Decide whether import is per-line, per-region or wholesale, whether the
source lines are left alone (they must be, unless there is a strong reason
otherwise), and how the player is told what did and did not come across.
