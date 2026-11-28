Changelog
=========

All notable changes to slashdiablo-maphack. Versions match the `VERSION` string
in [BH/Constants.h](BH/Constants.h); releases are tagged `v` plus that number.
A release candidate adds a suffix, as in `v1.11.0-rc.1`, and is published as a
GitHub pre-release with the notes under `# Unreleased`.

# Unreleased
* Add a `Monsters` tab to the Forge, where you style how monsters are marked on the map. Each rank (normal, minion, champion and boss) has a marker of its own, with the same icon, size, colours, border and line as the `Clickables` tab offers. Under them is a list of rules: each one names the monsters it is for, picked by name from a searchable list of monsters and super uniques, and can also ask for a rank, a difficulty, a modifier such as `Cursed` or a Conviction aura, and a monster level range, with any difficulty and any level matching until you set one. The ubers are listed apart from the act bosses the game names them after, as `Diablo Clone`, `Pandemonium Diablo`, `Uber Duriel`, `Uber Izual`, `Uber Mephisto`, `Uber Baal` and `Lilith`; type `uber` to find them all. A rule changes the icon, the line or both, and leaves the rest as it found it; choose `None` for the icon to hide what it matches. Rules apply from the top down and add up, so one can set the icon and another the line; where two set the same thing the lower one wins, and `Move up` and `Move down` reorder them. Point at the `Rules` heading for a reminder.
* The monster settings in `BH_settings.cfg` (`Monster Color`, `Monster Line`, `Monster Hide`, `Super Unique Color`, `Enhancement Color` and `Aura Color`) now only start the Forge off: they show up as its rank colours and rules, monsters that shared a colour gathered into one rule, and mark monsters exactly as before until you change something there. Once you do, the Forge's own `BH_forge.json` is what counts, and the lines can be deleted from `BH_settings.cfg`. The Cow King's herd, which BH has always marked in a colour of its own, is now an ordinary rule that can be restyled or removed, and `Cow King's herd` can be picked for a rule like any monster. The four monster colours have moved from the Map tab to the Forge.
* Add a session tracker, under `Session` on the Display tab, on by default. It follows a whole sitting on a character across games: time since the session started, time spent in games, experience gained and lost, experience per second over the session and over the last minute, levels gained, deaths, the monsters that died around you and the drops your item filter pinged. Its figures are new automap variables (`%SESSIONTIME%`, `%SESSIONPLAYTIME%`, `%SESSIONXPGAINED%`, `%SESSIONXPPERSEC%`, `%RECENTXPPERSEC%`, `%SESSIONLEVELS%`, `%SESSIONDEATHS%`, `%SESSIONKILLS%`, `%SESSIONDROPS%`), and with `Keep session history` each session is written as it happens to a file of its own in `BH_sessions`. The run tracker, its variables and its CSV file are unchanged.
* Add the race timer, a tool of its own beside the session tracker, under `Race` on the Display tab. A run is timed against a route, and BH marks each split as the game reaches it: an act boss dying, a quest done, an item picked up, an area entered or a level reached. The Race window is where a route is picked and a run started and stopped, and `.run start`, `stop`, `split`, `undo` and `skip`, or a hotkey for each, do the same from anywhere. `.route` picks the route from chat. Tick `Start a run when a new character joins its first game` and a new character starts one by itself. BH comes with routes for the five act bosses of Normal, the bosses through Hell, a Normal run's milestones, a single act, and every quest with the Cow King and level 40. Each split is shown ahead of or behind your best finished run on that route, with your fastest stretches in gold, the sum of them, and the best time still possible. Bests are kept in `BH_runs.json` for each route, class and softcore or hardcore, and `Compare runs on` times them in game time or real time. Routes are in `BH_routes.json`, to change or add to. `%RUNTIME%` and `%RUNSPLIT%` are new automap variables.
* Add the race HUD, drawn at the right of the screen while a run is being timed and for a minute after one is finished: the route and its splits against your best, or one line of the split the run is on. Like the session HUD it takes no clicks: `.race hud move` (or `Position HUD` under `Race`) places it, and `.race hud splits` or `.race hud line` changes its view.
* Add the session HUD, on by default with the session tracker: the session's clock, the game's number and time, and experience per second, drawn over the game at the top of the screen. `HUD view` picks one line, a compact three rows, or those with kills, deaths and the last drop, and `HUD background` what it is drawn on. The HUD takes no clicks, so it cannot be moved or changed by accident: type `.hud move` (or bind `Position HUD`) to drag it anywhere and step through its views with the mouse wheel, and `.hud line`, `.hud compact` or `.hud full` (or bind `Cycle HUD view`) to change view. It keeps to the corner or edge you put it nearest, and is not drawn under the game's own panels.
* Add the Session window, opened with `.session` or a hotkey set under `Panel hotkeys`. Its `Overview` tab gives the session's times, games, deaths, experience gained and lost, experience per second over the session and over the last minute and five, and how long the next level is at that rate. Its `Games` tab lists every game under its kind of run (`baal-12` and `baal-13` are both `baal`) with its length, experience and rate, and what a run's finished games average. Its `Kills` tab lists the areas you fought in with what died there, the time spent and the experience earned, the kinds of monster killed, and every unique monster, super unique and act boss with when and where it died. Its `Drops` tab lists every drop your item filter pinged, in the colour it was announced in, with when and where it fell. The search box filters whichever tab is in front. `.session new`, or a hotkey, starts a new session.
* Add a `History` tab to the Session window: every session there is a file of, newest first, with when it started, the character, time played, games, experience and rate, kills and drops. Click one to look through it in the other tabs, and `This session` to go back. `Sessions kept` (200 unless you change it, 0 for all) deletes the oldest files past that number.
* Add the Race window, opened with `.race` or a hotkey set under `Panel hotkeys`. It is one screen, laid out as the Forge is: the routes on the left, and the one clicked on the right with `Start` and `Stop` over its splits. While a run is going those are the run's, split by split against your best; otherwise they are the route's times to beat, with the runs that are over under them. The route clicked is the one runs are timed on. `New`, `Copy` and `Edit` swap the screen for the route's own until `Done`: its name in a box to type over, its splits with `Move up`, `Move down` and `Remove`, and beside them the splits there are to add (every act boss, quest and quest item, the Cow King and all quests done, in each difficulty, or type `level 24` in the box over them for a level), so routes are made and changed without editing `BH_routes.json`. A best time now belongs to the splits it was run on, so a route whose splits have changed starts its bests over instead of being compared with times from another route.
* The Codex shows the item you point at in a pane on the right of the window, instead of in a tooltip that could cover the list. A long description scrolls with the mouse wheel. Narrow the window and the tooltip comes back.
* The keyboard now goes only to the window with focus, and to the game when none has it, so a window left open while you play no longer takes Tab from the automap. Escape still closes every open window.
* Add `Fade unfocused windows` on the Display tab, off by default. BH's windows now stay solid when they do not have focus; turn this on to have them go see-through as they did before.
* Fix choosing a stat for a Codex condition doing nothing but move the list down a row, when the stat clicked lay over the condition below.
* Add `Identify items at Cain automatically` under `NPC menu` on the Input tab, off by default. Picks Cain's identify items for you as soon as you talk to him, whenever he offers it.
* Add `Trade with NPC` and `Gamble with NPC` hotkeys under `NPC menu` on the Input tab, unbound by default. With an NPC's menu up, they pick trade (or trade/repair) and gamble. A key only answers while the menu offers its option, and is the game's the rest of the time.
* Add `Shorten buy cooldown` under `Shopping` on the Input tab, on by default. Cuts the game's 500 ms wait between buying with a right click or selling to an NPC to 250 ms.
* Add `Shop highlights` on the Filter tab, on by default. Colours the items in an NPC's shop that your item filter would mark on the map, in the rule's map colour or a fixed one, and marks the shop tabs that hold one. It follows the filter level and hidden items as the map does. Can also announce the matches in chat, leaving out items whose rule has no drop notification.
* Add gamble marks under `Gambling` on the Input tab. Press F2 over an item in a gamble window to highlight every item of its base, and its exceptional and elite versions with `Match base tiers`. Marks survive a gamble refresh and can be kept between games with `Remember marks`.
* Add `Portal re-entry delay` under `Town portal` on the Input tab, off by default. Lets you take a town portal again sooner after taking one, timed from when you took it: 500 to 3000 ms, 1000 ms by default. Off leaves the game's own wait.
* Move `Gamble refresh` under the new `Gambling` heading on the Input tab.
* Add a refresh button to the gamble window, in the empty slot beside `Close`, drawn in the game's own button art and pressing as the game's buttons do. Hover it for its hotkey. Turn it off with `Show refresh button`.
* Gamble refresh now works at every gambler: Elzix, Alkor and Nihlathak as well as Gheed, Jamella and Anya. It only answers with a gamble window open, so the hotkey no longer turns a trade window or talk menu into one, and it keeps the tab you were looking at (`Keep selected tab`).
* Gamble refresh keeps the gamble window on screen while it refreshes, instead of flickering out to the world and back.
* Gamble refresh now says why it stopped when the NPC is out of reach or does not answer, instead of a bare timeout.
* Fix rare crashes when shift-clicking or ctrl-clicking items to move or identify them while items are being moved.
* Add the Forge, opened with `.forge` or a hotkey set under `Panel hotkeys`, where you style how things are marked on the map. Its `Clickables` tab styles regular chests, locked chests, super chests, containers and trapped ones, each on its own: the icon (a square, circle, diamond, triangle, star, cross or plus, or the game's own chest icon) and its size, the colours of its fill and border, how wide the border is, and a line to it from your character. Either colour can be left out, with the `No color` cell beside the colour picker's preview. Pick a kind from the list, or with the arrow keys, and the preview shows it magnified, with its real size beside it; point at a kind to see what it covers.
* The Forge keeps its styles in `BH_forge.json` beside `BH_settings.cfg`, holding only the markers you have changed. If the file has an error, the Forge says where and leaves the file alone until it is fixed.
* Chests and containers are now told apart on the map: locked chests, trapped chests and containers (including exploding barrels), and super chests. Every kind looks as chests always have until you restyle it. Urns, jars and baskets are now marked as well.
* Super chests in Lower Kurast are marked with the game's own super chest icon and the line `LK Chest Lines` drew, which carries over as the super chest line's colour. The marker shows before the chest's room has loaded, moves onto the chest once it has, and goes away for the rest of the game once the chest is opened. Set `Marked in` to `Every area` to mark the super chests of other areas, and their sparkly chests, too.
* Various UI/UX improvements to BH's windows: they stay solid without focus, and dropdown lists, the colour picker, scrollbars, sliders, selection bands and frames are tidied up, with several misdirected clicks fixed.

# Release Notes for 1.10.1 (2026-10-04)
* Add stat filter conditions to the codex.
* Add `Hide redundant gold piles` toggle in the Filter tab, off by default. Hides gold on the ground while you are already carrying the max gold for your level. Doesn't apply in town.
* Add `Show active area range` on the Map tab, off by default. A dashed line on the map that marks the edge of the area around you that the game is sending monsters, missiles and items for.
* BH now applies its existing patches once at startup, rather than applying and undoing them on every game join and exit.
* Fix certain settings not applying immediately when changed from the in-game menu.
* Fix crash on reloading the item filter when the filter file contained `SkillsList` or `TabSkillList` lines.
* Fix the codex reading a bonus to a random class as an Amazon one. The Hellfire Torch now reads `+3 to Random Class Skill Levels` rather than `+0-6 to Amazon Skill Levels`.
* Fix rare access violation crash on game create/join. Turns this crash into a momentary freeze that recovers. If you encounter this and keep getting the freeze, try waiting ~1 minute before retrying the create/join, then it should return to normal.
* Add `Quick save and exit`, `Fail to join after` and `Hold fail to join for` toggles on the lobby tab, on by default. They are there so that a client that is crashing during client-server connection brokering can be tested without those patches enabled.

# Release Notes for 1.10.0 (2026-09-12)
* Reworks the settings UI. Every setting is now searchable by name. The window is resizable and escape closes it.
* Adds a `Display` tab to the settings UI.
* Renames the `Item` tab to `Filter`
* Renames the `Interaction` tab to `Input`
* The stash export is now reachable from the settings UI, under `Stash export` on the Input tab
* Adds Account Manager, a panel drawn on the login screen listing the accounts you have saved.
  * Add a new entry by entering your details as you normally would and then click 'Save new password'.
  * Click an existing entry to automatically log in; right click one to label it, favourite it or forget it.
  * Favourites sit above the rest, and a label groups the accounts under it.
  * Labelled `Account manager` on the Lobby tab of the settings UI.
  * Accounts are kept in `BH_accounts.json` beside the settings, and the passwords in it are stored as typed, in plain text.
  * This is not a secure password manager, it's a convenience feature.
* Adds in-game active filter switching.
  * `BH (default)` is `BH.cfg`; every other option is a `.cfg` in the `filters` folder in the install directory, named by its file name.
  * Chosen as `Filter source` on the Filter tab of the settings UI, and applied without a restart.
* Add the Codex, an in-game reference window based on the game files.
  * Open it with numpad 9 or `.codex`. Change or disable the binding in settings.
  * Searchable tabs for runewords, unique items, set items, Horadric Cube recipes and item bases, each reached directly by typing `.rw <search>`, `.uni <search>`, `.set <search>`, `.cube <search>` and `.base <search>` in the game chat.
  * See [Codex](docs/Codex.md).
* Add `.help`, also reached as `.commands`, listing every chat command BH answers. See [Chat Commands](docs/Chat-Commands.md).
* Add `AREALVL` and `AREAID` filter conditions. See [Area ids and area levels](docs/Advanced-Item-Display.md#area-ids-and-area-levels).
* Rename the `Smart Scrolls` option to `Hide Redundant Scrolls`. Scrolls on the ground are hidden when every tome you carry is above the threshold, or when you carry no tome that takes them.
* Add `Scroll Visibility Threshold` option so ground scrolls can stay hidden until a tome drops to the chosen quantity. Editable from the Filter tab of the settings UI.
* Remove the legacy item name options `Alt Item Style`, `Color Mod`, `Shorten Item Names`, `Show Ethereal`, `Show Sockets` and `Show Rune Numbers`. Item names are now customised only through [Advanced Item Display](docs/Advanced-Item-Display.md); with it off, items are named as the game names them. Configs that set these keys need no change - the keys are ignored. Use the packaged filter (or write your own) instead.
* Add `Fail To Join`, tuning how long the client waits for a game to open before deeming it a failure to join. Editable from the Lobby tab of the settings UI.
* Add `Join Notice`, tuning how long the failed to join notice stays on screen. Editable from the Lobby tab of the settings UI.
* Make item filtering always ordered. Whether a hide rule (a blank label) hides an item depends on whether it was the first matching rule. A rule ending in `%CONTINUE%` decorates an item rather than settling it, so it does not protect the item from a subsequent hide rule. See [Advanced Item Display](docs/Advanced-Item-Display.md#rule-order-decides-what-is-hidden).
* Add `Show Experience Range`, a ring around your player icon showing how close you have to be to a monster's death to gain experience from it.
* Fix an item display rule marked `%NOTIFY-dead%` still announcing the item in chat when a later rule also matched it. The rule that draws the item on the automap now also decides whether it is announced, as it already did for everything else.
* Fix item display rules using `CLVL` or `CRAFTALVL` keeping the answer they gave before you levelled up. Items already seen were judged again only on rejoining the game; they are now judged again when your level changes.
* Fix Save and Exit freezing the client for several seconds, worst in large or graphically varied levels such as Halls of Vaught and Pit Level 2. (Fixed for 1.13c only)
  * The game capped its background archive reads at 256KB/s, and leaving a game waits on every read still outstanding, so the exit was paced by that cap rather than by any work.
  * Just think about all that time you spent waiting for the game to exit at Nihl, and your CPU was literally being instructed to sleep.
* Fix BH windows not staying where you put them when multiple were on screen.
* Fix BH windows swallowing mouse clicks in menus and lobby under certain circumstances.
* Releases now ship a single archive holding `BH.dll`, `BH.cfg`, `BH_settings.cfg`, `buffs.mpq` and installation instructions, instead of loose files. Release notes are the changelog entry for the version plus the install steps, rather than the commit log. See [Installation](docs/Installation.md).
* Feedback is always welcome. If something in this release reads wrong, behaves wrong or is missing an option you relied on, say so and it can be fixed.

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
  [Advanced Item Display](docs/Advanced-Item-Display.md#rule-order-decides-what-is-hidden).
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
