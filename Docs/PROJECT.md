# Project overview and decisions

## What it is

A demake of the Roblox horror game DOORS (LSPLASH) for the Game Boy Color, with an original Game Boy (DMG)
version built from the same source. It's written from scratch in C with GBDK-2020, with assembly where speed
matters, and must run on **real hardware**, not only in emulators. The owner has no Game Boy to test on, so
hardware correctness comes from accurate emulators (BGB, SameBoy, Emulicious) and from following the hardware
rules in DEBUGGING.md strictly.

GB Studio was considered and rejected: its engine limits (fixed actor logic, scene model, no custom
generation) get in the way of procedural rooms, Figure's hearing, Seek's chase and the rest of a feature-full
demake.

The first target is the **Hotel**: the lobby and elevator through Door 100, with every entity, item and
special room of floor 1. See DESIGN.md.

Reference for DOORS mechanics: the owner's Minecraft (Paper plugin) remake of the whole Hotel,
`F:\Minecraft DOORS Project\_Plugin\` (README.md and `docs/`), and the DOORS wiki at https://doorsgame.wiki/
(the owner's choice; not the Fandom one).

## History

- **2026-10-17**: project started. Planning session: the decisions below were made with the owner, and these
  docs were written. Nothing is built yet. Found on the owner's PC: BGB 1.6.4 with DMG and GBC boot ROMs at
  `E:\ROMs and Emulators\_Emulators\BGB (GameBoy)`, devkitPro's MSYS2 (GNU make, git), Python 3.15, Java 25
  (Corretto), Tiled and GB Studio in `F:\Programs`. Not installed yet: GBDK-2020, PyBoy, SameBoy, Emulicious.
- **2026-10-17**: second round of questions: HUD, health, inventory, entity and item scope, shops, saving,
  extras. Answers are in the table below and in DESIGN.md.
- **2026-10-17**: **Phase 0 done.** GBDK-2020 4.5.0, BGB 1.6.6 (64-bit, the project's own copy), SameBoy 1.0.4,
  Emulicious and PyBoy 2.8.1 (Python 3.13 venv) in `tools/`. Makefile + `build.ps1` build both ROMs from one
  source; a hello ROM passes the header check, PyBoy, and BGB with every exception break on, as GBC, DMG, and
  the DMG ROM on GBC. `.gitignore` added (no git commands run; the owner created the repository).
- **2026-10-17**: **Phase 1 done** (engine core): asset converter for Tiled tilesets/rooms and sprite
  sheets, a placeholder hotel tileset and a 30×24 test room, the player, scrolling with collision. Automated
  tests: an AUTOTEST build that walks by itself, checked in BGB with every exception break, a pixel-exact
  scroll test and a VBlank budget measurement in PyBoy. The first scrolling version overran VBlank and was
  redesigned (ARCHITECTURE.md).
- **2026-10-17**: **Phase 2 done** (rooms and doors): title screen with a pixel version of the DOORS logo, the
  reception and 9 room templates chained by a seeded generator, numbered doors, drawers, keys, locked doors,
  walking back, the on-demand HUD, fades. The wallpaper became the Minecraft version's green diamond texture
  (owner's request). A play-testing bot records a 30-room run that AUTOTEST builds replay, so BGB checks the
  whole game, not just a walk around one room.

## Decisions and why

| Decision | Reason |
|---|---|
| Top-down view | Owner's choice (2026-10-17). Closets, drawers, beds, Figure's maze, Halt's hallway, the Library and the Seek chase all read clearly from above, and the Game Boy's tile and sprite hardware is built for it. Side view loses room layouts; a raycaster would run at ~10 FPS in a small window |
| Two ROMs: `doors.gbc` (GBC only) and `doors.gb` (DMG) | Owner's choice (2026-10-17) over one dual-mode ROM. Each ROM can use its console fully (the GBC one assumes double speed, colour palettes, VRAM bank 1 and 32 KB WRAM) without runtime checks everywhere. Same source, compile-time switch (`CGB` / `DMG` target). Every feature has to be tested on both |
| GBDK-2020 (C via SDCC) plus assembly | Owner's choice (2026-10-17). C gets a feature-full game written in reasonable time; hot paths (tile streaming, darkness, audio) go to assembly when profiling says so |
| Solo only, no link cable | Owner's choice (2026-10-17). Entity tuning (Rush's speed, closet counts) is for one player. The Minecraft version's party rules (straggler speed, the trailing-edge Void, `min(alive, 4)` closets) don't apply |
| MBC5 + battery-backed SRAM | Owner's choice (2026-10-17). Up to 8 MB ROM and 128 KB SRAM, supported by every emulator and flash cart. Saves Knobs, achievements, statistics and settings |
| Art drawn as PNG by Claude, converted at build time | Owner's choice (2026-10-17). The owner can repaint any PNG and rebuild. PNGs use the 4-colour Game Boy palette rules (ARCHITECTURE.md) |
| Music in hUGETracker format (hUGEDriver), arranged by Claude | Owner's choice (2026-10-17). hUGEDriver is the standard free driver, works with GBDK, and the owner can open the songs in hUGETracker |
| Rooms authored in Tiled, chained by a seeded generator | Owner's choice (2026-10-17). Hand-made room templates with object markers for doors, closets, drawers, beds; like the real Hotel and the Minecraft version's room schematics |
| Scrolling rooms | Owner's choice (2026-10-17) over one screen per room. Rush can be seen coming down a corridor, and the Library, Courtyard and Seek chase need big spaces |
| Test tools in `tools/`: PyBoy, SameBoy, Emulicious | Owner approved (2026-10-17). PyBoy lets Claude boot builds, press buttons and take screenshots headless; SameBoy and Emulicious cross-check timing-sensitive code |
| The project's own BGB in `tools/bgb` | Owner's request (2026-10-17): the owner's BGB install stays untouched for playing. The project copy keeps every exception break on (DEBUGGING.md) |
| HUD only on demand | Owner's choice (2026-10-17) over a fixed bar. The full 160×144 screen is play field; a strip slides in when health, item or gold changes, or on Select. Closer to DOORS' sparse UI |
| 100 HP health bar | Owner's choice (2026-10-17) over hearts. Matches DOORS, and Eyes' drain and Screech's 40% show exactly |
| Select cycles the held item, Start opens the inventory grid, B uses | Owner's choice (2026-10-17) |
| Guiding Light death screen | Owner's choice (2026-10-17): per-entity hint text, as in DOORS |
| Minor entities: Timothy only | Owner's choice (2026-10-17). Jack, Dread and Glitch left out; Glitch has no job solo |
| Items: lighter, flashlight, battery, vitamins, bandage, lockpicks, skeleton key, crucifix | Owner's choice (2026-10-17). Smoothie and candle left out (later-floor items in DOORS) |
| Pre-run Knobs shop in the elevator + Jeff's Shop for gold | Owner's choice (2026-10-17), as in DOORS |
| Jeff's Shop at Door 55 | Owner's choice (2026-10-17), as in the Minecraft version (DOORS has it at 52) |
| 8-direction movement | Owner's choice (2026-10-17). Smoother in big rooms and the Seek chase |
| No modifiers for now | Owner's choice (2026-10-17). Code should leave room for them |
| Achievements that pay Knobs once, plus statistics | Owner's choice (2026-10-17) |
| No mid-run save | Owner's choice (2026-10-17). SRAM is written only between runs and in the elevator shop |
| Super Game Boy border for the DMG ROM | Owner's choice (2026-10-17). No walkable lobby, no bestiary |
| Separate art per console (`.gbc.png` colour, `.dmg.png` greys, same layout) | Owner's choice (2026-10-17) over grey art + palettes or automatic greys: the best result on each console, at the cost of drawing everything twice |
| Player: blocky Roblox avatar, 16×16, 3/4 view, walk cycle of 2 frames + idle | Owner's choices (2026-10-17). 16×16 is 2 hardware sprites, leaving room on each scanline for entities |
| Camera centred on the player, stopping at room edges | Owner's choices (2026-10-17): "always centred" over a dead zone or look-ahead; stop at walls over showing void |
| Separate rooms with a quick fade between them | Owner's choice (2026-10-17) over a Zelda slide or a seamless corridor: fits memory, any room shape. Entities that travel between rooms (Rush) will be tracked by distance off-screen |
| Doors open with A | Owner's choice (2026-10-17) over opening automatically |
| Door numbers on the door's plate and in the HUD | Owner's choice (2026-10-17) |
| Keys in drawers or lying in the open, mostly drawers (75%) | Owner's choice (2026-10-17). Lock chance 18% and no template repeated within 3 rooms, both from the Minecraft config |
| Exits go up or sideways, never down | Owner's choice (2026-10-17): doors read clearly in 3/4 view and progress is always "onward" |
| The reception (room 0) now, the elevator later | Owner's choice (2026-10-17). Door 1 always locked, key on the wall behind the desk |
| About 8 room templates to start | Owner's choice (2026-10-17); 9 plus the reception were made. More are needed for variety (STATUS.md) |
| Wallpaper = the Minecraft version's `green_terracotta` diamond texture | Owner's request (2026-10-17) |
| Title screen with the DOORS logo, pixelated | Owner's request (2026-10-17), from the logo URL they gave. The logo belongs to LSPLASH; this is a fan project |
| Title screen and seed from the player's timing | A Game Boy powers on identically, so the frame count and divider register when Start is pressed are the only randomness |
| A play-testing bot and a recorded run for AUTOTEST builds | Lets BGB, which can't press buttons, check the whole game under its hardware-rule breaks, and catches anything that breaks a full playthrough |
| Project root `F:\Gameboy Homebrew\Doors` | Owner's choice (2026-10-17), laid out like the Half-Life Wii U project |

## Owner preferences (for AI assistants)

- **Never run `git commit` or `git push`**, and don't stage changes. The owner handles all git operations.
- The owner wants to be asked multiple-choice questions about anything important, big or small, at any point.
- Test in the project's own **BGB**, `tools\bgb\bgb64.exe`. **Never run or change the owner's BGB install**
  (`E:\ROMs and Emulators\_Emulators\BGB (GameBoy)`), which is for playing (owner, 2026-10-17).
- Downloaded tools go in `tools/`, which is gitignored.
- The owner has no real Game Boy, so nothing may rely on emulator leniency (see DEBUGGING.md).
- Documentation lives in this `Docs/` folder, written so the next session can continue without the chat history.
  The style follows `F:\Wii U Homebrew\Half-Life\Docs`.
