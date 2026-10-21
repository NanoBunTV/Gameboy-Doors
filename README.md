# Roblox DOORS: Game Boy Edition

A fan demake of DOORS, the Roblox horror game by LSPLASH, for the Game Boy Color (and the original Game Boy too).

It's written from scratch in C with [GBDK-2020](https://github.com/gbdk-2020/gbdk-2020), with music on
[hUGEDriver](https://github.com/SuperDisk/hUGEDriver). The whole Hotel is here, from the reception to Door 100,
with its entities, items, shops and special rooms, in a top-down view. Every run's rooms are generated from a
seed, as in DOORS.

## Status

Complete and playable from the lift to the end of Door 100. It has been tested in accurate emulators (mostly
BGB, with every hardware-error break turned on) and by an automated test suite on both consoles, but **not yet
on a real Game Boy**. If you play it on real hardware or find a bug, please open an issue (see
[Reporting a problem](#reporting-a-problem)).

## Features

 - The Hotel, Doors 0–100: the reception, generated rooms with numbered doors, drawers, keys and locked doors,
   and the special rooms. These are the Library (50), Jeff's Shop (55), the Courtyard (89), the Greenhouse
   (90–99) and Room 100.
 - Rush, Ambush, Screech, Eyes, Halt, Hide, Dupe, Timothy, Figure, Seek (two chases), Snares and Jeff, each
   with its own warning signs and rules, and jumpscares for the ones that kill
 - Dark rooms, lit only by what you carry: a lighter, a flashlight (with batteries), or the faint glow around you
 - Items: lighter, flashlight, battery, vitamins, bandage, lockpicks, skeleton key and crucifix, found in
   drawers and on the floor, plus gold to spend at Jeff's Shop
 - The Library's puzzle: eight books, the paper and a five-wheel padlock, while Figure listens for you
 - Hiding in closets and under beds
 - Knobs earned at the end of every run, kept in the save and spent in the lift's shop before the next run
 - Music arranged from DOORS' soundtrack, and sound effects
 - The Guiding Light's hints when you die, then Try again or back to the title
 - A Game Boy Color version in full color, and an original Game Boy version
 - Saves on the cartridge (battery-backed SRAM): Knobs and statistics

## Controls

| Button | Action |
|---|---|
| D-pad | Walk (8 directions); you look the way you face |
| A | Open a door, drawer or closet, pick things up, hide, read, talk |
| B | Use the held item (light on / off, take vitamins) |
| B (held) | Sneak, in the Library and in Room 100 |
| Select (tap) | Change the held item (lighter, flashlight, vitamins) |
| Select (held) | Show the HUD: the time, health, held item and gold |
| Start | Pause and list what you carry. In the Library: the notes (the game doesn't pause). In the lift: leave without buying |

 - **Erasing the save:** hold Select and B on the title screen for 3 seconds, then A to confirm.
 - The HUD also shows on its own when something changes, such as your health or your gold.

## Playing

Download the latest release from the [Releases page](https://github.com/NanoBunTV/Gameboy-Doors/releases/latest).
There are two ROMs:

| File | Runs on |
|---|---|
| `doors.gbc` | Game Boy Color (and Game Boy Advance). Color version; it won't start on an original Game Boy |
| `doors.gb` | Original Game Boy, Game Boy Pocket, Super Game Boy, and also the Game Boy Color and Game Boy Advance |

 - **On an emulator:** use an accurate one, such as [BGB](https://bgb.bircd.org/),
   [SameBoy](https://sameboy.github.io/) or [Emulicious](https://emulicious.net/). The save is written to a
   `.sav` file next to the ROM.
 - **On real hardware:** use a flash cartridge that supports **MBC5 with 32 KB of battery-backed RAM**.

The game saves only between runs: when a run ends, and when you buy something in the lift. A run is played in
one sitting, as in DOORS.

## Known limitations

 - Not tested on a real Game Boy yet.
 - Only the Hotel. The later floors aren't included.
 - Single player only.
 - No achievements, statistics screen or Super Game Boy border yet. Statistics are already recorded in the save.
 - No saving in the middle of a run.

## Reporting a problem

Open an issue with the ROM you played (`doors.gbc` or `doors.gb`), the console or emulator, the door number,
and what happened. A screenshot or video helps.

## Credits

 - [DOORS](https://www.roblox.com/games/6516141723/DOORS) by LSPLASH - the original game, its entities, rooms,
   art and music, which this demake is based on.
 - [The DOORS wiki](https://doorsgame.wiki/) - reference for the game's mechanics.
 - [GBDK-2020](https://github.com/gbdk-2020/gbdk-2020) and [SDCC](https://sdcc.sourceforge.net/) - the
   toolchain and libraries.
 - [hUGEDriver and hUGETracker](https://github.com/SuperDisk/hUGEDriver) by SuperDisk - the music driver and
   the tracker the songs were arranged in.
 - [Tiled](https://www.mapeditor.org/) - the room editor.
 - [BGB](https://bgb.bircd.org/), [SameBoy](https://sameboy.github.io/),
   [Emulicious](https://emulicious.net/) and [PyBoy](https://github.com/Baekalfen/PyBoy) - used for testing
   during development.
 - Claude - AI assistance.

## License

This project's own code is licensed under the GNU General Public License v3 or (at your option) any later
version. Refer to the provided [LICENSE.md](LICENSE.md) file for further information.

## Third-party licenses

### [GBDK-2020](https://github.com/gbdk-2020/gbdk-2020)
library by the GBDK-2020 contributors under GNU General Public License v2 with a linking exception
### [SDCC](https://sdcc.sourceforge.net/) runtime library
by the SDCC contributors under GNU General Public License v2 with a linking exception
### [hUGEDriver](https://github.com/SuperDisk/hUGEDriver)
by SuperDisk, dedicated to the public domain

DOORS is a game by LSPLASH. Roblox is a trademark of Roblox Corporation. Game Boy and Game Boy Color are
trademarks of Nintendo. This is a non-commercial fan project, not affiliated with or endorsed by LSPLASH,
Roblox or Nintendo.

## Building

Requires GNU make (for example from [devkitPro](https://devkitpro.org/wiki/Getting_Started)'s MSYS2), Python
3.13 and PowerShell, on Windows.

1. Download the tools into `tools/` (the versions and where each one goes are in
   [Docs/BUILDING.md](Docs/BUILDING.md)):
   - [GBDK-2020](https://github.com/gbdk-2020/gbdk-2020/releases) 4.5.0 → `tools/gbdk/`
   - [hUGEDriver](https://github.com/SuperDisk/hUGEDriver/releases) 6.1.3 → `tools/hugedriver/`
   - [hUGETracker](https://github.com/SuperDisk/hUGETracker/releases) 1.0.11 → `tools/hugetracker/` (the
     build uses its `uge2source.exe`)
2. Create the Python environment for the asset converter and the tests:
   ```
   python -m venv tools\venv
   tools\venv\Scripts\python.exe -m pip install pyboy pillow mido
   ```
3. Build:
   ```
   .\build.ps1
   ```
   This produces `build\gbc\doors.gbc` and `build\dmg\doors.gb`. `.\build.ps1 -Test` also runs the automated
   tests, which need BGB in `tools/bgb/`.

[Full build instructions and developer notes](Docs/BUILDING.md) · [Developer documentation](Docs/README.md)
