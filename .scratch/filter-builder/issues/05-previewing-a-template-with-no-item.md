# What an action template looks like previewed with no item

Type: prototype
Status: open
Blocked by: 04

## Question

Preview is the action template's own appearance - the label and the automap mark
as they would be drawn - with no item involved. But a template is full of tokens
that only mean something against an item: `%NAME%`, `%FRES%`, `%GEMLEVEL%`.

Decide what those render as when there is nothing to substitute. A stylised
placeholder, a plausible sample value, the token itself shown as a token, or
something else. Get it wrong and the preview either lies about the width and
colour of the finished label or is unreadable as a label at all.

Make a rough drawing of the candidates and react to them rather than arguing it
in the abstract. Link the prototype from this ticket.
