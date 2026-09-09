# Measure what an extra rule list walked first costs

Type: task
Status: open

## Question

Every item on the ground is named every frame and every item in every room is
asked about the automap every frame, against 4,010 rules. The layer adds a second
list walked before that one.

Measure what that costs before the design assumes it is free. The measurement can
be taken today without building anything: add rule lists of a few sizes to a
config, in a busy room, and record the cost of the walk against the existing
verdict caching.

Produce a number per extra rule and a view on where the layer stops being cheap.
The answer feeds any ticket that wants to walk more than one rule per item -
simulation and the safety panel in particular.

`tools/profiling/` holds what the project already uses for this.
