# Condensing the Info window's navigation

Type: prototype
Status: open

## Question

`InfoWindow` builds five panels through `AddPanel` and heads them with tabs
across the top. The builder needs the width those tabs and their chrome take,
and it adds several more panels besides, which the current tab strip will not
hold.

Decide the replacement: side navigation, a collapsing rail, grouped navigation,
or something else. Decide what the navigation costs in width when it is open and
when it is not, what happens to the five existing panels' own layouts, and how
this behaves at the window sizes players actually use.

The number this ticket must produce is the width budget the builder's layout is
designed against; everything else about the builder's layout waits on it.

Build it rough and react to it. Link the prototype from this ticket.
