# Prior art: how existing loot-filter builders present rule building

Research for `.scratch/filter-builder/issues/01-prior-art-loot-filter-builders.md`.
Read 2026-09-09. Everything below is from the tool's own documentation, its own
UI, or its own issue tracker; secondary write-ups are marked as such.

The tools examined, roughly in order of how close they sit to us:

| Tool | Game | What the player edits |
| --- | --- | --- |
| `BH.cfg` / `BeLikeLeBron/bhconfig` | Slash Diablo (this project) | raw text, whole file |
| Path of Diablo loot filtration | Path of Diablo | raw text, richer language |
| Project Diablo 2 item filtering | Project Diablo 2 | raw text, richer language |
| FilterBird | PoD and PD2 | nothing; it only simulates |
| D2R native loot filter | Diablo II: Resurrected (Reign of the Warlock) | structured cards, in game |
| d2rfilters / DiabloBytes configurator | D2R | structured cards, on the web |
| Last Epoch loot filter | Last Epoch | structured rows, in game |
| FilterBlade | Path of Exile 1 and 2 | structured tier lists over a curated filter |

The headline: **nobody in the Diablo II lineage has ever shipped a structured
rule editor.** Every D2 mod that inherited BH's `ItemDisplay` line kept it as
text and answered the usability problem with better *reference material* and an
*out-of-game simulator*. The two games that did build structured editors, Last
Epoch and Path of Exile, went in opposite directions: Last Epoch built a small
condition vocabulary and a full rule editor with no preview, FilterBlade built a
huge curated filter and let the player move items between tiers, never letting
them author a condition at all.

---

## 1. What is structured and what is left as text

### The D2 text lineage (BH, Path of Diablo, Project Diablo 2)

All three share one rule shape, inherited from BH:

