# Advanced Item Display

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

For example, to write all unique ethereal item names in purple, do this:

    ItemDisplay[ETH UNI]: %PURPLE%%NAME%

## Item Groups

### Normal, Exceptional, Elite

- NORM: matches normal items
- EXC: matches exceptional items
- ELT: matches elite items

### Class Specific Items

There are conditions that refer to class specific items:

- CL1: druid pelts
- CL2: barbarian helms
- CL3: paladin shields
- CL4: necromancer heads
- CL5: assassin katars
- CL6: sorceress orbs
- CL7: amazon weapons

To display all ethereal paladin shields in purple, use this line:

    ItemDisplay[ETH CL3]: %PURPLE%%NAME%

### Weapon Groups

- WP1: axes
- WP2: maces
- WP3: swords
- WP4: daggers
- WP5: throwing weapons
- WP6: javelins
- WP7: spears
- WP8: polearms
- WP9: bows
- WP10: crossbows
- WP11: staves
- WP12: wands
- WP13: scepters
- WEAPON: any weapon

### Armor Groups

- EQ1: helms
- EQ2: body armor
- EQ3: shields
- EQ4: gloves
- EQ5: boots
- EQ6: belts
- EQ7: circlets
- ARMOR: any armor

To display ethereal elite polearms capable of 4+ sockets in purple, use this line in BH.cfg:

    ItemDisplay[WP8 ETH ELT NMAG (SOCK=0 OR SOCK>3) !7o7]: %PURPLE%%NAME%

The exclamation point prevents matching item code 7o7 (ogre axe), which can only have 3 sockets.

## Sockets

You can match items with a certain number of sockets. To display all items with more than 4 sockets in green:

    ItemDisplay[SOCK>4]: %GREEN%%NAME%

For conditions like SOCK that compare with a number, you can use <, >, or =.

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
- ED: matches % effective defense/damage (depending on item type)
- RES: matches resist all
- ID: matches items that have been identified
- DEF: matches total defense
- LIFE: matches +HP items (added 2/25/15, only available in builds after this date)
- IAS: matches increased attack speed (added 2/25/15, only available in builds after this date)

## Marking Items on the Map

You can mark any matching item on the map with a square by putting %MAP% anywhere in the action for the rule. This will put all mage plates on the map as a white square without changing the name:

    ItemDisplay[xtp]: %NAME%%MAP%

This square will be white because a color was not specified; if a color is used before the %MAP% action, then that will be used as the square color. For example, this marks mage plates with blue squares:

    ItemDisplay[xtp]: %NAME%%BLUE%%MAP%

## Example Configuration

Here is a sample configuration file containing some basic rules that display certain items in purple. Ethereality, sockets, and item level are added to the item name. Inferior items are filtered out. Runes over lem are shown on the map.

    //Item Display Configuration
    Advanced Item Display: True, None
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