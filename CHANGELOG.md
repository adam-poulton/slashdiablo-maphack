Changelog
=========

All notable changes to slashdiablo-maphack. Versions match the `VERSION` string
in [BH/Constants.h](BH/Constants.h); releases are tagged by date.

# Unreleased
* Add `Info Window` option (`BH_settings.cfg`, default `True, VK_NUMPAD9`): a
  tabbed in-game reference window, opened with the hotkey or `.info`, closed
  with escape. Its first tab lists the 78 released runeword recipes plus the
  server-side `Plague`, with a search box that filters by runeword name, rune
  name or item type and paging with `PgUp`/`PgDn`. `.rw <search>` opens the tab
  with that search applied. Recipes whose working title is still in `Runes.txt`
  (`Bound by Duty`, `Doomsayer`, `Widowmaker`, `Winter`, `Exile's Path`,
  `The Beast`) are shown under their released names. See
  [Info Window](docs/Info-Window.md).
* Add a `Listhook` drawing component: a paged list with a configurable column
  layout that measures and trims cells to fit their column, so future reference
  tabs don't each hand-roll a table.
* Text input boxes take a hint string shown while they are empty, and their
  clickable area now matches the box as drawn rather than just the text height,
  which left the bottom of the box dead to clicks. They also take focus on the
  press rather than the release.
* Fix window sizes being discarded when a BH window is created before the game
  reports its resolution, which could leave a window with no background and an
  unclickable, undraggable title bar. Also stop `UI.ini` state from triggering a
  config write during startup.
* Fix collapsed BH windows swapping places. Their position was derived from the
  order in which windows happened to be collapsed, so opening one moved the
  others. Each window now stays where it was put, and only the default position
  for a window that has never been moved is offset to avoid overlapping.
* Fix a collapsed window being clamped to the screen by its full window width
  instead of its title bar width, which pulled wide windows back from their
  saved position.
* Fix BH windows consuming mouse clicks while they are not on screen. Windows
  are only drawn in game, but the window procedure routed clicks to them
  everywhere, so a window left open in `UI.ini` could swallow menu and lobby
  clicks with nothing visible. Clicks now reach a window only when it is
  actually being drawn, and the same applies to the controls on its tabs.
* Give text input boxes a focused state: a solid field, a second border and a
  blinking cursor while focused, translucent with dimmed text while not. The
  idle and focused text colors are settable per box.

# Release Notes for 1.9.11g (2026-08-19)
* Add `Monster Curses` option (`BH_settings.cfg`, default `True, None`) to mark
  elite packs carrying the Cursed modifier with a purple `C` on the automap,
  alongside the existing enchantment letters.
* Add `AMAZON`, `SORCERESS`, `NECROMANCER`, `PALADIN`, `BARBARIAN`, `DRUID` and
  `ASSASSIN` item filter conditions, matching the class of the character you are
  playing, so one config can behave differently per class.
* Clarify in the docs that `XP`, `CLASSIC` and `CRAFTALVL` describe the character
  you are playing rather than whoever is holding the item.

# Release Notes for 1.9.11f (2026-08-19)
* Add `USEDSOCK` item filter condition and the `%USEDSOCKETS%` variable, exposing
  the filled-socket count so filters can match socketed items separately from
  completed runewords.
