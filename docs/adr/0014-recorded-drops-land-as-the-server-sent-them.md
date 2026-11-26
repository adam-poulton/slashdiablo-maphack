# Recorded drops land as the server sent them

Most of what BH does to items, the item filter above all, can only be seen with
the item on the ground in front of a character, and a sandboxed game (ADR 0011)
has a level 1 character in town with nothing on the ground. A scenario has to be
able to put the item there, and the character there with it.

## A drop is a recorded packet, played back

BH's item capture records each item packet the server sent as an item landed
(0x9C), with the filter's decision about it. The agent build's `drop` hands one
back to the client: rewritten to be a unit of its own, with an id far above any
the server gives, and to land a given number of tiles from the character, then
passed to the function the game loop gives each batch of packets from the
server (D2Client 1.13c `0xACE20`, which takes the batch in `eax` and its length
on the stack). It is done from the game loop, where the client takes in what
the server sends.

That is the path a real drop takes, BH's packet hook included, so the item is
named, coloured, announced and put on the map by the filter as any drop is. The
filter hides an item by stopping its packet, so a drop the filter hides never
becomes a unit, and `drop` says whether it landed.

The server knows nothing of the item. Clicking it does nothing, and it is gone
when the game is.

## The ground is read as the game shows it

`state` lists the items on the ground in the rooms the client has, each with
the name the game draws for it, which BH's name hook has already made the
filter's. A scenario checks an item by its code and that name, and a count of
none says the filter hid it. Where it lies on screen is left to the screenshot,
since the draw log holds only what BH draws and the game draws the names.

## A character is a copy of a save

A run plays a copy of the character save it is given, from its own Save folder
(ADR 0011), so a run can level, die and be saved without the file it came from
changing. `enter` picks it from the character list, and from the difficulty
dialog single player opens over the list for a character that has finished
Normal.

The driver names that dialog by its own buttons. A control's state at `0x08` is
a set of flags: bit 0 is set while it takes clicks, and a dialog's controls set
bit 3, which is how they are told from the list's buttons still behind them.

## Considered options

Making items in the game's own server, as a cheat would, was rejected. It
means patching the server, and an item made that way reaches the client by a
different path from those players see, where a recorded drop is one the server
really sent.

Building item packets from a description ("a unique Shako") was put off. It
needs every field's width from the game's tables to write a packet the client
reads, and recorded drops already cover the filter's cases, with its decision
to compare against.

## Consequences

Item capture is the way to a fixture for an item: turn it on, play until the
item drops, and the line it writes can be dropped in any run. The 1351 drops in
`BHTests/fixtures/filter-cases.txt` are the start. Characters for scenarios are
kept in `AgentHarness/fixtures/`.
