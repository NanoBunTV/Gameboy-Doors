# Architecture

As of Phase 2 (2026-10-17): the engine core (Phase 1) plus runs of generated rooms, doors, drawers and keys,
the HUD, fades and the title screen. Sections marked **planned** aren't built yet.

## Repository layout

| Path | Purpose |
|---|---|
| `Makefile`, `build.ps1` | Build both ROMs: `build/gbc/doors.gbc` and `build/dmg/doors.gb`, plus the AUTOTEST variants (BUILDING.md) |
| `src/` | Game code in C, shared by both targets (module table below) |
| `src/platform/` | Code that differs per target: `cgb/` (double speed, palettes), `dmg/` (BGP/OBP palettes) |
| `assets/tilesets/` | Tiled tilesets (`.tsx`) of 16×16 metatiles, each with a `.gbc.png` and a `.dmg.png` (ASSETS.md) |
| `assets/rooms/` | Tiled rooms (`.tmx`) |
| `assets/sprites/` | Sprite sheets: `<name>.gbc.png`, `<name>.dmg.png` and `<name>.json` (frame names) |
| `assets/fonts/`, `assets/title/` | The font; the title logo |
| `assets/music/`, `assets/sfx/` | **Planned**: hUGETracker songs, sound effects |
| `scripts/` | `assets.py` (asset converter), `check-rom.py`, `bgb-check.ps1`, `run-test.py`, `scroll-test.py`, `vblank-budget.py`, `bot.py`, `record-run.ps1` (DEBUGGING.md) |
| `tests/` | `autotest_input.c`: the recorded run AUTOTEST builds replay (written by `bot.py`) |
| `tools/` | Downloaded tools, not in git (`tools/README.md`) |
| `build/` | Output, not in git: `gen/` (generated C from assets), `gbc/`, `dmg/`, `gbc-autotest/`, `dmg-autotest/` (ROM, `.sym`, `.map`, `obj/`), `screens/`, `preview/` |

### Source modules

