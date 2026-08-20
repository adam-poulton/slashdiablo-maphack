Documentation for slashdiablo-maphack.

These pages live in [`docs/`](https://github.com/adam-poulton/slashdiablo-maphack/tree/master/docs)
in the main repository and are published to the wiki from there when a release
is tagged, so they describe the latest release rather than unreleased work. For
documentation of changes that have not shipped yet, read `docs/` on `master`.

**Edit these pages via a pull request against `docs/`** — changes made directly
in the wiki are overwritten the next time it is published.

The pages originated in the
[planqi wiki](https://github.com/planqi/slashdiablo-maphack/wiki) and were
imported with their history. Pages covering features added after 1.9.9 are new.

## BH Configuration

As of BH 1.9.9, there are two configuration files associated with BH. They are:

1. `BH.cfg`. This contains all of the advanced item display lines (`ItemDisplay`, `SkillList`, and `TabSkillList` lines). This file will be updated by the Slash Diablo Launcher.

2. `BH_settings.cfg`. This file contains all other settings. These settings can be overridden by the in-game menu. This file is up to the user to maintain, though they can start with a template.

Previous to 1.9.9, all configuration was in the `BH.cfg` file.

For examples of configs, see the User Configuration section below.

### Guides

Here are some guides to creating your own configuration files:
* [Advanced Item Display](Advanced-Item-Display.md)
* [Customize Monster Colors on Map](Monster-Colors.md)
* [Automap Info](Automap-Info.md) — the automap overlay, kill trackers, and the
  variables you can display
* [Run Tracker](Run-Tracker.md) — recording each run's statistics and drops to a
  file
* [Info Window](Info-Window.md) — the in-game reference window, its Runewords
  tab and the `.rw` chat command

### Reference tables

* [Classes](Classes.md)
* [Color Palette and Chat Colors](Color-Palette-and-Chat-Colors.md)
* [Gem Types](Gem-Types.md)
* [Monsters](Monsters.md)
* [Skills](Skills.md)
* [Skill Tabs](Skill-Tabs.md)
* [Stats](Stats.md)

### User Configurations
If you have done any customization of your BH.cfg file, feel free to add it here so that others can use your changes.

* M81's Custom Config: [ [Wiki](https://github.com/youbetterdont/bhconfig/wiki/User-Guide) | [BH.cfg](https://github.com/youbetterdont/bhconfig/releases/latest/download/BH.cfg) | [BH_settings.cfg](https://github.com/youbetterdont/bhconfig/releases/latest/download/BH_settings.cfg) ]
  - Defines a tier system that works well with the BH "Ping Level" feature
  - Defines all four filter levels (None, Minimal, Moderate, Aggressive)
  - Versions tracked with GitHub