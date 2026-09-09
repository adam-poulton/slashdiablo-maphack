# Prior art: how existing loot-filter builders present rule building

Type: research
Status: resolved

## Question

How do the loot-filter builders players already use present the job of building
a rule, and what do their users complain about?

Worth reading: Path of Diablo's and Project Diablo 2's filter tooling, the D2R
item-filter community's editors, `BeLikeLeBron/bhconfig`'s user and edit guides
(linked from the header of `Packaging/BH.cfg`), and the Path of Exile filter
editors, which solved the same problem at far greater scale.

What we want out of it: which parts of a rule these tools make structured and
which they leave as text, how they show what a rule will catch, how they handle
a vocabulary too large to fit on one screen, and where their users get stuck.
Not to copy a design, but so that our own decisions are made against what has
already been tried.

## Answer

Findings: `.scratch/filter-builder/research/01-prior-art-loot-filter-builders.md`

Surveyed BH/`bhconfig`, Path of Diablo, Project Diablo 2, FilterBird, D2R's
native filter (Reign of the Warlock), the D2R web builders, Last Epoch and
FilterBlade, from their own documentation, their own UI and their own issue
trackers.

**Structured versus text.** No mod in the D2 lineage ever shipped a rule editor.
PoD and PD2 kept `ItemDisplay[conditions]: actions` as text and answered the
usability problem by growing the *language* (named styles, item lists, macros,
aliases, block conditionals, a formula sub-language) and by shipping syntax
highlighting. Text is what makes a filter shareable and versionable, which is
why they kept it. The two structured editors sit at opposite poles: Last Epoch
gives a full rule editor over a deliberately tiny vocabulary (five condition
kinds, four actions, drag to reorder) with no escape hatch, and pays for it with
rules that cannot be expressed at all. FilterBlade never lets the player author
a condition: the unit of editing is dragging a named item between lettered
tiers, and raw text survives only as a quarantined "for experts" panel that
emits lines *out* of the GUI, never in.

**Preview.** Only FilterBlade previews an item that has not dropped, via three
distinct surfaces that map almost exactly onto this effort's settled
preview/test/simulation split: a Loot Generator with a zone level and a
"Generate valuable loot" bias button, "Import Item from PoE" for a real subject,
and an "Item Builder" for a constructed one. Its tier rows render the decorator
with no item involved, so the action template preview lives inline in the
editor. Attribution runs both ways ("find out why an item inGame is
highlighted", right-click an item to jump to its rule). FilterBird, the D2
lineage's answer, is an out-of-game simulator that names the rule responsible
for a verdict, and it demonstrates the hazard: it keeps its own copy of the item
facts and has rotted out of date with PD2. Last Epoch and D2R have no preview at
all.

**Vocabulary too large for a screen.** PD2/PoD pushed it entirely onto the text
editor and a 60-entry wiki table of contents. FilterBlade uses a four-level
accordion of 17 sections grouped by *where the player is in the game* (Currency,
Endgame, Campaign, Misc), each level with its own RESET, plus a global Ctrl+K
search and right-click-to-jump. Last Epoch simply shrank the vocabulary, which
is not open to us. D2R makes the card both the index and the editor.

**Curated default plus a layer.** FilterBlade is built around exactly this: the
player's filter is a stored *diff* re-applied to the newest curated filter on
export, with an explicit precedence ("Global One-Click Edits ... applied BEFORE
(i.e. have less priority than) your other changes"), a "Review changes" diff
view, and reusable Modules. Everyone else forks: PD2's "Copy to Local" costs you
updates, and bhconfig states flatly that "any changes you make to BH.cfg will
need to be maintained by the user". The strongest single finding is FilterBlade's
own warning on importing a hand-edited filter: it can simulate one but "CAN'T
reliably use the customizer with uploaded filters. There WILL be bugs." A
structured editor cannot attach to a file of lines. That supports the settled
decision (separate layer, migration by deletion) and says issue 12 should treat
imported `ItemDisplay` lines as a seed for new structured rules, never a round
trip.

**Safety check.** Nobody has one. FilterBlade comes closest with "The Customizer
is built fail-save ... You will see warnings if you're about to do something
dangerous", plus market-price-driven auto-tiering so value is data rather than
opinion, and it is still not enough: the most common issue class on NeverSink's
tracker is a valuable item silently hidden at some strictness (#288 Kingmaker,
#275 Imperial skean "worth 40-50d", #293, #297). Path of Diablo has the one
genuinely defensive decision in the lineage: an invalid expression "always
return[s] true to prevent accidentally hiding items that shouldn't be hidden".
BH already ships the germ of the check in the `[blocked]` tag on a live item that
would have been blocked as a packet item. The community substitute everywhere
else is a hand-maintained protective rule pinned to the top of the list. A
standing, curated, player-extendable safety panel would be novel, and the
evidence it is needed is a years-long backlog of "your filter hid my expensive
item".

**Where users get stuck.** Ordering goes invisible once a UI is organised by
category ("I want to change the Priority of 2 Rules but i cant find this
otpion"). Inconsistent affordances read as bugs ("there's no 'New Rule' button
at the bottom of the essence section"; a tier assignment that works below Strict
but not above). A preview that disagrees with the game destroys trust in the
preview. Rule budgets are felt at once (Last Epoch's 75-rule cap, called "very
arbitrary and silly"). Beginners stop at raw text and hex colours. And
bhconfig's own guide has to explain in bold that ping tiers "will **never** hide
items", which is exactly the tier-versus-filter-level confusion `CONTEXT.md`
names, and an argument for making the two controls structurally distinct.
