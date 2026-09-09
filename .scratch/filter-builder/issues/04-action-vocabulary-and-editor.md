# The action vocabulary and how an action is edited

Type: grilling
Status: open
Blocked by: 02

## Question

An action says what to do with an item that satisfies a rule: the name, the
description, the automap mark, the tier, whether the item is hidden, whether the
walk continues. Today it is one string full of `%TOKEN%` substitutions -
`%NAME%`, `%GEMLEVEL%`, `%dot-66%`, `%TIER-6%`, `%CONTINUE%`, colour codes.

Decide what an action looks like as fields rather than as a string: which parts
become their own control (tier, map colour, hide, continue) and which stay a
template the player composes. Decide how a template is edited when it is a
mixture of literal text, colour, and tokens that stand for facts about the item -
whether tokens are typed, inserted from a list, or something else. Decide which
tokens the builder offers, and whether that list comes from the same kind of
declaration the conditions get.

Hiding is a first-class action here, not a token.
