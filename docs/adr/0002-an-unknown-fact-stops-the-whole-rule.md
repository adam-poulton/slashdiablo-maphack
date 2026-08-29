# An unknown fact stops the whole rule

Some facts cannot be known on the path that is asking. An item's price needs the item to exist as a game unit, and items arriving in a 0x9c packet are filtered before that happens, precisely so the packet can be blocked. Facts like these are held as `std::optional`; a condition that reads an absent one marks the evaluation as unknown, and `Rule::Evaluate` then reports no match for the entire rule rather than for that condition alone.

Deciding this at the rule rather than at the condition is what makes negation safe. Were an absent fact to simply make its condition false, `!PRICE<5000` would match every item on the ground, and a rule written to exclude cheap items would instead select everything.

## Considered options

Returning false from the condition is simpler and matches what the previous stub implementations did, but it carries the negation problem above.

Propagating a third value through the operators and the shunting yard handles every combination correctly and is the more complete answer. It was rejected as scope: it changes every operator and the rule evaluator, and it was not worth riding along with the collapse of the condition hierarchy.

## Consequences

This replaces a worse behaviour rather than introducing a new one. `RequiredLevelCondition` previously returned true unconditionally from packet data, so any `REQLVL` comparison matched every item on the ground. Under this policy such a rule matches nothing until the value can be derived, which is wrong in a direction that hides items rather than announcing them falsely.

Required level is derivable from tables the client already has loaded, so it need not stay unknown. That work is deferred until there are tests able to check a derived value against `D2COMMON_GetItemLevelRequirement`.
