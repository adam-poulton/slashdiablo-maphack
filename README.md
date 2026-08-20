slashdiablo-maphack
===================

A customized maphack for reddit's slashdiablo D2 server

This maphack is based on BH maphack, written by McGod from the blizzhackers
forum. It was extensively customized for the slashdiablo realm by Deadlock39,
who created versions 0.1.1 and 0.1.2.

Currently works with client versions 1.13c and 1.13d

See [CHANGELOG.md](CHANGELOG.md) for the release history.

Configuration is documented in [docs/](docs/Home.md), which is the source of
truth. It is published to the
[wiki](https://github.com/adam-poulton/slashdiablo-maphack/wiki) when a release
is tagged, so the wiki tracks the latest release while `docs/` on `master` also
covers unreleased changes. These pages originated in the upstream
[planqi wiki](https://github.com/planqi/slashdiablo-maphack/wiki). Send
documentation changes as pull requests against `docs/` rather than editing the
wiki, since wiki edits are overwritten on the next publish.

Major features include:

* Full maphack
  * Monsters, missiles displayed on map
  * Infinite light radius
  * Configurable monster colors (see [Monster Colors](docs/Monster-Colors.md))
  * Indicators of current level's exits
  * Chests shown on the automap
  * Configurable automap overlay offset for non-vanilla resolutions (1.13c only)
* Configurable item display features (see [Advanced Item Display](docs/Advanced-Item-Display.md))
  * Modify item names and add sockets, item levels, ethereality
  * Change colors and display items on the map
  * Permanently show ground items
  * Optional ordered filtering, so hide rules and their exceptions are applied in config order
* One-click item movement
  * Shift-rightclick moves between stash/open cube and inventory
  * Ctrl-rightclick moves from stash/open cube/inventory to ground
  * Ctrl-shift-rightclick moves from stash/inventory into closed cube
* Auto-party: automatically accept party invites (`Party Enabled`, on by
  default, no hotkey)
* Auto-loot permission: automatically grant looting permission to your party
  members. Hardcore characters only, (`Looting Enabled`, on by default, no hotkey)
* Use potions directly from inventory (defaults: numpad + for healing, numpad -
  for mana, numpad / for rejuv)
* Display gear of other players (default hotkey: 0)
* Screen showing detailed character stats, including the next
  FCR/FHR breakpoint (default hotkey: 8)
* Warnings when buffs expire (see "Skill Warning" in config file)
* Quest drop warnings for Mephisto/Diablo/Baal
* Experience Meter
  * Show the current %, % gained, and exp/sec above the stamina bar
* Run tracker
  * Record each run's length, XP gained, kill counts and drops to a file in
    your Diablo `data` directory (off by default)
* Kill counters and kill rates (unique/champion/total) on the automap
* Tabbed in-game info window (default hotkey: numpad 9), currently a runeword
  recipe lookup searchable by runeword name, rune or item type, also reachable
  with `.rw` in chat — see [Info Window](docs/Info-Window.md)
* Gamble refresh hotkey for Gheed, Jamella and Anya (default: F5)
* Reload configs in-game with ctrl+r or numpad 0 (both configurable)

Imports from LoliSquad's branch:
* Cow King and his pack now has a separate color on the minimap
* If your game name consists of word+number, it will guess your next game name to be +1 (x123 -> x124)
  * `Autofill Next Game: True`, defaults to true
* Remembers your last game's password
  * `Autofill Last Password: True`, defaults to true
* You can inspect Valkeries, Shadow Masters and Iron Golems to see what they spawned with or was made of
* Improved in-game color palette (16x16, removed an excess color square that didn't exist)

The hotkeys for all features can be changed in the config file. Hotkeys only
fire while in game with the chat box closed.

Configuration is split across two files, both shipped in
[Packaging](Packaging): `BH_settings.cfg` for settings, and `BH.cfg` for
advanced item display rules.

Another example config can be found in [planqi/bh_config](https://github.com/planqi/bh_config).

# Building

The project builds with MSBuild from `BH.sln`, targeting C++17 with the v142
(VS 2019) or v143 (VS 2022) platform toolset. This is what CI uses:

```
msbuild /t:BH:Rebuild /p:Configuration=Release BH.sln
```

Opening `BH.sln` in Visual Studio and building the `BH` project in Release does
the same thing. To stamp the build with a commit SHA the way CI does, pass
`/p:CustomDefinitions="SHA=<sha>"`.

A CMake build (CMake >= 3.7) also exists. Create a build directory within the
project root, make it the current working directory, then run
`cmake -DBUILD_SHARED_LIBS=TRUE -DCMAKE_WINDOWS_EXPORT_ALL_SYMBOLS=TRUE ..`
followed by `cmake --build . --config Release`. To enable multi-processor
support, set `CXXFLAGS=/MP` before running cmake.
