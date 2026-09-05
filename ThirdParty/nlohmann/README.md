# nlohmann/json

`json.hpp`, the single-header build of [JSON for Modern C++](https://github.com/nlohmann/json),
version 3.11.3, taken from that release unmodified.

Vendored rather than fetched so that a checkout builds without a network, which
is how every other dependency here is kept. Pinned to a version rather than to
the latest release so that two checkouts of the same commit build the same
thing.

Read by the accounts store, which is the one thing BH both writes and reads JSON
for. `JSONObject.h` remains what the catalogues and the stash export write with:
it serialises and does not parse, which is why this is here.