> Each rule has Input and Output and follows this basic format:
> `ItemDisplay[ Input ]: Output`
> ... The Input specifies a set of conditions an item must satisfy for the rule
> to apply to it, and Output specifies the actions (text or keywords) used when
> a matching item is found.
> -- [PD2 wiki, Item Filtering](https://wiki.projectdiablo2.com/wiki/Item_Filtering)

Nothing is structured. Conditions, boolean operators, comparison operators,
colours, tier keywords, map marks and the name text all live in one line of
text. PD2 documents the whole vocabulary as a stack of wiki tables: boolean
conditions, mutable codes, item group codes, item codes, value conditions, info
codes, named attribute codes, numbered attribute codes, skill codes, and three
appendices of numeric IDs. Path of Diablo has grown the *language* rather than a
UI: its filter file now supports ten top-level definition kinds beyond
`ItemDisplay` -- `FilterLevel`, `SkillList`, `ItemList`, `Option`, `Attribute`,
`ItemDescription`, `ItemStyle`, `TextMacro`, `Sound`, `EnableIf`/`EndIf`
([PoD wiki, Advanced Loot Filtration](https://pathofdiablo.com/wiki/Advanced_Loot_Filtration)).
PD2 went further still and added a formula sub-language with operators,
variables and functions.

The reason is visible in the same documents: the text form is what makes a
filter *shareable and versionable*. PD2 lists a dozen public filters, each a
GitHub repository with a season badge, and its launcher subscribes you to one
([PD2 wiki, Item Filtering](https://wiki.projectdiablo2.com/wiki/Item_Filtering)).
The filter is a community artefact first and a personal setting second. A
structured store would have had to invent sharing; text got it free.

The concession to size is *aliasing*, not UI. PD2:

> `Alias[RWBASES]: (NMAG !INF !RW SOCK=0)`
> ... Aliases act as a very simple Find-and-Replace that occurs when a filter is
> loaded.

PoD's `TextMacro`, `ItemList`, `SkillList` and named `ItemStyle` are the same
idea, one step better typed. Both exist so an author writes a condition group
once and a name thereafter, which is the text world's version of a reusable
condition row.

### D2R's native filter (Reign of the Warlock)

D2R shipped an in-game loot filter with the Reign of the Warlock expansion.
The player never sees syntax: rules are cards toggled on or off per item type,
expandable to reach thresholds.

> Toggle Categories: Flip cards on or off for each item type. Expand cards to
> fine-tune rune tiers, gem quality, potion tiers, and gold thresholds.
> -- [DiabloBytes, D2R Loot Filter Configurator](https://diablobytes.com/tools/loot-filter/)

The condition vocabulary is deliberately tiny compared to ours: equipment
rarity, equipment quality (normal / exceptional / elite), equipment category,
ethereal, socketed, gold threshold
([Icy Veins, Loot Filter](https://www.icy-veins.com/d2/loot-filter)). No stats,
no affixes, no area, no character level. The *transport* is JSON pasted from the
clipboard, so the text form survives as an interchange format the player never
authors by hand.

### Last Epoch

The fullest structured rule editor of the lot, and the closest shape to what
this effort is proposing. A rule is: one action, plus zero or more conditions,
plus an optional character-level range. There are exactly five condition kinds,
each with its own picker
([Maxroll, Last Epoch Loot Filter Guide](https://maxroll.gg/last-epoch/resources/loot-filter-guide)):

- **Affix**: multi-select, with tier thresholds behind an "advanced" toggle
- **Class Requirement**: dropdown, multi-select
- **Level**: dropdown of modes, including relative ones like "Max lvl below character level"
- **Rarity**: dropdown
- **Item Type**: hierarchical -- a Type dropdown, then an optional Subtypes window

Actions are four: Show, Hide, Recolor, Emphasize. Recolor and Emphasize are
Show plus a decoration, which is worth noting against our own action vocabulary:
Last Epoch did not separate "keep it" from "how loudly". Rules carry an
enable/disable toggle, a duplicate button and a delete button, and are reordered
by drag.

The structure has hard edges that the guide has to warn about:

> We cannot select multiple Types and also select Subtypes. We also cannot have
> multiple Item Type conditions in a single rule.

That is the cost of structure without an escape hatch: some rules simply cannot
be expressed, and the player has to split them across rules and manage ordering
by hand.

### FilterBlade

FilterBlade is the outlier. The player does **not** author conditions at all.
The unit of editing is a *tier list*: a named item sits in a lettered tier, and
the tier carries the appearance.

> DRAG & DROP items to change the tier. Add new tiers at the bottom or in the
> toolbar. Hover over the wisdom scroll to receive additional advice for
> currency management.
> -- FilterBlade, Customize > Currency > General Currency (observed 2026-09-09)

Tiers are S / A / B / C / D / E+ / E- and each row renders the actual in-game
label for the item that names it. Appearance is edited through "Decorators", and
a Style Editor changes a colour or sound *everywhere it is used*:

> Change reoccurring colors and sounds of the filter. If you change a color
> here, it will affect every entry in the filter that uses that color. Items
> that have the default appearance won't be listed here and can't be changed
> using the Style Editor.

Raw text survives only as a deliberate, quarantined escape hatch, under
Advanced, behind a warning that reads "Only use these tools if you know what you
are doing. Most users won't need to use any of them." One of those tools is the
**Filter Line Translator**, which does the opposite of what one might guess: you
design an appearance in the GUI and it emits the filter line for you to paste
into a hand-written filter. The GUI is a source of text for text authors, never
a consumer of it.

**Takeaway for us.** The two ends are Last Epoch (structure everywhere, tiny
vocabulary, no escape hatch, and rules that cannot be expressed) and PoD/PD2
(text everywhere, unbounded vocabulary, and nobody can read it). FilterBlade
found a third position by making the *condition* an editorial decision it owns,
and giving the player only the assignment of an item to a tier and the tier to
an appearance. Our settled decision -- structure with no raw rule text -- is
Last Epoch's position, and Last Epoch's documented failure mode (a rule that
cannot be written, so write two and get the order right) is the one to design
against.

---

## 2. How each shows the player what a rule will catch

### FilterBird: the D2 lineage's answer, and it is a simulator, not a preview

FilterBird is a browser simulator for PoD and PD2 filters, written by
BetweenWalls, who also authors the Feather filter. It is linked from the PD2
wiki's tooling list as "FilterBird (filter simulation)".

> select an item and see how a filter would display that item, simulating the
> in-game behavior
> ... Determine whether a filter shows what you're looking for ... Discover
> which rules are responsible for displaying/hiding different items.
> -- [BetweenWalls/filterbird README](https://github.com/BetweenWalls/filterbird)

The player configures a character, picks an item, and the tool walks the filter
against it and names the responsible rule. It has an Error Checking mode and an
Auto-Simulate mode, both switchable off "for large or complex filters". The PD2
wiki recommends it as the way to compare filters:

> FilterBird can be used to compare most items between filters one-at-a-time,
> and singleplayer can be used with an item pack and local filter so that the
> filter can be altered and reloaded in-game to compare any number of items.
> -- [PD2 wiki FAQ](https://wiki.projectdiablo2.com/wiki/Item_Filtering)

Its limits are instructive, because they are exactly the *item facts* problem
this effort has: "unique/set items cannot have customized attributes, crafted
items lack predetermined affixes", and a list of condition codes it simply does
not implement (`CHSK`, `PREFIX`, `SUFFIX`, `MAPID` and others). A simulator that
does not share the game's catalogue drifts away from the game, and this one has
drifted: "Many PD2 changes after s6 have not been implemented."

### FilterBlade: three distinct surfaces, which is the same split this effort settled on

FilterBlade's Simulate screen offers exactly three ways to obtain a subject:

- **Loot Generator**: "Generate loot" and "Generate valuable loot" buttons, a
  Zone Level control, and a configurable loot-pile background image
- **Import Item from PoE**: pull a real item off the player's own account
- **Item Builder**: "Create your own items and see how they look in the filter."

That maps almost one to one onto this effort's *simulation* (a random assortment
biased toward the rare end), *test* against a live subject, and a constructed
subject. The "Generate valuable loot" button is notable: it is a deliberate bias
knob on a random sample, which is the same instinct as our rarity slider.

Two more mechanisms are worth stealing conceptually:

> You can right-click e.g. an item to jump to its rule in the Customizer or a
> tierlist element to move it! (Long press on mobile)

> Test & Simulate: Want to test your filter? Verify your results? Find out why
> an item inGame is highlighted?

Attribution runs both ways -- from a simulated item to the rule that produced
its verdict, and from a rule to the item. And the tier list rows themselves are
a *preview with no item*: each row draws the appearance the decorator produces,
so the player sees the action template rendered before any item is involved.
That is precisely issue 05's surface, and FilterBlade puts it inline in the
editor rather than in a separate panel.

The Overview tab is the third thing:

> The Overview tab shows typical items with their current visuals and
> explanations of why they are special. Good to get an overview of the current
> filter and learn about its colors and highlights.

A curated gallery of representative items, used as onboarding rather than as a
test.

### Last Epoch and D2R: nothing

Last Epoch has no preview of any kind. The Maxroll guide, thorough as it is, has
no preview section, and the advice it gives instead is behavioural: keep a
protective Show rule at the very top and check in game. D2R's filter lives in
the game, so the game is the preview, but there is no "what would this catch"
surface, and D2R's own mechanic complicates it further:

> Items still appear on drop: This is normal -- D2R shows all items when they
> first drop. The filter kicks in when you press Show Items.
> -- [DiabloBytes](https://diablobytes.com/tools/loot-filter/)

Worth contrasting with our own model, where a *packet item* can be stopped
before the player ever sees it, so a mistaken hide is genuinely silent. D2R's
filter cannot silently lose an item on drop. Ours can. That raises the value of
the safety check for us above what any of these tools needed.

**Takeaway for us.** Only one tool in the whole survey (FilterBlade) previews an
item that has not dropped, and it does it by *generating* one, *building* one by
hand, or *rendering the decorator alone with no item*. FilterBird proves the
attribution feature is the one people actually reach for: "discover which rules
are responsible for displaying/hiding different items". FilterBird also proves
the maintenance hazard: a simulator with its own copy of the item facts rots.
Ours would be walking the same rules against the same catalogue in the same
process, which is a real structural advantage over every tool here.

---

## 3. A condition vocabulary too large for one screen

Our vocabulary is roughly 40 condition classes in
`BH/Modules/Item/ItemFilter.h` plus the keyword table in `ItemDisplay.cpp`.
Nobody solved this with a flat list.

**PD2 and PoD: paginate the documentation, not the tool.** PD2's Item Filtering
page is a single wiki page with a 60-entry table of contents, four levels deep,
with the item-code tables alone running to thousands of rows. The tooling
answer is syntax highlighting: the wiki's tools list is "FilterBird (filter
simulation) / Item Filter Highlighting (syntax highlighting for Notepad++) /
Loot Filter Hints (syntax highlighting for Visual Studio Code)". In other words,
the vocabulary problem was pushed entirely onto the text editor.

**FilterBlade: a four-level accordion plus a global search.** The Customize
screen is 17 sections in 4 named groups:

- Currency & Rewards: Currency, Unique items, Divination Cards, Vendor Recipes, Gold, Current League
- Endgame (level 68+): Scarabs/Fragments/Splinters, Maps/Contracts/Logbooks, Rares, Special Bases & Crafting, Jewels/Flasks/Tinctures, Highlight specific bases
- Campaign (level 1-67): Campaign & Leveling
- Miscellaneous: League-specific items and mods, Chancing Bases, Miscellaneous

Each section expands into sub-sections (Currency expands to MAIN: General
Currency, Currency Stacks, Campaign Special Rules; EXOTIC CURRENCIES: Delve
Fossils & Resonators, Delirium Orbs, Blight Oils, Heist Rogue's Markers,
Essences, Misc. Exotic Currencies), each of which expands into a tier list. Every
level carries its own RESET button, so a player can undo a branch without
resetting the filter. The groups are named after *where the player is in the
game*, not after the data model. A persistent search bar sits in the header
("Search (Ctrl + K)") and right-click on any item jumps straight to its rule,
which is the real escape from the tree.

**Last Epoch: shrink the vocabulary.** Five condition kinds, and a hierarchical
picker only inside Item Type. This is only possible because Last Epoch's item
model is small. Ours is not, and this option is not open to us.

**D2R: one card per item type, thresholds folded inside the card.** The card is
the unit of both navigation and editing, so there is no separate index.

**Takeaway for us.** Two things generalise. First, group by *when the player
cares*, not by what kind of condition it is -- FilterBlade's top-level split is
Campaign / Endgame / Currency, and D2R's builder opens by asking "Pick your
classes and where you are in the game". Second, a search that jumps to a
condition, and a right-click that jumps from a subject to the rule that judged
it, are what make a deep tree survivable. A tree alone is not enough.

---

## 4. A curated default filter with the player's changes on top

This is the question with the most useful prior art, because FilterBlade is
built entirely around it and everyone else failed at it.

**FilterBlade owns the base and stores only the diff.**

> FilterBlade will ALWAYS give you the newest up-to-date filter. You can
> customize a filter now and load it again later and automatically get an
> updated version when you export!

The player's saved filter is a set of changes against NeverSink's filter, and
exporting re-applies them to the current version. There is an explicit layering
order too, in the Global One-Click Edits panel:

> Quick edits that affect the whole filter and are applied BEFORE (i.e. have
> less priority than) your other changes.

And a way to see the whole diff, under Advanced:

> Review changes: Visualize every single change that you customized in the
> currently active filter.

Reusable change sets are first class:

> Modules are building blocks for filters. If you have multiple filters using
> common changes you can keep the common changes in a module!

**Migrating off a hand-edited text filter is where FilterBlade gives up.** Its
"Test Local Filter" tool carries this warning verbatim:

> WARNING: This severely limits the functionality on FilterBlade:
> Expect bugs!
> You CAN test the uploaded filters in the loot simulator
> You CAN use the advanced->structure screen to change the filter
> CAN'T reliably use the customizer with uploaded filters. There WILL be bugs.
> CAN'T use the 'my filters' system with uploaded filters. Just doesn't work.

The most mature loot-filter builder ever written, after a decade, can *simulate*
an imported hand-written filter but cannot *customize* it. The structured
customizer needs the base filter's own structure to attach to, and a file of
lines does not carry it. This is direct evidence for the settled decision that
the layer is separate from `BH.cfg` and that migration is the player deleting
lines at their own pace, and direct evidence against any plan to parse `BH.cfg`
into editable structure (issue 12 should read this warning).

Even carrying a filter forward between the tool's *own* versions loses work.
[NeverSink-Filter #291](https://github.com/NeverSinkDev/NeverSink-Filter/issues/291):

> I had my 3.27 filter moved to 3.28. Filterblade said it is successfully
> converted. But, it seemed like I had default filterblade filter. ... It
> seemed like it moved all the essences to back to their default filterblade
> strickness of my filter originally had.

**PD2 has no layering: customizing forks the file.**

> Put the customized filter into `ProjectD2\filters\local` and select it from
> "Local" in the launcher. Local filters won't be updated automatically, so if
> yours is based on a public filter, you may want to follow that filter in case
> any important changes are made to it.
> -- [PD2 wiki FAQ](https://wiki.projectdiablo2.com/wiki/Item_Filtering)

The wiki also warns that an old filter is actively dangerous: "Unless the older
filter was extremely simple, it will likely behave in unexpected and undesireable
ways. Using an outdated filter is not recommended."

**bhconfig, our own base filter, says the same in one sentence.**

> any changes you make to `BH.cfg` will need to be maintained by the user
> -- [bhconfig wiki, User Guide](https://github.com/BeLikeLeBron/bhconfig/wiki/User-Guide)

The one thing it does layer is settings: in-game preferences are written to
`BH_settings.cfg`, separately from `BH.cfg`, so a config update does not lose
them. The edit guide's own instructions show how hostile the un-layered file is
to a small change. Moving one unique between tiers is a three-part edit:

> `ItemDisplay[UNI oba]: %GOLD%T4%MAP% %NAME%%TIER-4%` becomes
> `ItemDisplay[UNI oba]: %RED%T3%MAP% %NAME%%TIER-3%`

The colour, the printed label and the tier tag all encode the tier separately
and all three must be changed in step, and then the guide advises physically
moving the line to the right section of the file
([bhconfig wiki, How to edit the config](https://github.com/BeLikeLeBron/bhconfig/wiki/How-to-edit-the-config)).
Three redundant encodings of one fact is exactly the sort of thing a structured
action template removes for free.

**Last Epoch and D2R: profiles, not layers.** Both import a whole filter and let
you keep several. Last Epoch's guide recommends "Duplicate makes an exact copy
of the filter, which comes in handy when making a filter for a similar type of
build", and the community's request is for something better -- a forum thread
asks for the ability "to import filters into existing ones so new rules add to
current ones rather than requiring recreation of all existing rules"
([Last Epoch forums](https://forum.lastepoch.com/t/a-few-loot-filter-editing-suggestions/66733)).
That is a request for exactly the layer this effort has already settled on.

---

## 5. A safety check that warns when a valuable item would be silently hidden

**No tool in this survey has one.** The closest four things:

**FilterBlade: fail-safe editing plus export validation, described but not specified.**

> Did I break it? The Customizer is built fail-save and it's hard to 'break'
> your filter by using it. You will see warnings if you're about to do
> something dangerous. There is also additional validation when exporting.

That is the only claim of a warning-on-danger mechanism found anywhere. Note
what makes it possible: because the player never authors a condition, the tool
knows exactly which named items each edit affects.

**FilterBlade: value is data, not opinion.** NeverSink's filter auto-tiers items
from live market prices, and Premium sync advertises "economy updates for
tierlists of uniques, currency, etc." That is the mechanism that keeps a curated
list of "must never be lost" current without a human curating it. It also breaks:
[#284](https://github.com/NeverSinkDev/NeverSink-Filter/issues/284) "Currency
Tiering is fetching the wrong values from poe.ninja (stash instead of exchange)",
[#290](https://github.com/NeverSinkDev/NeverSink-Filter/issues/290) "Something
with the economy based auto tiering is currently not working at all".

**And it still is not enough.** The single most common issue class on
NeverSink's tracker is a valuable item silently hidden at some strictness:

- [#288](https://github.com/NeverSinkDev/NeverSink-Filter/issues/288) `v8.19.1 - Very-Strict filter hides 5 Divine Orb Unique "Kingmaker" ?` -- body in full: "Not much else to say.. I don't think this should happen."
- [#275](https://github.com/NeverSinkDev/NeverSink-Filter/issues/275) "Imperial skean high qual is hidden on uberstrict while worth 40-50d ... the online lootfilter has not been adjusted accordingly with the market"
- [#297](https://github.com/NeverSinkDev/NeverSink-Filter/issues/297) "Pearl of Tsoatha hidden on Uber Plus"
- [#293](https://github.com/NeverSinkDev/NeverSink-Filter/issues/293) "High value unique Annihilation's Approach hidden"

These arrive as bug reports against the curated filter, from players who found
out the expensive way. There is no standing panel anywhere that says "here is
what your filter currently does to the items you said must never be lost".

**Path of Diablo: fail open at the parser.** The one genuinely defensive design
decision found in the D2 lineage:

> Any invalid expressions will be ignored and always return true to prevent
> accidentally hiding items that shouldn't be hidden.
> -- [PoD wiki, Advanced Loot Filtration](https://pathofdiablo.com/wiki/Advanced_Loot_Filtration)

An unparseable condition matches everything rather than nothing, so a typo
cannot silently hide loot. PoD also colours the filter-load message green or
orange to report whether errors were found, and validates every item code, stat
id, skill id and list name at load. Worth holding against ADR 0002 (an unknown
fact stops the whole rule), which is the opposite reflex applied to a different
case: PoD is talking about a *malformed* condition, ADR 0002 about a
*well-formed* condition whose answer is unavailable. Both are choosing which way
to fail; PoD chose to fail toward showing.

**BH itself already ships the germ of the safety check.** From this repo's
`docs/Advanced-Item-Display.md`:

> To warn users that an item they see in game will be blocked by the packet
> filter, a `[blocked]` tag is added to the item. ... The blocked tag is only
> generated when the item has an ignore rule and not an explicit whitelist rule.

That is a per-item, in-world warning that a *live item* would have been a
*blocked packet item*. The safety list this effort wants is the same idea moved
off the live item and onto a curated set, checked ahead of time instead of after
the fact.

**The community's own workaround is a pinned protective rule.** Last Epoch's
guide, on protecting high-rarity items:

> Make sure this rule is always at the very top. It cannot protect these items
> from any rules above it!

Icy Veins gives D2R players the inverse discipline: "Starting out by hiding
everything with one rule and then building rules to show can be an effective
way". Both are hand-maintained conventions standing in for a check the tool does
not perform, and both fail exactly when the player forgets the ordering.

**Takeaway for us.** A safety check that warns rather than refuses, driven by a
curated and player-extendable list, would be genuinely novel in this space. The
evidence that it is needed is a four-year backlog of "you hid my expensive item"
issues on the most careful filter project in the genre. Two design notes from
the evidence: value drifts, so the list needs to be extendable and ideally
sourced from something that updates; and the warning must be *standing*, not
only fired at Apply, because FilterBlade's export-time validation demonstrably
does not catch these.

---

## 6. Where users get stuck, in their own words

**Ordering is invisible once the UI is organised by category.**
[NeverSink-Filter #272](https://github.com/NeverSinkDev/NeverSink-Filter/issues/272),
titled "Priority Rules":

> I want to change the Priority of 2 Rules but i cant find this otpion. ... 281
> and 282 should be under glassblower rule.

Once rules are grouped by subject, the fact that a walk is ordered stops being
visible, and the player cannot find how to reorder. Directly relevant to issue
09 (order within a layer).

**Structured UIs are not uniformly capable, and the gap reads as a bug.**
[#274](https://github.com/NeverSinkDev/NeverSink-Filter/issues/274), "Can't add
custom rules for essences.":

> there's no "New Rule" button at the bottom of the essence section. ... Was
> hoping to color-code essences, but while the base types seem to be there,
> without adding rules there's not much I can do.

[#269](https://github.com/NeverSinkDev/NeverSink-Filter/issues/269):

> from "Very Strict" and higher strictness, I can't update the filter to show
> all quivers by adding them all to Rank A. I can do it from "Strict" and below.

Both are the same complaint: an affordance present in one part of the tree is
missing in another, or works differently at a different filter level. If our
condition registry renders every condition class the same way, this whole class
of complaint disappears; if it special-cases some, it will not.

**The preview disagreeing with the game destroys trust in the preview.**
[#263](https://github.com/NeverSinkDev/NeverSink-Filter/issues/263): "Gems with
6%+ quality showing ingame but simulate show as not to be shown". FilterBird has
the structural version of the same problem, having drifted out of date with PD2
and never implemented several condition codes.

**Text is where beginners stop.** Summarising a Steam discussion thread on PoE
(secondary): players report that opening an existing filter shows raw text and
hex colours with no clue how to add or remove lines, and that "editing a text
file is not intuitive, and is in fact really easy to screw up if you don't know
what you're doing", with players explicitly asking for an in-game customizable
loot filter "similar to Last Epoch"
([Steam, Path of Exile discussions](https://steamcommunity.com/app/238960/discussions/0/1642041106349139897)).

**Rule budgets are felt immediately.** Last Epoch caps a filter at 75 rules and
the forum thread about it is one of the more active filter threads: "my loot
filter does not hide enough stuff and it is really hard to make a generic loot
filter", "Would it be possible to increase the limit of rules to 125?", and the
suggestion thread calling the limit "very arbitrary and silly", asking for 100
to 150 ([Last Epoch forums, 75 rules limit](https://forum.lastepoch.com/t/loot-filter-75-rules-limit/43505)).
The pressure comes from wanting one filter to serve several characters and the
whole level range at once, which is the same pressure our filter level and class
conditions relieve.

**Strictness is understood as a promise, and players are surprised when it costs
them.** bhconfig's own user guide is unusually honest here and pre-empts the
complaint:

> It will inevitably hide something that someone wished it did not. It's
> advisable to look at the config yourself before using the Aggressive mode.

and separates two things players confuse:

> this setting will **never** hide items. It only controls the notification and
> map box generation.
> -- [bhconfig wiki, User Guide](https://github.com/BeLikeLeBron/bhconfig/wiki/User-Guide)

The FAQ it anticipates is, in its own framing, "distinguishing between items
hidden versus simply not notified". That is precisely the *tier* versus *filter
level* distinction in `CONTEXT.md`, and the fact that the base filter's own guide
has to explain it in bold is evidence the builder should make it structurally
impossible to confuse: a tier control and a hiding control that visibly do
different things.

**Everything else is plumbing, and there is a lot of it.** PD2's FAQ spends most
of its length on file placement, file naming (`default.filter.filter`,
`default.txt`, `default.filter.txt`), text encoding (ANSI versus UTF-8 for
special characters), two installations, nested folders, antivirus, and "Not
saving and closing the filter file after pasting rules into it". A
BH-owned structured store deletes this entire category of support burden, which
is a real and underrated argument for the settled storage decision.

---

## Things this effort should take from the survey

1. **The FilterBlade import warning is the strongest evidence in the file.** A
   structured customizer cannot reliably attach to a hand-edited text filter.
   The settled decision (separate layer, migration by deletion) is the right
   one, and issue 12 should scope importing `ItemDisplay` lines as a *seed for
   new structured rules*, never as a round trip.
2. **Attribution both ways is the feature people actually use.** FilterBird's
   pitch is "discover which rules are responsible", FilterBlade's is "find out
   why an item inGame is highlighted" plus right-click-to-jump. Our test surface
   should name the rule that produced a verdict, and let the player jump from
   that rule back into the editor.
3. **Render the action template inline, not only in a panel.** FilterBlade's
   tier rows *are* the preview. Our issue 05 surface may be better placed beside
   the action fields than as a separate view.
4. **Group the condition vocabulary by when the player cares.** Campaign /
   endgame / class, not by condition class. Pair the tree with a search that
   jumps.
5. **Make ordering visible from wherever a rule is edited.** #272 is a warning
   about what happens when a category tree hides the walk.
6. **Render every condition class through the same row.** #274 and #269 are what
   inconsistent affordances feel like from outside.
7. **The safety check is genuinely novel and clearly needed.** Nothing in the
   survey does it. The evidence it is needed is a steady stream of "your filter
   hid my expensive item" issues against the most careful filter in the genre.
8. **Consider which way a rule fails.** PoD deliberately fails open on a
   malformed condition, precisely to avoid hiding something by accident. Our
   equivalent question is what an incomplete draft rule does before Apply.

## Sources

- [Project Diablo 2 wiki, Item Filtering](https://wiki.projectdiablo2.com/wiki/Item_Filtering)
- [Path of Diablo wiki, Loot Filtration](https://pathofdiablo.com/wiki/index.php?title=Loot_Filtration)
- [Path of Diablo wiki, Advanced Loot Filtration](https://pathofdiablo.com/wiki/Advanced_Loot_Filtration)
- [BetweenWalls/filterbird](https://github.com/BetweenWalls/filterbird) and [the live simulator](https://betweenwalls.github.io/filterbird/)
- [FilterBlade](https://www.filterblade.xyz/) (UI observed directly, 2026-09-09)
- [NeverSinkDev/NeverSink-Filter issues](https://github.com/NeverSinkDev/NeverSink-Filter/issues)
- [bhconfig wiki, User Guide](https://github.com/BeLikeLeBron/bhconfig/wiki/User-Guide)
- [bhconfig wiki, How to edit the config](https://github.com/BeLikeLeBron/bhconfig/wiki/How-to-edit-the-config)
- [Maxroll, Last Epoch Loot Filter Guide](https://maxroll.gg/last-epoch/resources/loot-filter-guide) (secondary)
- [Last Epoch forums, Loot Filter 75 Rules Limit](https://forum.lastepoch.com/t/loot-filter-75-rules-limit/43505)
- [Last Epoch forums, A few Loot Filter Editing Suggestions](https://forum.lastepoch.com/t/a-few-loot-filter-editing-suggestions/66733)
- [Icy Veins, D2R Loot Filter](https://www.icy-veins.com/d2/loot-filter) (secondary)
- [DiabloBytes, D2R Loot Filter Configurator](https://diablobytes.com/tools/loot-filter/) (secondary)
- [Project-Diablo-2/LootFilters](https://github.com/Project-Diablo-2/LootFilters)
- This repo: `docs/Advanced-Item-Display.md`, `BH/Modules/Item/ItemFilter.h`, `Packaging/BH.cfg`
