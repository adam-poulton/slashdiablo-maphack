By enabling the "Advanced Item Display" configuration parameter, you can customize exactly how items are displayed. When this parameter is enabled, it will supersede these other parameters:

- Show Ethereal
- Show Sockets
- Show iLvl
- Show Rune Numbers
- Alt Item Style
- Color Mod
- Shorten Item Names

To use Advanced Item Display, you will configure one or more rules in BH.cfg. Each rule looks like this:

    ItemDisplay[ ...CONDITIONS... ]: ACTIONS

The CONDITIONS specify a set of conditions an item must satisfy, and ACTIONS specify the actions taken when a matching item is found. The most basic type of action is to simply enter the name you want to see when viewing that item. If you leave the CONDITIONS empty, the rule will match all items. If you leave the ACTIONS empty, the rule will blank out the names of any matching items.

You will normally have multiple rules in your configuration file. The first rule that matches a given item is the one that will be used to display that item (with one exception, the %CONTINUE% action, described below).

The simplest rule has no conditions and no actions:

    ItemDisplay[]:

This rule will match every item in the game, and will prevent the client from showing that item. (So when you hit the alt key, you will see nothing other than gold stacks.) Next is another simple rule that matches every item in the game, and gives them all the same name:

    ItemDisplay[]: Wirt's Other Leg

## Rules with No Actions

In version 0.1.4, items matching rules with no actions were still visible on the ground, but their names were changed to a single space. As of version 0.1.5, this is no longer true; these items are actually filtered out by the game client. As far as your game is concerned, the items do not exist. This is useful for reducing ground clutter from small gold stacks, junk items, etc. But be careful when using rules like this, as they can cause unexpected effects. For example, if you have a rule with no actions that matches health potions, they will effectively be invisible to you and you will not be able to see the potions on your own belt.

As of 1.9.9, an ignore rule is only created when the item name and description are not blank, there is no map action, **and** there is no `%CONTINUE%` statement. Essentially only blank lines can create ignore rules.

Additionally, blank item names are no longer generated. This could occasionally happen in previous releases when a name was set blank but the item was not completely filtered. To warn users that an item they see in game will be blocked by the packet filter, a `[blocked]` tag is added to the item. For example:

![image](https://user-images.githubusercontent.com/39288882/77542745-e28e0f80-6e63-11ea-90c1-770a0a4f8871.png)

The blocked tag is only generated when the item has an ignore rule and not an explicit whitelist rule (could be any valid name or a map condition). 

## Item Codes

The simplest condition consists of an item code. An item code is the unique 3-letter code assigned to each type of item. You can find a full list of item codes here: http://bhfiles.com/files/Diablo%20II/ItemCodes.html.

EDIT: the bhfiles.com list contains some incorrect codes (e.g. gnarled staff is actually cst). A more correct (though less readable) list can be found here: https://github.com/dkuwahara/OmegaBot/blob/master/data/item_data.txt (the item code is the second column).

The item code for a scroll of town portal is "tsc". So to shorten the names of all town portal scrolls in the game to just "TP", you would put this line in BH.cfg:

    ItemDisplay[tsc]: TP

To give all super healing potions (item code hp5) the name "Pan-Galactic Gargle Blaster", you would put this in BH.cfg:

    ItemDisplay[hp5]: Pan-Galactic Gargle Blaster

## Changing Colors

You can change the color of item text. To make your super healing potions appear with red text, do this:

    ItemDisplay[hp5]: %RED%Pan-Galactic Gargle Blaster

The %RED% color will apply to everything that comes after it, until another color is encountered. To display the word "Blaster" in green text (with everything else still red), you would write:

    ItemDisplay[hp5]: %RED%Pan-Galactic Gargle %GREEN%Blaster

These are the colors you can use:

- %WHITE%
- %RED%
- %GREEN%
- %BLUE%
- %GOLD%
- %GRAY%
- %BLACK%
- %TAN%
- %ORANGE%
- %YELLOW%
- %PURPLE%

If you ever had the "Shorten Item Names" parameter enabled, you can use these rules to replicate exactly what it does:

    ItemDisplay[tsc]: %GREEN%**%WHITE%TP
    ItemDisplay[isc]: %GREEN%**%WHITE%ID
    ItemDisplay[vps]: Stam
    ItemDisplay[yps]: Anti
    ItemDisplay[wms]: Thaw
    ItemDisplay[gps]: Ranc
    ItemDisplay[ops]: Oil
    ItemDisplay[gpm]: Chok
    ItemDisplay[opm]: Expl
    ItemDisplay[gpl]: Stra
    ItemDisplay[opl]: Fulm
    ItemDisplay[hp1]: %RED%**%WHITE%Min Heal
    ItemDisplay[hp2]: %RED%**%WHITE%Lt Heal
    ItemDisplay[hp3]: %RED%**%WHITE%Heal
    ItemDisplay[hp4]: %RED%**%WHITE%Gt Heal
    ItemDisplay[hp5]: %RED%**%WHITE%Sup Heal
    ItemDisplay[mp1]: %BLUE%**%WHITE%Min Mana
    ItemDisplay[mp2]: %BLUE%**%WHITE%Lt Mana
    ItemDisplay[mp3]: %BLUE%**%WHITE%Mana
    ItemDisplay[mp4]: %BLUE%**%WHITE%Gt Mana
    ItemDisplay[mp5]: %BLUE%**%WHITE%Sup Mana
    ItemDisplay[rvs]: %PURPLE%**%WHITE%Rejuv
    ItemDisplay[rvl]: %PURPLE%**%WHITE%Full
    ItemDisplay[aqv]: Arrows
    ItemDisplay[cqv]: Bolts
    ItemDisplay[key]: Key

## Adding to Existing Names

Sometimes it's nice to add onto an existing item name. To leave a super healing potion with the same name, but write it in red text, do this:

    ItemDisplay[hp5]: %RED%%NAME%

To prepend "Ninja" to every item name in the game, use this rule:

    ItemDisplay[]: Ninja %NAME%

You can append every item's vendor value to its name using the %PRICE% action, as shown here:

    ItemDisplay[]: %NAME% ($%PRICE%)

To add item level to item names, do this:

    ItemDisplay[]: %NAME% {L%ILVL%}

To show an item's item code, use the following rule:

    ItemDisplay[]: %NAME% [%CODE%]

To add the number of sockets to item names, this would work:

    ItemDisplay[]: %NAME% -%SOCKETS%-

`%BASENAME%` gives the item's base type name rather than its full name, which
is useful for uniques and set items where the displayed name tells you nothing
about what the item actually is:

    ItemDisplay[UNI]: %NAME% (%BASENAME%)

That turns "Wizardspike" into "Wizardspike (Bone Knife)".

## Logical Operators

Often you will want to match multiple conditions. This rule will match any item that satisfies both CONDITION1 and CONDITION2 (since no logical operators are given, the conditions are implicitly ANDed together):

    ItemDisplay[CONDITON1 CONDITION2]: ...

For more complicated conditions, you can use the following logical operators:

- AND
- OR
- ! (logical negation)

The previous rule is exactly equivalent to this one:

    ItemDisplay[CONDITON1 AND CONDITION2]: ...

You can also use parentheses to group conditions. See below for examples of these logical operators.

## Item Quality

Several conditions let you match items of certain quality:

- INF: matches inferior items
- SUP: matches superior items
- MAG: matches magic items
- RARE: matches rare items
- SET: matches rare items
- UNI: matches unique items
- NMAG: matches nonmagical (white/gray) items
- ETH: matches ethereal items
- RW: matches runewords
- CRAFT: matches crafted items

For example, to write all unique ethereal item names in purple, do this:

    ItemDisplay[ETH UNI]: %PURPLE%%NAME%

## Item Groups

### Normal, Exceptional, Elite

- NORM: matches normal items
- EXC: matches exceptional items
- ELT: matches elite items

### Class Specific Items

There are conditions that refer to class specific items:

- CL1 or DRU: druid pelts
- CL2 or BAR: barbarian helms
- CL3 or DIN: paladin shields
- CL4 or NEC: necromancer heads
- CL5 or SIN: assassin katars
- CL6 or SOR: sorceress orbs
- CL7 or ZON: amazon weapons

To display all ethereal paladin shields in purple, use this line:

    ItemDisplay[ETH CL3]: %PURPLE%%NAME%

### Weapon Groups

- WP1 or AXE: axes
- WP2 or MACE: maces
- WP3 or SWORD: swords
- WP4 or DAGGER: daggers
- WP5 or THROWING: throwing weapons
- WP6 or JAV: javelins
- WP7 or SPEAR: spears
- WP8 or POLEARM: polearms
- WP9 or BOW: bows
- WP10 or XBOX: crossbows
- WP11 or STAFF: staves
- WP12 or WAND: wands
- WP13 or SCEPTER: scepters
- WEAPON: any weapon

### Armor Groups

- EQ1 or HELM: helms
- EQ2 or CHEST: body armor
- EQ3 or SHIELD: shields
- EQ4 or GLOVES: gloves
- EQ5 or BOOTS: boots
- EQ6 or BELT: belts
- EQ7 or CIRC: circlets
- ARMOR: any armor

To display ethereal elite polearms capable of 4+ sockets in purple, use this line in BH.cfg:

    ItemDisplay[WP8 ETH ELT NMAG (SOCK=0 OR SOCK>3) !7o7]: %PURPLE%%NAME%

The exclamation point prevents matching item code 7o7 (ogre axe), which can only have 3 sockets.

## Sockets

You can match items with a certain number of sockets. To display all items with more than 4 sockets in green:

    ItemDisplay[SOCK>4]: %GREEN%%NAME%

For conditions like SOCK that compare with a number, you can use <, >, or =.

### Filled sockets (as of BH 1.9.11f)

`SOCK` counts the total number of sockets in an item, whether or not anything
has been put in them. `USEDSOCK` counts only the sockets that have something
inserted (a gem, rune, or jewel). The two together let you distinguish an empty
base from a partially filled one from a completed runeword.

To find four socket bases that are still empty and worth using for Spirit:

    ItemDisplay[SOCK=4 USEDSOCK=0 !RW]: %NAME%%MAP%

To find items someone has already started filling, so you don't vendor them by
accident:

    ItemDisplay[USEDSOCK>0 !RW]: %ORANGE%%NAME%

The matching `%USEDSOCKETS%` variable prints the count, in the same way
`%SOCKETS%` prints the total:

    ItemDisplay[SOCK>0]: %NAME% [%USEDSOCKETS%/%SOCKETS%]

Note that a completed runeword still reports its sockets as used, so pair
`USEDSOCK` with `!RW` when you want socketed-but-unfinished items only.

## Runes, Gems, and Gold

The "RUNE" condition can be used to match different types of runes based on the rune number. To display runes Lem and higher in purple "20 - Lem" format:

    ItemDisplay[RUNE>19]: %PURPLE%%RUNENUM% - %RUNENAME%

The "GEM" condition will match based on gem quality (1=chipped, 5=perfect). To display all flawless and perfect gems in purple:

    ItemDisplay[GEM>3]: %PURPLE%%NAME%

The "GEMTYPE" condition will match on the type of the gem, based on this list: [gem types](Gem-Types). To match all perfect diamonds:

    ItemDisplay[GEM=5 GEMTYPE=2]: ...

To block the game from showing any gold stacks smaller than 1000:

    ItemDisplay[GOLD<1000]:

Unlike other conditions, GOLD can *only* be used to filter out small stacks. It cannot currently be used to change the color or show stacks of certain sizes on the map.

## Skills

It is possible to match bonuses to all skills, bonuses to class skills, bonuses to particular skill trees, and bonuses to individual skills.

### All Skills

To match all items with +2 or more to all skills:

    ItemDisplay[ALLSK>1]: ...

### Class Skills

Find the number of the class from this page: [class list](Classes); next, append that number to "CLSK". This will match grand matron bows with +2/+3 to amazon skills (class number 0 from the list):

    ItemDisplay[amc CLSK0>1]: ...

### Skill Tabs

Find the number of the skill tab here: [skill tabs](Skill-Tabs); then append it to "TABSK". So to match all items with +1 or more to warcries (#34 from that list):

    ItemDisplay[TABSK34>0]: ...

### Individual Skills

Look up the number of the skill on this page: [skills list](Skills). For example, Battle Orders is skill number 149. So to display barbarian helms capable of 3os with +3 to BO in purple:

    ItemDisplay[CL2 ((ILVL>25 SOCK=0) OR SOCK=3) NMAG SK149>2]: %PURPLE%%NAME%

## Other Conditions

Other miscellaneous conditions:

- ILVL: matches against the item level
- QLVL: matches against the item quality level
- ALVL: matches against the item affix level
- CLVL: matches against you character current level
- DIFF: matches against the games current difficulty
- ED: matches % effective defense/damage (depending on item type)
- RES: matches resist all
- FRES: matches fire resist
- CRES: matches cold resist
- LRES: matches lightning resist
- PRES: matches poison resist
- FCR: matches faster cast rate
- FHR: matches faster hit recovery
- FBR: matches faster block rate
- FOOLS: matches against the Fools mod
- LVLREQ: matches against the items level requirement
- ARPER: matches against attack rating based on char level
- MFIND: matches magic find
- GFIND: matches gold find
- STR: matches +STR items
- DEX: matches +DEX items
- FRW: matches faster run walk rate
- MINDMG: matches +MIN damage items
- MAXDMG: matches +MAX damage items
- DTM: matches damage to mana
- MAEK: matches mana after each kill
- REPLIFE: matches replenish life
- REPQUANT: matches repair quantity
- REPAIR: matches repair durability
- ID: matches items that have been identified
- DEF: matches total defense
- LIFE: matches +HP items
- MANA: matches +MANA items
- IAS: matches increased attack speed
- CRAFTALVL: matches the resulting affix level of the item if it were to be crafted by the character you are playing
- USEDSOCK: matches the number of sockets that have been filled (see [Filled sockets](#filled-sockets-as-of-bh-1911f))
- XP: matches when the character you are playing is an expansion character
- CLASSIC: matches when the character you are playing is a classic character
- AMAZON, SORCERESS, NECROMANCER, PALADIN, BARBARIAN, DRUID, ASSASSIN: match when the
  character you are playing is of that class

`XP` and `CLASSIC` take no operator or number; they are used on their own. They
describe your own character, not whoever is holding the item, and let a single
config behave differently on classic and expansion characters:

    ItemDisplay[CLASSIC gcv]: %NAME%%MAP%
    ItemDisplay[XP gcv]:

The class names work the same way, and likewise refer to the class you are playing
rather than any class restriction on the item itself, so one config can be shared
between characters of different classes. This hides keys only while playing an
assassin:

    ItemDisplay[key ASSASSIN]:

They combine with `!`, `AND` and `OR` like any other condition, e.g.
`ItemDisplay[key !ASSASSIN]:` hides keys on every class except the assassin, and
`ItemDisplay[cm1 (SORCERESS OR NECROMANCER)]: %NAME%%MAP%` marks small charms only
on those two classes.

## Marking Items on the Map

You can mark any matching item on the map with a square by putting %MAP% anywhere in the action for the rule. This will put all mage plates on the map as a white square without changing the name:

    ItemDisplay[xtp]: %NAME%%MAP%

This square will be white because a color was not specified; if a color is used before the %MAP% action, then that will be used as the square color. For example, this marks mage plates with blue squares:

    ItemDisplay[xtp]: %NAME%%BLUE%%MAP%

## Item Descriptions (as of BH 1.9.9)

Item descriptions allow users to add custom descriptions to items. For example, we can set a line describing how to make a Hoto runeword with an eligible flail (below).

![](https://cdn.discordapp.com/attachments/524426475957387264/700332221252501604/unknown.png)

Item descriptions are added using curly braces `{}`. The above description was created with the following line.
```
ItemDisplay[NMAG !RW (fla OR 9fl OR 7fl) SOCK=4]: {%WHITE%Heart of the Oak: %ORANGE%KoVexPulThul}
```
It is also valid to set the description in the same line as the item name or item `MAP` actions. For example:
```
ItemDisplay[NMAG !RW (fla OR 9fl OR 7fl) SOCK=4]: %NAME%%MAP%{%WHITE%Heart of the Oak: %ORANGE%KoVexPulThul}
```
Descriptions are processed separately from name and map actions, so a line with only a description will not set an ignore rule. For example, the following would still result in the item being visible in game.
```
ItemDisplay[NMAG !RW (fla OR 9fl OR 7fl) SOCK=4]: {%WHITE%Heart of the Oak: %ORANGE%KoVexPulThul}
ItemDisplay[NMAG !RW (fla OR 9fl OR 7fl) SOCK=4]: %NAME%
```
Descriptions do not white list items. So an item with only a description can still be hidden. For example:
```
ItemDisplay[NMAG !RW (fla OR 9fl OR 7fl) SOCK=4]: {%WHITE%Heart of the Oak: %ORANGE%KoVexPulThul}
ItemDisplay[NMAG !RW (fla OR 9fl OR 7fl) SOCK=4]:
```
Descriptions support any keywords that the normal item name does with the exception of `%CONTINUE%`. The `%CONTINUE%` keyword is not valid inside curly braces, but it can still be used outside curly braces to further modify descriptions. For example:
```
ItemDisplay[NMAG !RW (fla OR 9fl OR 7fl) SOCK=4]: {%WHITE%Heart of the Oak: %ORANGE%KoVexPulThul}%CONTINUE%
ItemDisplay[NMAG !RW ETH fla SOCK=4]: {%NAME% (best base)}
```
Above, the description will be "Heart of the Oak: KoVexPulThul (best base)" for an eth 4 socket flail. The `%NAME%` keyword becomes a replacement token for the description up to that point.

## Native ilvl display (as of BH 1.9.9)

The item level is now displayed within the item properties. Similarly, affix level is shown for magic, rare, and crafted quality items. Affix level is only shown if it is different than item level. Additionally, the user must set "Advanced Item Display" and "Show iLvl" for these features to be active. Below shows some rare gloves with item level and affix level display.

![image](https://user-images.githubusercontent.com/39288882/77383136-54b90400-6d3f-11ea-91d8-554a44c610a3.png)

## In-game item filter modes (as of BH 1.9.9)

The in-game menu supports four options for "Filter Level": None, Minimal, Moderate, and Aggressive. It is up to the BH.cfg to set the behavior for the filter modes. There is a new keyword `FILTLVL` to support these modes. The modes described previously correspond to `FILTLVL` 0, 1, 2, and 3, respectively.

Here's an example of what can be done using the "Filter Level":

![image](https://user-images.githubusercontent.com/39288882/79245509-0aeaa780-7e2d-11ea-889a-d3ee75073c5a.png)

The skull cap is blocked when the filter level is set to moderate but not when it is set to None. This is because of the `FILTLVL>0` condition in the blocking line.


## In-game configurable ping levels (as of BH 1.9.9)

The "Ping Tiers" setting allows the user to control which lines in the config will ping and be drawn on the map. There is a new keyword, `TIER-x` that supports this. For example:

![image](https://user-images.githubusercontent.com/39288882/79245919-a24ffa80-7e2d-11ea-968a-e620e0972b7c.png)

![image](https://user-images.githubusercontent.com/39288882/79245934-a9770880-7e2d-11ea-84fc-7297a43ca512.png)

Above we set the skull cap as a `TIER-2` item. This means that "Ping Tiers" must be set to 2 or more in game in order for this item to ping. The `TIER-x` command impacts only the map-box and notification. It does not impact the item name. All items with an explicit name or a map condition are whitelisted regardless of the "Ping Tiers" setting in game. In this context "whitelist" means that the named- or mapped-item will always show in-game, even if the config had another line that tried to block the item.

For example, in the following situation, the skull cap would be displayed in game regardless of "Ping Tiers", but it will only notify and map when "Ping Tiers" is set to 2 or more.
```
ItemDisplay[!RW NMAG skp]: %NAME%%MAP%%TIER-2%
ItemDisplay[!RW NMAG skp]:
```

If no TIER level is specified, it defaults to TIER-0. This is essentially an unconditional map + notification.

## Craft affix level condition and display (as of BH 1.9.9)

There is a new keyword `CRAFTALVL` that evaluates to the affix level of an item if it were to be crafted by the character you are playing. For example, the below displays the new affix level as part of the item description.
```
 // Magic Amulets [VERBOSE]
 ItemDisplay[MAG amu]: %NAME%{%WHITE%Caster: %ORANGE%Ral %PURPLE%O%WHITE%Perfect %BLUE%Jewel %WHITE%(%CRAFTALVL%)}
```
![image](https://media.discordapp.net/attachments/518165306141573151/700732831508332614/unknown.png)

The keyword can also be used as part of the filter condition. For example:

```
 // Magic Amulets [VERBOSE]
 ItemDisplay[MAG amu CRAFTALVL>89]: %NAME%%MAP%
```

## Ordered item filtering (as of BH 1.9.11f)

By default, a hide rule (a rule with a blank action) only takes effect when no
other rule anywhere in `BH.cfg` gives the item a name or a map marker. Rules are
kept in separate lists internally, and the whitelist is checked without regard
to where the rules sit in the file, so the order you wrote them in is ignored.

This makes broad "hide this whole category" rules impractical. Say you want rare
belts gone at the most aggressive filter level, and you add this line to the top 
of your config:

```
ItemDisplay[FILTLVL=3 RARE BELT]:
```

By default this usually does nothing, because your config will already have
other lines that name or map rare belts. 
Every one of those whitelists the item regardless of where it sits, 
so to actually hide rare belts you have to hunt down each of those
lines and add `FILTLVL<3` to it. Miss one and the belts keep showing, and you
get to repeat the exercise for every category you want to filter.

Setting `Ordered Item Filtering: True` in `BH_settings.cfg` makes each rule's
position in the file significant. An item is hidden when the matching hide rule
comes *before* the matching whitelist rule. The catch-all above then works as
written, from a single line near the top:

```
// near the top of BH.cfg
ItemDisplay[FILTLVL=3 RARE BELT]:

// ... hundreds of lines later
ItemDisplay[RARE]: %NAME%
```

Exceptions go *above* the hide rule rather than below it

### ⚠️ Order now matters, and a broad rule high in the file is destructive

With this setting on, a hide rule suppresses **every** whitelist rule below it
that matches the same item. The failure mode is severe and silent - items simply
stop existing as far as the game client is concerned, with no error and nothing
in the log to tell you which line did it.

The extreme case is a bare catch-all:

```
ItemDisplay[]:
```

At the top of the file with ordered filtering on, that hides every item in the
game, ignoring all of the hundreds of rules beneath it. Your entire config
appears to have stopped working. The same line at the *bottom* of the file is
the harmless idiom it has always been.

Before enabling this, be aware that:

* A rule you wrote as a fallback will behave as a veto if it sits above the
  rules it was meant to fall back to. Move fallbacks to the bottom.
* The default config was written with this ordering in mind and places the hide
  rules at the bottom, if usure you should re-read yours top to bottom before 
  trusting it.
* The broader a hide rule's conditions, the higher the cost of putting it early.
  Prefer conditions that are as narrow as the intent (`FILTLVL=3 RARE BELT`
  rather than `RARE` or nothing at all).
* Hidden items are filtered out at the packet level, so as far as your client is
  concerned they never dropped. There is nothing to reveal in game and no
  in-game way to tell a hidden drop from one that did not happen. If items seem
  to be missing after enabling this, turn the setting back off to confirm
  ordering is the cause, then bisect your config.

The setting defaults to `False`, preserving the older behaviour.

## Excluding items from the run tracker

The [run tracker](Run-Tracker.md) records the items that dropped during each
run. Adding `%NOTRACK%` to a rule's action keeps matching items out of that
record while leaving them displayed normally in game:

    ItemDisplay[tsc]: %NAME%%NOTRACK%

This is useful for items you map or ping for convenience but don't want
cluttering your drop history. Items that drop in town are never tracked.

## Example Configuration

Here is a sample configuration file containing some basic rules that display certain items in purple. Ethereality, sockets, and item level are added to the item name. Inferior items are filtered out. Runes over lem are shown on the map.

    //Item Display Configuration
    ItemDisplay[tsc]: %GREEN%**%WHITE%TP
    ItemDisplay[isc]: %GREEN%**%WHITE%ID
    ItemDisplay[vps]: Stam
    ItemDisplay[yps]: Anti
    ItemDisplay[wms]: Thaw
    ItemDisplay[gps]:
    ItemDisplay[ops]:
    ItemDisplay[gpm]:
    ItemDisplay[opm]:
    ItemDisplay[gpl]:
    ItemDisplay[opl]:
    ItemDisplay[hp1]: %RED%**%WHITE%Min Heal
    ItemDisplay[hp2]: %RED%**%WHITE%Lt Heal
    ItemDisplay[hp3]: %RED%**%WHITE%Heal
    ItemDisplay[hp4]: %RED%**%WHITE%Gt Heal
    ItemDisplay[hp5]: %RED%**%WHITE%Sup Heal
    ItemDisplay[mp1]: %BLUE%**%WHITE%Min Mana
    ItemDisplay[mp2]: %BLUE%**%WHITE%Lt Mana
    ItemDisplay[mp3]: %BLUE%**%WHITE%Mana
    ItemDisplay[mp4]: %BLUE%**%WHITE%Gt Mana
    ItemDisplay[mp5]: %BLUE%**%WHITE%Sup Mana
    ItemDisplay[rvs]: %PURPLE%**%WHITE%Rejuv
    ItemDisplay[rvl]: %PURPLE%**%WHITE%Full
    ItemDisplay[aqv]: Arrows
    ItemDisplay[cqv]: Bolts
    ItemDisplay[key]: Key
    ItemDisplay[tes]: Andy*Duriel Essence
    ItemDisplay[ceh]: Mephisto Essence
    ItemDisplay[bet]: Diablo Essence
    ItemDisplay[fed]: Baal Essence

    // Ignore all inferior items
    ItemDisplay[INF]:

    // Runes and gems
    ItemDisplay[RUNE<20]: %RUNENUM% - %RUNENAME%
    ItemDisplay[RUNE>19]: %PURPLE%%RUNENUM% - %RUNENAME%%MAP%
    ItemDisplay[GEM>3]: %PURPLE%%NAME%

    // Add ethereality, sockets, ilvl to the name
    ItemDisplay[ETH]: Eth %NAME%%CONTINUE%
    ItemDisplay[SOCK>0]: %NAME% (%SOCKETS%)%CONTINUE%
    ItemDisplay[]: %NAME% L%ILVL%%CONTINUE%

    // Polearms
    ItemDisplay[WP8 ETH ELT NMAG (SOCK=0 OR SOCK>3) !7o7]: %PURPLE%%NAME%

    // Body armor
    ItemDisplay[EQ2 ELT ETH NMAG !SUP SOCK=0]: %PURPLE%%NAME%
    ItemDisplay[uui !ETH NMAG !SOCK=1 !SOCK=2 DEF>450]: %PURPLE%%NAME%
    ItemDisplay[utp !ETH NMAG !SOCK=1 !SOCK=2 DEF>505]: %PURPLE%%NAME%
    ItemDisplay[xtp !ETH NMAG !SOCK=1 !SOCK=2 DEF>254]: %PURPLE%%NAME%
    ItemDisplay[xtp !ETH NMAG ED>13]: %PURPLE%%NAME%

    // Paladin shields
    ItemDisplay[CL3 ELT ETH NMAG RES>29 SOCK=0]: %PURPLE%%NAME%
    ItemDisplay[CL3 ELT !ETH NMAG RES>39 !SOCK=1 !SOCK=2]: %PURPLE%%NAME%

    // Barb helms with +3 BO
    ItemDisplay[CL2 ((ILVL>25 SOCK=0) OR SOCK=3) NMAG SK149>2]: %PURPLE%%NAME%

    // Druid pelts with +3 tornado
    ItemDisplay[CL1 ((ILVL>25 SOCK=0) OR SOCK=3) NMAG SK245>2]: %PURPLE%%NAME%

    // Wands with BS/BS capable of 2os
    ItemDisplay[WP12 !wnd !9wn !ywn NMAG !SOCK=1 SK84>1 SK93>1]: %PURPLE%%NAME%

    // Leaf/memory bases
    ItemDisplay[WP11 NMAG (SOCK=0 OR SOCK=2) SK52>2]: %PURPLE%%NAME%
    ItemDisplay[WP11 NMAG (SOCK=0 OR SOCK=4) SK58>2]: %PURPLE%%NAME%

    // Grand matron bows
    ItemDisplay[amc ((SOCK=0 !SUP) OR SOCK=4) NMAG CLSK0=3]: %PURPLE%%NAME%

    // Monarch shields
    ItemDisplay[uit (SOCK=0 OR SOCK=4) NMAG DEF>145]: %PURPLE%%NAME%

## More Information

See the [BH 1.9.9 Pre-Release Notes](https://github.com/youbetterdont/slashdiablo-maphack/wiki/BH-1.9.9-Pre-release-Notes) for detailed changes on this release.