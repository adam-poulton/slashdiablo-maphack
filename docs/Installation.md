Installation
============

BH is a DLL that is injected into a running Diablo II. It is not an executable
and there is nothing to run on its own.

Requirements:

* Diablo II: Lord of Destruction, patch 1.13c or 1.13d
* Windows
* A launcher or injector that loads `BH.dll` into the game. On slashdiablo this
  is the Slash Diablo Launcher.

## What is in the release archive

| File | Purpose |
| --- | --- |
| `BH.dll` | The maphack itself. This is the only file that has to be replaced when updating. |
| `BH_settings.cfg` | All settings other than item display. Yours to maintain; the in-game settings window writes back to it. |
| `BH.cfg` | Advanced item display rules (`ItemDisplay`, `SkillList`, `TabSkillList`). |
| `buffs.mpq` | Buff and debuff icons used by the screen info overlay. |
| `Installation.md` | This page. |

## Installing

1. Close Diablo II.
2. Unzip the archive into your Diablo II directory - the folder holding
   `Diablo II.exe` and `Game.exe`. All four files sit next to the executables,
   not in a subfolder.
3. Start the game through the Slash Diablo Launcher, or inject `BH.dll` into
   `Game.exe` with the injector you already use.

BH loads its configuration from the directory the DLL is in. If
`BH_settings.cfg` is missing it falls back to `BH_Default.cfg`, and if that is
missing too it starts with built-in defaults and says so in a message box.

## Updating

Replace `BH.dll` and keep the config files you already have. `BH.cfg` and
`BH_settings.cfg` in the archive are starting points, so overwriting them
discards any customisation you have made. Read the release notes for the
version you are moving to: renamed or removed settings are called out there,
and every setting is documented in
[the wiki](https://github.com/adam-poulton/slashdiablo-maphack/wiki).

`buffs.mpq` changes rarely, but replacing it alongside the DLL is harmless.

## Confirming which version is loaded

* Type `.version` in game.
* The version is drawn in the top right of the menus, and in the footer of the
  settings window (numpad 8 by default).

If nothing appears, the DLL was never injected. That is a launcher or injector
problem rather than a BH one.

## Files BH creates

BH keeps the position, size and minimised state of its windows next to the DLL,
so they come back where you left them. Nothing needs maintaining.

With the run tracker enabled, each run's statistics and drops are written to your
Diablo II `data` directory. See
[Run Tracker](https://github.com/adam-poulton/slashdiablo-maphack/wiki/Run-Tracker).
