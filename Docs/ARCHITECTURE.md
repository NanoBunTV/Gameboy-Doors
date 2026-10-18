# Architecture

As of Phase 1 (2026-10-17): the engine core is built: room loading, scrolling, sprites, the player. Sections
marked **planned** aren't built yet.

## Repository layout

| Path | Purpose |
|---|---|
| `Makefile`, `build.ps1` | Build both ROMs: `build/gbc/doors.gbc` and `build/dmg/doors.gb`, plus the AUTOTEST variants (BUILDING.md) |
| `src/` | Game code in C, shared by both targets (module table below) |
| `src/platform/` | Code that differs per target: `cgb/` (double speed, palettes), `dmg/` (BGP/OBP palettes) |
| `assets/tilesets/` | Tiled tilesets (`.tsx`) of 16×16 metatiles, each with a `.gbc.png` and a `.dmg.png` (ASSETS.md) |
| `assets/rooms/` | Tiled rooms (`.tmx`) |
| `assets/sprites/` | Sprite sheets: `<name>.gbc.png`, `<name>.dmg.png` and `<name>.json` (frame names) |
| `assets/music/`, `assets/sfx/` | **Planned**: hUGETracker songs, sound effects |
| `scripts/` | `assets.py` (asset converter), `check-rom.py`, `bgb-check.ps1`, `run-test.py`, `scroll-test.py`, `vblank-budget.py` (DEBUGGING.md) |
| `tools/` | Downloaded tools, not in git (`tools/README.md`) |
| `build/` | Output, not in git: `gen/` (generated C from assets), `gbc/`, `dmg/`, `gbc-autotest/`, `dmg-autotest/` (ROM, `.sym`, `.map`, `obj/`), `screens/`, `preview/` |

### Source modules

