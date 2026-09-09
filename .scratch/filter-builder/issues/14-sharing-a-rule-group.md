# Sharing a rule group: what it carries, and what an importer does with what it cannot read

Type: grilling
Status: open
Blocked by: 02, 08

## Question

A group can be written out and read back in, so that a player can share a piece
of their filter.

Decide what an exported group carries besides its rules - its name, the profile
it came from, the BH version that wrote it, whether the enabled flags travel.
Decide what an importer does with a group naming a condition its registry does
not have, which is what happens when the exporter is on a newer BH or a different
mod's tables. Decide where an imported group lands: into the current profile,
into one of its own, offered for review first.

Decide whether the exported form is the same file the layer is stored in or a
separate shape meant to be pasted into a chat window.
