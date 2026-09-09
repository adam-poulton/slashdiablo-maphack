# The layer on disk: profiles, groups, rules, and whether Config can carry them

Type: grilling
Status: open

## Question

The layer is BH-owned, structured and never hand-edited. A profile (Reset, End
game) is a separate file switched wholesale. Inside a profile are groups, inside
groups are rules, and both groups and rules carry an enabled flag.

`Config` reads flat key/value and a few assoc and list shapes out of `.cfg`
files, and can `Write()`. Decide whether that is the right thing to carry nested,
machine-written structure or whether this file wants its own format and its own
reader; decide it against what already exists rather than by preference.

Decide what a rule is identified by inside the file, what happens to a file
written by a newer BH and read by an older one, where the files live relative to
`BH.cfg`, and how a profile is chosen and remembered.