| File | What it does |
|---|---|
| `main.c` | Startup (LCDC setup), title, the frame loop |
| `title.c/.h` | The title screen (logo, blinking PRESS START) and the run's seed |
| `game.c/.h` | Playing a run: entering rooms (fades, placing the player at the door), door states and number plates, drawers, keys, pressing A, walking through doors, the camera |
| `run.c/.h` | The run: seed, per-room random generator, which template each room uses, locks and key places, what's been opened |
| `assets.h` | The data types the generated asset files use: `tileset_t`, `room_def_t`, `room_obj_t`, `sprite_sheet_t`, `font_t`, `image_t`; `TILESET_*`, `OBJ_*`, `WALL_*` |
| `room.c/.h` | The current room: map and objects in WRAM, the tileset's metatile tables in WRAM, collision queries, changing a metatile on screen, door-number plates |
| `bg.c/.h` | Camera and background streaming (below); the single-tile write queue; STAT-safe VRAM copy; VRAM writes in inline assembly |
| `hud.c/.h` | The font, the on-demand HUD on the window layer, messages, text printing |
| `sprites.c/.h` | Sprite sheets in VRAM, drawing 16×16 frames as two 8×16 hardware sprites, the per-frame shadow OAM allocator |
| `player.c/.h` | 8-direction walking, collision, corner nudging, facing (8 directions), walk animation |
| `input.c/.h` | Joypad held/pressed; in AUTOTEST builds the recorded run instead of the joypad |
| `platform.h`, `platform/*/` | Per-console setup, palettes (kept in WRAM, written in VBlank) and the fade to black |

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
| ROM bank 0 | Engine code (no `#pragma bank`), GBDK library, the room template table (`room_templates.c`). **74% full on GBC** as of Phase 2 (`romusage`, DEBUGGING.md): engine code will have to move into banks soon |
| ROM banks 1+ | Generated asset data and the AUTOTEST recording (`#pragma bank 255`, placed by GBDK's autobanking). Read with `SWITCH_ROM(BANK(x))` and restore the bank after |
| VRAM 0x8000–0x87FF | Sprite tiles 0–127: player 0–35, key 36–39 |
| VRAM 0x8800–0x97FF | Background tiles 0–255 (LCDC bit 4 clear: tiles 0–127 at 0x9000, 128–255 at 0x8800): **tileset 0–183** (the title logo borrows them), **door-number plates 184–191** (2 tiles each, made at runtime), **font 192–255**. Sprite tiles 128–255 would overlap this, so sprites stay below 128 |
| VRAM 0x9800 | Background map (32×32), streamed as below. GBC: attributes in VRAM bank 1 at the same address |
| VRAM 0x9C00 | Window map: the HUD (row 0 border, row 1 text). GBC: attributes all palette 7 |
| GBC BG palettes | 0–6 the tileset (or the title logo), 7 the font/HUD |
| GBC sprite palettes | 0 player, 1 key |
| WRAM | Room map (`room_map`, up to 3,072 metatiles) and objects, metatile tables (`mt_tiles`, `mt_flags`, `mt_attrs`: 128 metatiles, 120–127 made at runtime), run state (~500 bytes for 101 rooms), the tile write queue, frame tables, GBDK's shadow OAM at 0xC000, the stack from 0xE000 down. DMG uses ~4.9 of 8 KB |
| SRAM | **Planned**: the save |

LCDC: `LCDCF_BG8800 | LCDCF_BG9800 | LCDCF_WIN9C00 | LCDCF_WINON | LCDCF_OBJ16 | LCDCF_OBJON | LCDCF_BGON`
(+ `LCDCF_ON`), set in `main()`. The window is hidden by moving it below the screen (WY = 144).

## Cartridge

MBC5 with RAM and battery (cartridge type `0x1B`). ROM size grows as needed (GBDK's autobanking assigns
banks; the ROM is 32 KB as of Phase 1). SRAM will hold the save (Knobs, achievements, statistics, settings;
no mid-run state) with a magic number and checksum, and is enabled only while it's being read or written.
Keeping two copies and writing them alternately would protect against power loss mid-write (**to decide**
when the save is built).

## Frame loop (`main.c`, `game_frame()` in `game.c`)

1. `vsync()`: waits for VBlank. GBDK's VBlank interrupt has already copied the shadow OAM to OAM by DMA.
2. `bg_flush()`: sets SCX/SCY/WY, writes changed palettes (`platform_vblank()`), and writes the queued
   background column, row or batch of single tiles. Must come straight after `vsync()`, while VBlank lasts.
3. Input, player movement and collision. Walking into an open door's doorway starts a room transition
   (below) and ends the frame.
4. A pressed: use whatever is in front of the player.
5. HUD: timers, slide, queue changed text.
6. Camera: target = player centre minus half the screen, clamped to the room; moved at most 4 px per axis
   per frame. `bg_set_camera()` builds the next column or row, or a batch of queued single tiles.
7. Sprites: `spr_begin()`, draw calls into shadow OAM, `spr_end()` hides entries left over from last frame.

Loops outside play (the title, fades) use `game_wait_frames()`, which does steps 1–2 and lets queued
single-tile writes through (`bg_idle()`).

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
why `game.c` caps camera speed at 4 px per frame. A bigger jump (a room change) redraws with
`bg_draw_full()`, which is STAT-safe with the LCD on but takes a frame or two, so it's done while the screen
is faded to black.

**Single-tile writes** (`bg_queue_tile`): a ring of 64 (address, tile, GBC attribute) entries for things that
change in place: a metatile changing on screen (`room_set_metatile` → `bg_refresh_metatile`, which only
queues the tiles inside the loaded region; the rest is drawn from `room_map` when it streams in) and HUD
text. A frame writes **either** a column/row **or** up to 20 queued tiles, never both, to stay inside the DMG's
VBlank: queued tiles go first unless a visible column/row is missing. `bg_draw_full` drops queued writes to
the background map (they'd paint the old room) but keeps the window's.

`scripts/scroll-test.py` checks every background pixel against a reference picture of the room built from the
game's own map, while the AUTOTEST build plays its recorded run (DEBUGGING.md).

## Rooms, doors and the run (`run.c`, `game.c`)

DOORS' numbering (Minecraft docs 02): **room n is entered through door n**; room 0 is the reception and its
exit is door 1. Rooms are separate maps (owner, 2026-10-17); walking through a door fades to black, loads the
next (or previous) room and fades in.

- **Planning** (`run_plan`): room n's template, lock and key place come from a xorshift16 generator seeded with
  the run seed and n, so a run is reproducible from its seed and a room's plan doesn't depend on the order
  things happened. Rooms are planned in order because each template must be entered through the wall
  opposite the previous room's exit (top → bottom, left → right, right → left). A template used in the last 3
  rooms isn't picked (relaxed if nothing else fits). Room 100's exit stays shut ("TO BE CONTINUED...").
- **Locks**: a room with `allow_lock` and somewhere to put the key locks its exit 18% of the time. The key is
  in one of its drawers 75% of the time when it has both drawers and key spots (owner: mostly drawers). The
  reception always locks, key on its fixed key spot. Opening the door uses the key up.
- **State** per room (`run_flags`, `run_drawers`, `run_key_slot`, `run_template`; 101 rooms): exit opened,
  key taken, drawers searched. Loading a room puts that state into the fresh map (`apply_room_state`), so
  going back finds it as left. The player carries at most one key (`run_key_door`).
- **Doors** are swapped between metatile sets by state (open / closed / locked, per wall). A closed door's top
  metatile is a **runtime metatile** (120+): `room_make_plate` copies the door's plate tiles from ROM, draws
  the number with a 3×5 digit font, and writes them to BG tiles 184+.
- **Arriving**: through the entry door, the player stands just inside it facing into the room; coming back
  through the exit, just inside that. The doorway cells are the trigger, so arriving never re-triggers.
- **Using things** (A): the point 12 px ahead of the feet in the facing direction picks the metatile; a door,
  drawer or visible key there is used. Messages show in the HUD.
- **Corner nudge** (`player.c`): walking straight into a wall with a gap within 6 px to one side slides the
  player 1 px toward the gap, so 16 px doors don't need pixel-exact lining up.

## Fades and palettes (`platform/*`)

`platform_set_fade(0..3)` darkens everything one step (3 = black). Palettes are kept in WRAM and written in
the next VBlank by `platform_vblank()` (called from `bg_flush`): on GBC because palette RAM can't be written
while the LCD draws; on DMG it's just the BGP/OBP registers with every shade shifted toward black. A room
change: fade out (3 steps × 3 frames), hide sprites and HUD, load and draw the room with the screen black,
fade in. The LCD stays on throughout: turning it off would flash white on a DMG.

## HUD (`hud.c`)

The window layer at 0x9C00, 2 rows: a border line and 20 characters of text, sliding up 2 px a frame from
WY 144 to 128 while shown. It shows the status line ("DOOR 012" and the key icon) on entering a room and
while Select is held, or a centred message ("IT'S LOCKED.", "GOT KEY FOR DOOR 012"). Changed characters
go through the single-tile queue. On GBC the window's attributes are all palette 7 (the font's).

## Title (`title.c`)

The logo (an `image_t`: tiles, map, GBC attributes) is loaded over the tileset's BG tiles with the LCD off,
so `room_forget_tileset()` makes the first room reload the tileset (STAT-safe, behind the fade). The run's
seed is the number of frames the title was shown XOR the divider register when Start is pressed: a Game Boy
powers on identically every time, so a person's timing is the only randomness. AUTOTEST builds use the
fixed seed `0xC0DE`.

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
  (at most 4) are packed into at most 7 palettes, each sorted light to dark. Tile properties `solid` (bool)
  and `name` become `mt_flags` and the `MT_*` constants.
- **Room** (`.tmx`): the tile layer becomes a metatile map (empty cells are metatile 0, void); the objects
  (entry, exit, drawer, keyspot, spawn) become a `room_obj_t` list in metatile units, sorted by type; map
  properties `kind` and `allow_lock`. Validated: door sizes and positions, one entry/exit, drawers on
  dressers, somewhere for a key when locking is allowed.
- **Room list** (`roomlist`, all `.tmx` at once): `room_templates.c` in bank 0, with each template's kind,
  entry and exit walls, `allow_lock`, drawer and key-spot counts (what `run.c` plans with, without switching
  banks) and `room_template(id, &bank)`. Checks there's exactly one start room and that every exit wall has a
  room that can follow it.
- **Sprite sheet** (`.json` + both PNGs): 16×16 frames in a grid, named in the JSON. GBC: each 8×16 column
  gets a palette of up to 3 colours (+ transparency). DMG: each column uses OBP0 (white, dark, black) or OBP1
  (light, dark, black), picked automatically.
- **Font** (`.json` + both PNGs): 8×8 characters, one palette, and an ASCII → tile table.
- **Image** (the title logo, both PNGs): deduplicated tiles, a tile map, GBC palette per tile.

Rules for the art are in ASSETS.md; the converter stops with an error naming the place that breaks one.
**Music** (planned): hUGETracker exports `.uge` songs to C for hUGEDriver.