| File | What it does |
|---|---|
| `main.c` | Startup (LCDC setup), loading a room, the frame loop, the camera (centred on the player, stopped at the room's edges) |
| `assets.h` | The data types the generated asset files use: `tileset_t`, `room_def_t`, `sprite_sheet_t`; `TILESET_*` ids |
| `room.c/.h` | The current room: map in WRAM, the tileset's metatile tables in WRAM, collision queries |
| `bg.c/.h` | Camera and background streaming into the hardware map (below), with the VRAM writes in inline assembly |
| `sprites.c/.h` | Sprite sheets in VRAM, drawing 16×16 frames as two 8×16 hardware sprites, the per-frame shadow OAM allocator |
| `player.c/.h` | 8-direction walking, collision, facing (8 directions), walk animation |
| `input.c/.h` | Joypad held/pressed; in AUTOTEST builds a fixed walking route instead of the joypad |
| `platform.h`, `platform/*/` | Per-console setup and palette loading |

## Two ROM targets

One source tree, two builds selected by a define (`-DCGB` / `-DDMG`, set by the Makefile):

| | `doors.gbc` | `doors.gb` |
|---|---|---|
| Header CGB flag (0x0143) | `0xC0` (GBC only) | `0x00` (runs on DMG, and on GBC in compatibility mode) |
| CPU | Double speed (`cpu_fast()`) | Normal speed |
| WRAM | 32 KB (banks 1–7 for room buffers) | 8 KB |
| VRAM | 2 banks: more tiles, per-tile attributes (palette, flip, priority) | 1 bank |
| Palettes | 8 background + 8 sprite palettes of 4 colours | BGP, OBP0, OBP1 (4 shades) |
| Art | `<name>.gbc.png` (colour) | `<name>.dmg.png` (greys), drawn separately (owner's decision) |
| Effects | **Planned**: palette-based darkness, flicker, colour per entity | **Planned**: tile-based darkness, simpler effects |

`#ifdef CGB` appears in `src/platform/`, the generated asset files, and where GBC attributes or palettes are
written (`room.c`, `bg.c`, `sprites.c`). Game logic is the same. The DMG build is the performance floor:
anything in the main loop must fit a DMG frame.

## Memory map

| Area | Use |
|---|---|
| ROM bank 0 | Engine code (no `#pragma bank`), GBDK library |
| ROM banks 1+ | Generated asset data (`#pragma bank 255`, placed by GBDK's autobanking). Read with `SWITCH_ROM(BANK(x))` and restore the bank after |
| VRAM 0x8000–0x87FF | Sprite tiles 0–127 (player at 0–35) |
| VRAM 0x8800–0x97FF | Background tiles 0–255 (LCDC bit 4 clear: tiles 0–127 at 0x9000, 128–255 at 0x8800). Sprite tiles 128–255 would overlap this, so sprites stay below 128 |
| VRAM 0x9800 | Background map (32×32), streamed as below. GBC: attributes in VRAM bank 1 at the same address |
| VRAM 0x9C00 | Free (window map, for the on-demand HUD later) |
| WRAM | Room map (`room_map`, up to 3,072 metatiles), metatile tables (`mt_tiles`, `mt_flags`, `mt_attrs`: 128 metatiles max), frame tables, GBDK's shadow OAM at 0xC000, the stack from 0xE000 down |
| SRAM | **Planned**: the save |

LCDC: `LCDCF_BG8800 | LCDCF_BG9800 | LCDCF_OBJ16 | LCDCF_OBJON | LCDCF_BGON` (+ `LCDCF_ON`), set in `main()`.

## Cartridge

MBC5 with RAM and battery (cartridge type `0x1B`). ROM size grows as needed (GBDK's autobanking assigns
banks; the ROM is 32 KB as of Phase 1). SRAM will hold the save (Knobs, achievements, statistics, settings;
no mid-run state) with a magic number and checksum, and is enabled only while it's being read or written.
Keeping two copies and writing them alternately would protect against power loss mid-write (**to decide**
when the save is built).

## Frame loop (`main.c`)

1. `vsync()`: waits for VBlank. GBDK's VBlank interrupt has already copied the shadow OAM to OAM by DMA.
2. `bg_flush()`: sets SCX/SCY and writes the queued background column or row. Must come straight after
   `vsync()`, while VBlank lasts.
3. Input, player movement and collision.
4. Camera: target = player centre minus half the screen, clamped to the room; moved at most 4 px per axis
   per frame. `bg_set_camera()` builds the next column or row into the queue.
5. Sprites: `spr_begin()`, draw calls into shadow OAM, `spr_end()` hides entries left over from last frame.

Everything for a frame is computed against the camera of step 4 and becomes visible together at the next
VBlank: scroll registers and background from step 2, sprites from the DMA in step 1.

## Scrolling (`bg.c`)

The hardware map is a 32×32-tile ring: room tile (tx, ty) is at map slot (tx & 31, ty & 31), so rooms can be
any size. The screen shows up to 21×19 tiles. The map is kept correct for those **plus one tile beyond each
edge** (23×21): `bg.c` tracks the region it holds (`ld_l..ld_r` × `ld_t..ld_b`).

- Each frame, `bg_set_camera()` drops what left the wanted region (no writes needed) and adds **at most one**
  column or row: first anything visible that's missing, then the margins. A new column covers the region's
  rows, a new row its columns, so the region stays a rectangle.
- The column or row is built in C from the room map and the metatile tables (16×16 metatile → four tiles, and
  on GBC four attribute bytes) into a buffer of at most 23 bytes (+23 attributes).
- `bg_flush()` writes it with inline assembly that checks STAT before each byte, so a write can never land
  while VRAM is locked, even if it runs past VBlank. Straight after `vsync()` it doesn't need to wait.

**Why one per frame with a margin**: the first version wrote both the new column and the new row on a
diagonal step (19 + 21 bytes) and ran past VBlank: up to 14 scanlines into a 10-line VBlank on DMG, 11 on GBC
(measured with `scripts/vblank-budget.py`). With one operation per frame it's at most 8 of 10 lines on DMG and
6 of 10 on GBC, including GBDK's VBlank interrupt and OAM DMA. The margin means a column or row is written
before it scrolls into view, so a camera of up to 4 px per axis per frame never shows a missing tile. That is
why `main.c` caps camera speed at 4 px per frame. A bigger jump (a teleport) must redraw with
`bg_draw_full()` while the LCD is off.

`scripts/scroll-test.py` checks every background pixel against a reference picture while the AUTOTEST build
walks (DEBUGGING.md).

## Sprites (`sprites.c`)

8×16 hardware sprites. A 16×16 frame is two of them (left and right column); facing right is the left-facing
frame mirrored (`SPR_FLIPX`: the columns swap and both get the X-flip bit). Each sheet's frame table (tile
and attribute per column, `0xFF` = empty column) is copied to WRAM on load, so drawing needs no bank switch.
GBC: attribute bits 0–2 are the palette, offset by the sheet's base palette. DMG: bit 4 picks OBP0 or OBP1.

Planned: 40 sprites, at most 10 per scanline. Rush and Ambush are large (32×32 or more, 8 sprites); Figure is
tall. When more than 10 would share a line, sprites will be cycled between frames (flicker) in a fixed order.

## Player (`player.c`)

- Position in pixels plus an 8-bit subpixel per axis. Speed 1.25 px/frame (75 px/s, about 4.7 metatiles a
  second); diagonals 0.883 px/frame per axis, so the speed is the same in all 8 directions.
- Collision box: the feet, x 3–12 and y 10–15 within the 16×16 sprite. The 3/4 view lets the head overlap a
  wall above. Each axis moves one pixel at a time and stops at the first solid metatile, so walking diagonally
  into a wall slides along it.
- Facing: 8 directions from the D-pad (`DIR_*`), kept when standing still. The sprite shows up, down or
  side; diagonals use the side frames.
- Animation: idle, step A, idle, step B, 8 frames each while walking.

## Asset pipeline (`scripts/assets.py`)

Runs from the Makefile; writes `build/gen/<kind>_<name>.c/.h`. The `.c` holds the GBC and DMG data behind
`#ifdef CGB`; the `.h` declares it and defines constants (`MT_HOTEL_FLOOR`, `SPR_PLAYER_LEFT_IDLE`, …).

- **Tileset** (`.tsx` + both PNGs): cuts 16×16 metatiles into 8×8 tiles, deduplicates them (separately per
  console), and writes the metatile tables. GBC palettes are found automatically: each 8×8 tile's colours
  (at most 4) are packed into at most 8 palettes, each sorted light to dark. Tile properties `solid` (bool)
  and `name` become `mt_flags` and the `MT_*` constants.
- **Room** (`.tmx`): the tile layer becomes a metatile map (empty cells are metatile 0, void); the object named
  `spawn` is the player's start point (pixels, top left of the sprite).
- **Sprite sheet** (`.json` + both PNGs): 16×16 frames in a grid, named in the JSON. GBC: each 8×16 column
  gets a palette of up to 3 colours (+ transparency). DMG: each column uses OBP0 (white, dark, black) or OBP1
  (light, dark, black), picked automatically.

Rules for the art are in ASSETS.md; the converter stops with an error naming the place that breaks one.
**Music** (planned): hUGETracker exports `.uge` songs to C for hUGEDriver.