* Add `Ordered Item Filtering` option (`BH_settings.cfg`, default `False`).
  When enabled, hide rules (blank labels) and their exceptions are evaluated in
  config order, instead of any whitelist rule anywhere in `BH.cfg` overriding a
  hide rule regardless of position. This allows a single catch-all near the top
  of the file to hide a whole category. Note that with it enabled a broad hide
  rule high in the file silently suppresses everything below it — see
  [Advanced Item Display](docs/Advanced-Item-Display.md#ordered-item-filtering-as-of-bh-1911f).
* Add `Smart Scrolls` option (`BH_settings.cfg`, default `False, None`) to
  dynamically hide ground scrolls when your tomes are full.

# Release Notes for 1.9.11e (2026-08-06)
* Hotkeys are now restricted to in-game with the chat box closed, so they no
  longer fire while typing.
* Packaging config files updated for the 1.9.11 feature set.

# Release Notes for 1.9.11d (2026-08-05)
* Show the next Faster Cast Rate and Faster Hit Recovery breakpoint on the
  character stats screen.
* Add default game name, password and description to the config:
  `Default Game Name`, `Default Password`, `Default Description`.

# Release Notes for 1.9.11c (2026-08-02)
* Add configurable automap overlay offset, correcting the hardcoded automap
  centering constants. The native terrain and BH icons are shifted together.
  Resolution specific, 1.13c only, defaults to `0` (vanilla behaviour).
  ```
  Automap Offset X: -36
  Automap Offset Y: -4
  ```

# Release Notes for 1.9.11b (2026-07-31)
* Gamble refresh now also supports Anya (in addition to Gheed and Jamella).
* Restore the ctrl+r reload config hotkey, now with an opt out:
  `Ctrl+R Reload Config: False` in `BH_settings.cfg`.

# Release Notes for 1.9.11a (2026-07-30)
* Add gamble refresh hotkey for Gheed and Jamella. Open the gamble screen and
  press the hotkey to close and reopen the interface.
  `Gamble Refresh: True, VK_F5` in `BH_settings.cfg`, also configurable in the
  BH settings pane.
* The reload config hotkey is no longer hard coded.
* Fix experience display precision.
* Add a release workflow; update the build workflow's upload-artifact action.

# Release Notes for 1.9.11 (2025-11-21)
* Removed the Stash Export module. Stash exporting and its Mustache templates
  are no longer available.

# Release Notes for 1.9.10
## New features
### Run tracking and statistics
* Record every run to a file in `<diablo>/data`, with game date/time, current
  level, run length and the items that dropped. Configured with the
  `Run Details[...]` lines in `BH_settings.cfg`; disabled by default.
  * Items dropped in town are not tracked, and `%notrack%` on an ItemDisplay
    line excludes that item.
  * Duplicate drops are suppressed, and you are warned if the file is locked by
    Excel or another process.
  * Add `Run Details Ping Level` to control which drops are recorded.
* Add kill counters and kill rate statistics, available on the automap and in
  the run tracker: `%TOTALKILLED%`, `%UNIQUEKILLED%`, `%CHAMPKILLED%`, their
  `...KILLSPERMIN` / `...KILLSPERSEC` rates, and the `LAST` variants for the
  previous game. Summons, traps and similar units are excluded from the count.
* Add experience and pacing variables: `%GAMESTOLVL%` (estimated games to level,
  cleared when switching characters), `%LASTGAMETIME%`, `%LASTXPPERCENTGAINED%`
  and `%LASTXPPERSEC%`.
* Add average player count.
* Show run stats when entering a game; fix the endgame timer.

### Maphack and UI
* Show the area level on the automap with `%AREALEVEL%`.
* Automatically show the automap on game join.
* Show quest bug status on the automap (only once the quest is complete), and
  add Andariel/Duriel bug flags.
* Add `Quest Drop Warning` for the Mephisto/Diablo/Baal quest drops, centered on
  screen. Skill/buff warnings no longer overlap when several expire at once.
* Add `Skip NPC Quest Messages` (1.13c and 1.13d).
* Add an in-lobby ad blocker.
* Fix the UI sometimes being drawn off screen, and add a toggle to hide it.
* Allow custom colors for enhancements and auras.
* Make the stat range color configurable (`Stat Range Color`).
* Add `Use Rejuv Potion` hotkey.
* Fix lobby error `6FF61787` when the PC sleeps.

### Item display and filtering
* Add `Always Show Items` (default `True, VK_L`) to permanently show items on
  the ground, and `Alt Item Style`, which shows more items while holding alt.
* Add `%BASENAME%` to display an item's base type name.
* Add `XP` and `CLASSIC` filter options.
* Add partial stat matching.
* `COUNT` arguments accept a vertical pipe as a space substitute, allowing
  complex expressions inside them.
* Set and unique ids now support 4 digits, and ids are matched more strictly.
* `ItemConfig` conditions match more strictly; condition groups can be wrapped
  in parens, and bad tokens are reported instead of silently ignored.
* iLvl display changes.
* Add config groups (work in progress).

### Item mover
* The item mover now uses the in-game structures rather than screen
  coordinates, and right/left click coordinates are unsigned.

## Bug fixes
* Remove the StormLib.dll dependency.
* Fix the `D2COMMON_GetLevel` ordinal for 1.13d.
* Fix a null pointer access in `InitializeMPQData`, and stop nesting the
  `weapons` and `misc` loops inside the `armor` loop.
* Don't depend on global initialization order when creating patches.
* Remove patches in a loop, fixing monsters that could not be un-targeted while
  holding alt.
* Attempted fix for a chat colors crash.
* Fix a conviction check, the classic condition, and buff icon positioning.
* Correct a 1.13d offset; fix the quest warning.
* Remove the stray character that some editors choke on, and fix color codes
  mangled by the GitHub web editor.

## Build
* Move to Visual Studio 2019 and C++17.
* Include the commit SHA in the build version.
* Add GitHub Actions workflows to build pull requests (artifacts expire after
  90 days) and to run manual builds.

# Release Notes for 1.9.9
* Add new text replacement colors for glide (with default non-glide colors)
  * coral (red), sage (green), teal (blue), light_gray (gray)
* Split BH config into settings (`BH_settings.cfg`) and advanced item display (`BH.cfg`)
* Color super uniques a specific color much like what already exists for `Monster Color[733]`
  * e.g. `Super Unique Color[3]:     0x0A` to color Rakanishu red on map.
* Adds support for a configurable item description field (in the same location of required level, durability, etc.). The description goes in curly braces `{}` on the 'rename' side of an ItemDisplay line. [more info](docs/Advanced-Item-Display.md#item-descriptions-as-of-bh-199)
* The item level and affix level can now be displayed as part of the item's properties (like required level, durability, etc.). To enable this, "Advanced Item Display" and "Show iLvl" must be on.
* Add support for up to 'gs9' display in game list
* Add filter and ping levels. [more info](docs/Advanced-Item-Display.md#in-game-item-filter-modes-as-of-bh-199)

# Release Notes for 1.9.8
## Bug fixes
* `BOW` and `SCEPTER` item groups now work correctly
* `UI.ini` file frequent file write issue fixed
* Fixed an issue where the ebug tag was applied to eth items that spawned with ED.
* Fixed issue where multiple parties were formed upon game creation. [more info](https://github.com/planqi/slashdiablo-maphack/pull/44)
* Require `%CONTINUE%` to be used to continue processing map commands from different lines. This makes the map commands behave identically to the name commands. Before, all matching `%MAP%` commands were applied regardless of `%CONTINUE%`, so the last matching one would be shown (last one drawn). [more info](https://github.com/planqi/slashdiablo-maphack/pull/42)

## Optimization
* Item name lookup code efficiency improved.
* Item names are cached, so that the lookup code does not need to constantly execute.
* Item map box colors are cached, ditto ^^

## New features
* Added support for filtering on charged skills. Use the `CHSK` keyword. For example, to find level 2 lower resist wands, use `WAND MAG CHSK91>1` as the filter criteria. The skill index is the same as that used for individual skill bonuses. [more info](https://github.com/planqi/slashdiablo-maphack/pull/33)
* Add support for filters based on item quality level. Use the `QLVL` keyword. For example, `SIN QLVL>40` would select all katars capable of spawning staffmods.
* Added `CRAFT` keyword for selecting crafted items. This works similar to `UNI`, `SET`, etc.
* Add option to remove FPS limit in single player
* Add support for 'gs5' display in game list

# Release Notes for 1.9.7
* Add scrollbar when there are more than 8 characters on a realm account
* Support displaying classic stat ranges
* Open mpq in readonly mode instead of making a copy (faster load time)
* Add option to see game difficulty and server in game list

# Release Notes for 1.9.6
* Fix cpu-overutilization toggle issue

# Release Notes for 1.9.5
* add autofill game description option
* add game creation config items to bh settings in game
* make patch for cpu-overutilization optional

# Release Notes for 1.9.4
* Fix connecting to a realm with a 1.13d client
* Show messagebox if no config found on load
* Fix possible hang when loading game list
* Don't hide items that are on the map when detailed notifications are on
* Set elite flag before exceptional and normal

# Release Notes for 1.9.3
* Add option to use item name/color from BH.cfg when showing the drop notification `Item Detailed Notifications`
* Try to keep items inside columns of a width of two when moving items around
* Add class item specific keywords
  * `BAR` `DRU` `DIN` `NEC` `SIN` `SOR` `ZON`
  * They have the same functionality as `CL1` style selectors
* Add item type specific keywords
  * `BELT` `CHEST` `HELM` `SHIELD` `GLOVES` `BOOTS` `CIRC`
  * `AXE` `MACE` `SWORD` `DAGGER` `THROWING` `JAV` `SPEAR` `POLEARM` `BOW` `XBOW` `STAFF` `WAND` `SCEPTER`
  * They have the same functionality as `WP1` and `EQ1` style selectors
* Fixed `JAV`/`WP6` and `ARMOR` selectors

# Release Notes for 1.9.2
* Add custom notification colors `%notify-1%`
  * The number is the same as 'chat color' and is represented in Hex
  * The item needs to be on the map to send a notification (it won't notify with just the `%notify-xx%` setting)
  * Disable notifications for something on the map with `%notify-dead%`
* Add sell price condition to item filter `PRICE`
* Add `Suppress Invalid Stats` option
* Move minimized settings UI with shift-drag
* Add variable stats display for items
* Add quick TP hotkey
* Add TP tome quantity warning
* Add quick ID ability (shift-leftclick)
* Add MINDMG and MAXDMG to the `+` condition pool

# Release Notes for 1.9.1
* Show game patch version (1.13c or 1.13d) while out of game
* Draw lines to LK superchests
* Show monster enchantments on map
* Added several new keywords to ItemDisplay
  * MAXDUR for enhanced durability percent
  * FRES for fire resistance
  * CRES for cold resistance
  * LRES for lightning resistance
  * PRES for poison resistance
  * Stats can now be combined in a limited pool by adding a + between them:
	* STR, DEX, LIFE, MANA, FRES, LRES, CRES, PRES
	* Example config lines:
	  * `ItemDisplay[EQ5 RARE FRW>10 CRES+LRES+FRES>79]: %PURPLE%o %YELLOW%%NAME%%MAP% // GG Boots`
	  * `ItemDisplay[amu !SET FCR>9 (STR+DEX+LIFE>14 OR CRES+LRES+FRES>29)]: %PURPLE%o %YELLOW%%NAME%%MAP% // GG Amulets`
  * FOOLS for Fool's mod. Used without any operators or numbers
    * Example config line:
	  * `ItemDisplay[WEAPON RARE FOOLS ED>199 IAS>10]: %RED%o %YELLOW%%NAME%%MAP%`
  * GOODSK for + skills of any of the user defined good classes
  * GOODTBSK for + skills tab of any of the user defined good tab skills

* To utilize Good Class/Tab skills add the following to the .cfg
```
SkillsList[0]: 			False		// Amazon
SkillsList[1]: 			True		// Sorceress
SkillsList[2]: 			True		// Necromancer
SkillsList[3]: 			True		// Paladin
SkillsList[4]: 			True		// Barbarian
SkillsList[5]: 			True		// Druid
SkillsList[6]:			True		// Assassin
TabSkillsList[0]:		False		// Amazon Bow
TabSkillsList[1]:		False		// Amazon Passive
TabSkillsList[2]:		True		// Amazon Javelin
TabSkillsList[8]:		True		// Sorceress Fire
TabSkillsList[9]:		True		// Sorceress Lightning
TabSkillsList[10]:		True		// Sorceress Cold
TabSkillsList[16]:		False		// Necromancer Curses
TabSkillsList[17]:		True		// Necromancer Poison & Bone
TabSkillsList[18]:		False		// Necromancer Summoning
TabSkillsList[24]:		True		// Paladin Combat
TabSkillsList[25]:		False		// Paladin Offensive
TabSkillsList[26]:		False		// Paladin Defensive
TabSkillsList[32]:		False		// Barbarian Combat
TabSkillsList[33]:		False		// Barbarian Masteries
TabSkillsList[34]:		True		// Barbarian Warcries
TabSkillsList[40]:		False		// Druid Summoning
TabSkillsList[41]:		False		// Druid Shapeshifting
TabSkillsList[42]:		True		// Druid Elemental
TabSkillsList[48]:		True		// Assassin Traps
TabSkillsList[49]:		False		// Assassin Shadow Disciplines
TabSkillsList[50]:		False		// Assassin Martial Arts
```
* The numbers in braces corresponds to the internal code for the skill so it is important to use this exact list.
* If you do not put this in your config you will not be able to use GOODSK and GOODCLSK, but nothing with break.


# Release Notes for 1.9.0
* Configuration changes in UI are saved on UI close and game
* Add monster resistances and % health missing feature
* Add Chat Colors module to color messages from users:
```
Whisper Color[*chat]: 10
Whisper Color[*trade]: 7

```

# Release Notes for BH Maphack v1.8
* Stash export improvements:
  * Add account name to stash export file name
  * Add rare and crafted item names to stash export
* Map boxes are drawn on top of other things
* Add four possible box sizes to draw on the map
  * Big to small: border, map, dot, px
  * example config line:
    * `ItemDisplay[tsc]: %green%+ %white%TP%map-97%%line-20%%border-20%%dot-0a%%px-33%`
      * the new format is border-xx where the xx is the color code
      * things like `%red%%map%` will still work and won't override a border that is set
  * ![Boxes](readme_gfx/map_boxes.png)
  * ![Color palette](readme_gfx/color_palette.png)
* Support multiple ItemDisplay lines with the same key
* Draw all of an item's map config lines, not just the first
* Add some fancy ItemDisplay magic (see example configs)
* Add ability to reload BH config (default key: numpad 0; hard coded: control r)
* Add ability to draw lines to or hide monsters on map
  * `Monster Hide[149]: // chicken`
  * `Monster Line[479]:      0x9B // shenk`
* Add `DARK_GREEN` as a color
* Other color; add Other Extra
  * This enables places like Black Marsh to have lines to The Hole and The Forgotten Tower
  * Add support for various possible paths at the start of act 3
  * Other Extra is for supporting an extra exit on a level (e.g. Hole Level 1 exit from Black Marsh).
* Remove need for Visual Studio Redistributable
* ItemDisplay conditions can now use `&&` for AND and `||` for OR
* `%replacement_strings%` don't need to be in caps
* Fixed toggle key for xp meter (default: numpad 7)
* Updated stats page
  * Custom stats can be added like: `Stat Screen[red_cooldown]: // reduced cooldown`
* Can be loaded on Diablo start

# Release Notes for BH Maphack v1.7a
- A fork of Underbent's v1.6 by Slayergod13

## Updates to Underbent's v1.6 changes:
- BH.Injector
	- Refactored the injection process so that it no longer executes the core maphack logic inside of the loader lock.
		- This resulted in a minor frame rate increase
		- More importantly it allowed the BH.dll to load the Stormlib.dll for the purpose of reading the MPQ files
	- No longer needs to load Stormlib.dll
	- No longer writes out temporary mpq text files
	- Fixed a bug where opening the injector without any windows open would cause the injector to crash
- BH.dll
	- Now loads the MPQ data inside the maphack
- Item Module
	- Now relies on the data read from the MPQ files within the maphack dll
	
## New Features & Bug Fixes:

### BH Config
- Can now read lines of arbitrary length
- Fixed a bug where lines with a single '/' would be truncated instead of waiting for a double slash "//"

### StashExport
_(this module was removed in 1.9.11)_
- New Module Capable of exporting the current characters inventory in [JSON](http://www.json.org) or custom formats using mustache templates
- Uses the MPQ data to figure out the item information
- Templates can be specified in the BH.cfg using mustache syntax: https://mustache.github.io/mustache.5.html
     - Subset of mustache implemented (and some additions):
          - Literals
               - Support for SOME escape characters added (\r\n\t)
          - Variables
          - Partials
              - Added ability to isolate the child scope to prevent infinite recursion in partials (the context would no longer have access to its parent context)
              - {{>partial}} {{>>isolated-partial}}
          - Sections
               - Inverse
               - Conditional (for truthy values)
               - Iterator (for arrays)
               - And some new additions:
                    - Comparisons:
                         - String Equality: {{#key=value}}
                         - String Inequality: {{#key!value}}
                         - Float Greater: {{#key>value}}
                         - Float Less: {{#key<value}}
                         - String In Set: {{#key$value1|value2|value3}}
                         - String Not In Set: {{#key^value1|value2|value3}}
- Added several data structures to support the StashExport module
	 - JSONObject - Used to contain the item data in a generic fashion, also makes the templating MUCH easier
	 - TableReader/Table - Used to read the txt/tbl files in the data directory, these files are used for parsing the item stats
	 - MustacheTempalte - Used for templating text

#### Features
-  Can identify item quality
-  Can identify which unique/set/runeword the item is
-  Can identify the magix prefix/suffixes
-  Attempts to collapse known aggregate stats (all res) using the aggregate name
-  Will collapse identical items into a single entry with a count (useful for runes and gems)
-  Can exclude stats on items that are fixed so that only the important stats are shown
-  Can get stats for jewels that have been placed into a socketed item

### D2Structs
- Adjusted some structures to better state the purpose of some previously "unknown" or unspecified bytes

### ScreenInfo
- Added display for current/added/rate of gain for experience
	 - BH Toggle: "Experience Meter"
	 
### Maphack
- Refactored the rendering pipeline for the automap objects (monsters, items, missiles, etc) so that the frames could be recycled. 
	- This allows the system to reuse calculations from previous frames and only store the draw commands.
	- This can result in a large frame rate increase on slower machines
- Added ability to display chests on the automap

### ItemDisplay
- The predicate parser will no longer use exceptions for control flow.
	- The old design was resulting in a large frame rate penalty that has been aleviated
	
## New Configuration Items & Defaults:
```
// Maphack section:
// Toggles whether or not to show chests on the automap
Show Chests:			True, VK_X
// Controls how many frames to recycle the minimap doodads for (higher values save more frames)
Minimap Max Ghost: 20

// Experience Display
Experience Meter:		True, VK_NUMPAD7

// Stash Export
// Mustache Templates
Mustache Default:	stash
Mustache[stats]: {{#defense}}\n\n    >{{defense}} defense{{/defense}}{{#stats}}\n\n    > {{value}}{{#range}} ({{min}}-{{max}}){{/range}} {{^skill}}{{name}}{{/skill}}{{skill}}{{/stats}}
Mustache[header-unique]: {{#quality=Unique}}**{{^name}}{{type}}{{/name}}{{name}}** (L{{iLevel}}){{#sockets}}[{{sockets}}]{{/sockets}}{{/quality}}
Mustache[header-magic]: {{#quality$Magic|Rare}}**{{^name}}{{type}}{{/name}}{{name}}** (L{{iLevel}}){{#sockets}}[{{sockets}}]{{/sockets}}{{/quality}}
Mustache[header-else]: {{#quality^Unique|Magic|Rare}}{{^isRuneword}}{{^name}}{{type}}{{/name}}{{name}}{{/isRuneword}}{{#isRuneword}}**{{runeword}}** {{type}}{{/isRuneword}} (L{{iLevel}}){{#sockets}}[{{sockets}}]{{/sockets}}{{/quality}}
Mustache[header]: {{>header-unique}}{{>header-magic}}{{>header-else}}{{#count}} **x{{count}}**{{/count}}
Mustache[item]: {{>header}}{{>stats}}{{^isRuneword}}{{#socketed}}\n\n  * {{>>item}}{{/socketed}}{{/isRuneword}}\n
Mustache[stash]: {{#this}}* {{>item}}\n\n{{/this}}
```
