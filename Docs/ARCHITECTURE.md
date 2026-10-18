# Architecture

As of Phase 4 (2026-10-18): the engine core (Phase 1), runs of generated rooms with doors, drawers and keys
(Phase 2), hiding, Rush, Ambush, Hide, health, death and the Guiding Light, music and sound effects
(Phase 3), and dark rooms, Screech and Eyes (Phase 4). Sections marked **planned** aren't built yet.

## Repository layout

| Path | Purpose |
|---|---|
| `Makefile`, `build.ps1` | Build both ROMs: `build/gbc/doors.gbc` and `build/dmg/doors.gb`, plus the AUTOTEST and DEBUG variants (BUILDING.md) |
| `src/` | Game code in C, shared by both targets (module table below) |
| `src/platform/` | Code that differs per target: `cgb/` (double speed, palettes), `dmg/` (BGP/OBP palettes) |
| `assets/tilesets/` | Tiled tilesets (`.tsx`) of 16×16 metatiles, each with a `.gbc.png` and a `.dmg.png` (ASSETS.md) |
| `assets/rooms/` | Tiled rooms (`.tmx`) |
| `assets/sprites/` | Sprite sheets: `<name>.gbc.png`, `<name>.dmg.png` and `<name>.json` (frame names) |
| `assets/fonts/`, `assets/title/`, `assets/scares/` | The font; the title logo; the jumpscare screens |
| `assets/music/` | hUGETracker songs (`.uge`); `midi/` holds the owner's MIDI files they were arranged from. Sound effects are register scripts in `src/sfx_data.c` |
| `scripts/` | `assets.py` (asset converter), `check-rom.py`, `bgb-check.ps1`, `run-test.py`, `scroll-test.py`, `vblank-budget.py`, `bot.py`, `record-run.ps1`, `plan.py` (a copy of the run planner, for choosing the test seed) (DEBUGGING.md) |
| `tests/` | `autotest_input.c`: the recorded run AUTOTEST builds replay (written by `bot.py`) |
| `tools/` | Downloaded tools, not in git (`tools/README.md`) |
| `build/` | Output, not in git: `gen/` (generated C from assets), `gbc/`, `dmg/`, `gbc-autotest/`, `dmg-autotest/`, `gbc-debug/`, `dmg-debug/` (ROM, `.sym`, `.map`, `obj/`), `screens/`, `preview/` |

### Source modules

| File | What it does |
|---|---|
| `main.c` | Startup (LCDC setup, sound), title → run → death screen → run or title |
| `title.c/.h` | *Banked.* The title screen (logo, blinking PRESS START, the Lobby Theme) and the run's seed |
| `game.c/.h` | *Banked.* Playing a run: entering rooms (fades, placing the player at the door), door states and numbers (the real exit and a Dupe room's wrong doors), drawers, keys, pressing A, walking through doors, the camera, the light level, the player's light, Screech's bite and Dupe's |
| `run.c/.h` | *Banked.* The run: seed, per-room random generator, which template each room uses (and which of its exits is real), locks and key places, spawn, dark-room, Eyes, Dupe and Halt rolls, Dupe's wrong numbers, Timothy's drawer, what's been opened, health, Hide's bans |
| `entity.c/.h` | *Banked.* Rush and Ambush: the spawn queue, the route, the flicker, the rumble, the kill test |
| `hide.c/.h` | *Banked.* Closets and beds, and Hide's timer, heartbeat and eviction |
| `screech.c/.h` | *Banked.* Screech: the roll in dark rooms, where he waits, the look, the bite |
| `eyes.c/.h` | *Banked.* Eyes: where he hangs, his glow, the looking test and the drain |
| `halt.c/.h` | *Banked.* Halt's hallway: its own loop from the armed door to the far end (legs, TURN AROUND, contact) |
| `timothy.c/.h` | *Banked.* Timothy: the spider's jump out of a drawer and his scuttle away |
| `library.c/.h` | *Banked.* The Library (room 50): the books and paper planned from the seed, reading them, the notes and padlock panels, the entry hint, the music |
| `figure.c/.h` | *Banked.* Figure: hearing, the route search (a flood fill in the room map's spare tail), wandering and hunting, footsteps, contact, the edge arrow |
| `light.c/.h`, `light_move.c`, `light_shapes.c` | Darkness (below). `light.c` (bank 0): what streaming asks. `light_move.c` (banked): moving a light. `light_shapes.c` (banked): the shapes, made at boot |
| `death.c/.h` | *Banked.* The jumpscares (also Screech's bite when you live), the Guiding Light (typed hint, Try again / Title) |
| `audio.c/.h`, `sfx_data.c/.h` | hUGEDriver music (one song at a time) and the sound-effect player (bank 0); the effects' register scripts (`sfx_data.c`, banked: `audio.c` switches `BANK(sfx_data)` in to read them) |
| `image.c/.h` | Full-screen pictures (logo, jumpscares) and clearing the screen, STAT-safe |
| `assets.h` | The data types the generated asset files use: `tileset_t`, `room_def_t`, `room_obj_t`, `sprite_sheet_t`, `font_t`, `image_t`; `TILESET_*`, `OBJ_*`, `WALL_*` |
| `room.c/.h` | The current room: map and objects in WRAM, the tileset's metatile tables in WRAM, collision queries, changing a metatile on screen, door-number plates |
| `bg.c/.h` | Camera and background streaming (below); the single-tile write queue; STAT-safe VRAM copy; VRAM writes in inline assembly |
| `hud.c/.h`, `hud_status.c` | The font, the on-demand HUD on the window layer, messages, text printing. `hud.c` (bank 0): the font, and the functions taking text (a string literal is in its caller's bank). `hud_status.c` (banked): timers, the slide, the status line. `hud_state.h`: what the two share |
| `sprites.c/.h` | Sprite sheets in VRAM, drawing 16×16 frames as two 8×16 hardware sprites (bigger frames as several), the per-frame shadow OAM allocator |
| `player.c/.h` | *Banked.* 8-direction walking, collision, corner nudging, facing (8 directions), walk animation |
| `input.c/.h` | Joypad held/pressed; in AUTOTEST builds the recorded run instead of the joypad |
| `platform.h`, `platform/*/` | Per-console setup, palettes (kept in WRAM, written in VBlank), the fade to black, dimming, Ambush's green tint, Halt's blue tint and his white flash |

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
| Effects | Flicker and hiding dim the room palettes (not the sprites or the HUD); Ambush tints them green. Darkness: unlit tiles are the font's black space (palette 7) | Flicker and hiding shift BGP darker (the HUD dims too: one BG palette); no tint. Darkness: the same tiles as GBC |

`#ifdef CGB` appears in `src/platform/`, the generated asset files, and where GBC attributes or palettes are
written (`room.c`, `bg.c`, `sprites.c`). Game logic is the same. The DMG build is the performance floor:
anything in the main loop must fit a DMG frame.

## Memory map

| Area | Use |
|---|---|
| ROM bank 0 | Engine code that must stay put (no `#pragma bank`): everything that switches banks to read assets (`room.c`, `image.c`, `hud.c`, `sprites.c`, `audio.c`), streaming and what it asks of the light (`bg.c`, `light.c`), input, platform; GBDK library; hUGEDriver (1.9 KB); the room template table (`room_templates.c`: with Phase 5's per-exit routes ~0.5 KB). **90% full on GBC, 77% on DMG** after Phase 6 (88% / 76% after Phase 5; 84% / 73% just before it, after `hud_status.c` and `sfx_data.c` moved out) |
| ROM banks 1+ | Banked code (`game`, `run`, `title`, `entity`, `hide`, `death`, `player`, `screech`, `eyes`, `halt`, `timothy`, `library`, `figure`, `light_move`, `light_shapes`, `hud_status`, `sfx_data`: `#pragma bank 255` and `BANKED` functions), generated asset data, songs, the AUTOTEST recording, all placed by GBDK's autobanking. The ROM is 128 KB (64 KB until Phase 6). Banked code must never `SWITCH_ROM` (it would switch itself out), and may pass pointers to its own constants (strings) only to bank-0 functions, which read them while the caller's bank is still in |
| VRAM 0x8000–0x8FFF | Sprite tiles: player 0–35, key 36–39, Rush 40–71, Ambush 72–103 (`entity_load`), Eyes 104–133, Screech 134–141, Timothy 142–143. In Halt's hallway Halt (18 tiles) and the TURN AROUND letters (14) borrow Rush's and Ambush's tiles from 40 (no other entity runs there; `game_init` puts them back on the way out). In the Library Figure (28 tiles) and the marks (glint, arrows: 8) do the same, from 40 (`library_load_sprites`; `game_init` on the way out to room 51). Tiles 128 and up are the same memory as background tiles 128 and up, which a jumpscare picture of more than 128 tiles overwrites, so `game_start` (and the room after Screech's bite) reloads every sheet (`game_init`, STAT-safe). Sprites must stay below 184 (the door numbers and the font) |
| VRAM 0x8800–0x97FF | Background tiles 0–255 (LCDC bit 4 clear: tiles 0–127 at 0x9000, 128–255 at 0x8800): **tileset 0–183** (the title logo and the jumpscares borrow them), **door numbers 184–191** (2 tiles each, made at runtime: slots 0–2 a room's exits, 3 the hallway's shut door), **font 192–255** (its space, all black, is darkness's tile) |
| VRAM 0x9800 | Background map (32×32), streamed as below. GBC: attributes in VRAM bank 1 at the same address |
| VRAM 0x9C00 | Window map: the HUD (row 0 border, row 1 text), rows 2–8 the Library's panel (notes, padlock). GBC: attributes all palette 7 |
| GBC BG palettes | 0–6 the tileset (or the title logo / a jumpscare), 7 the font/HUD (blue on the Guiding Light) |
| GBC sprite palettes | 0 player, 1 key, 2 Rush, 3 Ambush, 4 Eyes, 5 Screech, 6 Timothy (in Halt's hallway: 2 Halt, 3 the letters; in the Library: 2 Figure, 3 the marks) |
| WRAM | Room map (`room_map`, up to 2,816 metatiles (3,072 until Phase 6: the 256 bytes went to the Library on GBC); `room_row`, each row's offset; in the Library its unused tail holds Figure's route search) and objects, metatile tables (`mt_tiles`, `mt_flags`, `mt_attrs`: 128 metatiles, 120–127 made at runtime), run state (~700 bytes for 101 rooms), the light shapes (360 bytes), the GBC's cached dim levels (448 bytes; the blue tint isn't cached), `run_exit` (101 bytes), hUGEDriver's variables, the tile write queue (128 entries), frame tables, GBDK's shadow OAM at 0xC000, the stack from 0xE000 down. DMG uses ~6.4 of 8 KB (1.6 KB left under the stack); on GBC, WRAM bank 1 has about 237 bytes left under the stack after Phase 6 (STATUS.md) |
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
   On a GBC frame that uploaded palettes, the map writes wait for the next frame (below).
3. Input. Hidden: A or B leaves, Hide's timer runs. Otherwise: player movement and collision; walking into
   an open door's doorway starts a room transition (below) and ends the frame, unless Rush or Ambush is
   out; A uses whatever is in front of the player.
4. Rush / Ambush move (`entity_update`), Eyes looks (`eyes_update`), Screech waits (`screech_update`). Hide,
   Rush / Ambush, Eyes or Screech can end the run here (`GAME_DEAD`); Screech's bite when you live shows his
   face and redraws the room (`survived_scare`, also used for Dupe). Timothy's jump (`timothy_update`).
5. The light level: the larger of hiding's darkness and the flicker (`platform_set_dim`).
6. HUD: timers, slide, queue changed text (the status line is rebuilt only when what it shows changes).
7. Camera: target = player centre minus half the screen, clamped to the room; moved at most 4 px per axis
   per frame. `bg_set_camera()` builds the next column or row, or a batch of queued single tiles.
8. The player's light (`light_set`) and a few rows of the redraw it needs (`light_step`), **only on frames
   that didn't build a column or row**: the player crosses a tile edge on the same frame as the centred
   camera, and both together overran DMG frames.
9. Sprites: `spr_begin()`, draw calls into shadow OAM, `spr_end()` hides entries left over from last frame.
10. `audio_frame()`: a music tick and the sound effects' next steps. After the VBlank work on purpose:
   hUGEDriver takes a few scanlines and the DMG has two to spare.

Loops outside play (the title, fades, the death screen) use `game_wait_frames()`, which does steps 1–2 and 10
and lets queued single-tile writes through (`bg_idle()`). `main()` runs title → run → `death_run()` → a new
run (Try again) or the title.

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

**Palette frames** (GBC): a palette upload (64 bytes of BG palette RAM, which can only be written in VBlank)
plus a column in the same VBlank ran 12 lines into a 10-line VBlank once the flicker started changing
palettes every few frames. So `platform_vblank()` reports when it uploaded, `bg_flush()` then leaves the map
writes for the next frame, and `bg_set_camera()` builds nothing new while last frame's writes are still
waiting. The one-tile margin covers the delay. Now 6 of 10 lines on GBC.

**Single-tile writes** (`bg_queue_tile`, `bg_refresh_span`): a ring of 128 (address, tile, GBC attribute)
entries for things that change in place: a metatile changing on screen (`room_set_metatile` →
`bg_refresh_metatile`, which only queues the tiles inside the loaded region; the rest is drawn from
`room_map` when it streams in), tiles a moving light lights or darkens, and HUD text. A frame writes **either** a column/row **or** up to 20 queued tiles, never both, to stay inside the DMG's
VBlank: queued tiles go first unless a visible column/row is missing. `bg_draw_full` drops queued writes to
the background map (they'd paint the old room) but keeps the window's.

`scripts/scroll-test.py` checks every background pixel against a reference picture of the room built from the
game's own map (and, in dark rooms, its light state), while the AUTOTEST build plays its recorded run
(DEBUGGING.md).

## Darkness (`light.c`, `light_move.c`, `light_shapes.c`)

A dark room (owner, 2026-10-18) is black except the tiles a light reaches: an unlit tile is drawn as the
font's space (all black; GBC palette 7, which dimming never touches). It's the same on both consoles, so
darkness costs map writes, not palettes.

- **Sources**: two, each a shape at a tile. The player's: the **glow** (a rounded 5×5 tiles) until items
  exist; in DEBUG and AUTOTEST builds B cycles to the **lighter** (radius 3) and the **flashlight** (a cone
  7.5 tiles long, 1 wide at the player and 5 at the end, in the facing direction, plus the glow). Eyes':
  the lighter's shape at his middle.
- **Shapes** (`light_shapes.c`, at boot): for each of 10 shapes (glow, lighter, 8 flashlight directions),
  rows -8..8 around the source, each one span of columns, plus the first and last row with anything.
- **Streaming**: every column or row `bg.c` builds in a dark room is masked (`light_mask_column`,
  `light_mask_row`): per source, the rows or span that fall in it, then a byte loop blacks out the rest.
  Nothing outside the room is ever lit.
- **Moving a light** (`light_set`): records the shape and place before the move; `light_step` then redraws
  what changed, 4 rows per frame, row by row as intervals: what the new shape adds is lit, what the old one
  loses goes dark unless the other source lights it. Nothing per tile asks what's lit.
- **Cost** (measured on DMG with PyBoy hooks, DEBUGGING.md): SDCC's 16-bit code is slow on the SM83, so the
  per-row and per-tile work is 8-bit, in room tile coordinates (rooms are at most 127 metatiles a side). The
  first version took ~140 scanlines to move the glow one tile; now a dark room drops no more frames than a lit
  one, except with the debug flashlight and Eyes together (STATUS.md).
- **Rooms**: `run_plan` rolls darkness for rooms 15 and up (10%, after every earlier roll). Rush and Ambush
  never fire into a dark room (the spawn waits, as for a room with no hiding spot). The lights-out sound plays
  when a dark room follows a lit one, not again for a run of dark rooms.

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
- **Doors** are swapped between metatile sets by state (open / closed / locked, per wall). A door's top
  metatile (front doors open or shut, side doors shut) is a **runtime metatile** (120): `room_make_plate`
  copies its top two tiles from ROM, paints the number over the art's "000" in brass on the dark header
  (3×5 digits at x 2/6/10, y 2–6), and writes them to BG tiles 184–185 (the next exits 186–187 and 188–189:
a slot per exit). Only those two tiles are copied, so
  the digits must end above pixel row 8 (a compile-time check; until 2026-10-18 their bottom row ran past
  the buffer, which was the "cut-off" digit).
- **Dupe rooms** (Phase 5): a template with 2–3 exits. `run_plan` picks the real one per run (`run_exit`;
  the next room must be entered from the opposite wall of *that* door), then rolls Dupe (35% from room 6).
  The others are **wrong doors**: shut, padlocked when the real one is, and numbered by `run_fake_number`
  (one below or up to two above, no repeats, from the seed) when Dupe is there, numberless (`PLATE_BLANK`)
  when not. A at one (`use_fake`), in order: "IT WON'T BUDGE." while Rush is out; "IT'S FAKE." without Dupe;
  "IT WON'T OPEN." once he's fired from it (`RF2_DUPE_HIT`, a bit per wrong door; the plate then shows
  scribbles, `PLATE_GARBLED`); else it opens, 20 frames, his face (`death_jumpscare(DEATH_DUPE)`), 40 HP,
  and the room comes back. No key is looked for or spent, and Eyes stays (not progress).
- **Halt** (Phase 5): `run_plan` arms at most one plain exit a run (not locked, single exit, from room 10,
  3%: `RF2_HALT`). Walking through it runs `halt_run()` before the next room: its own loop (vsync, flush,
  input, player, Halt, HUD, camera, sprites, audio; `GAME_HALT`), in the `halt` template with the entry shut
  (a blank plate) and the exit open. Each leg Halt is placed on the side the player walks away from, at
  `d = speed × grace + (leg − grace) × (speed − walk)` (Minecraft docs 06's sum; here 1.4 px/frame, 40
  frames, legs of 150 back and 203 forward, so d is 73–82 px: at the screen's edge). He moves at the player
  across at his speed, up and down at half a pixel. At a leg's end: `halt_dir` flips, he's placed on the
  other side, TURN AROUND (sprite letters) shows for 60 frames, the background flashes white for 4. Contact
  (10 px across, 14 up and down): 60 HP and he's put back at the leg's start, or death (`DEATH_HALT`, no
  jumpscare). The far door ends it: `RF2_HALT_DONE`, the sprite sheets reloaded, the next room as usual.
  Nothing else runs meanwhile (Rush, Screech, Eyes, Hide).
- **Timothy** (Phase 5): opening a drawer asks `run_timothy_in` (once a run, never the key's drawer, a hash
  of the seed, room and drawer: 1 in 200). He hops from the drawer to the player in 20 frames, takes 10 HP
  (never the last: at 10 or less you're left on 1), then runs off along the floor toward the room's wider
  side for 50 frames. The drawer counts as searched. DEBUG and AUTOTEST builds: poke `run_force_timothy` to 1
  and the next drawer holds him.
- **The Library** (Phase 6, `library.c`, `figure.c`): `run_plan` gives room 50 the `library` template and
  its exit `RF_LOCKED` (the padlock: no key), skips every roll there (spawns, dark, Eyes, Dupe, Halt) and
  Halt on room 49, whose template must have one exit, on the top wall. `entity_room_entered` doesn't fire a
  queued Rush or Ambush in the Library (the spawn waits, with no patience lost). In the room, `door_set` draws
  the locked exit with `door_bottom_padlock` and a blank plate; `apply_room_state` turns the entry doorway
  into wall (door 50 seals). `library_enter()` (from `enter_room`, before the room is shown) plans the books
  the first time from the seed: the bookcase metatiles (`MTF_BOOKCASE`) with floor below, in map order, eight
  of them picked; a shuffle of the eight shapes; a digit each; a second shuffle's first five for the paper.
  Then it loads Figure's sheets over Rush's tiles, starts him (deaf for 3 s the first time) and the music.
  Each frame `library_frame()` runs first (Start toggles the notes; the open padlock takes every button, the
  player standing still), then `library_update()` (the entry hint's two lines; Figure). A at a book cell or
  the `paper` object reads it into the HUD. The panels are window rows 2–8 under the status line
  (`hud_panel_set`, `text_print_win`); while one is open, sprites with their middle below its top
  (`spr_clip_y`) aren't drawn. Leaving for room 51: `library_exit()` (Figure stopped, `library_left`, the
  music off) and `game_init()` for the sprite VRAM.
- **Figure** (`figure.c`): his feet move along the middles of metatiles, downhill on a distance map: a flood
  fill from his goal over the floor (four neighbours, one byte a cell, 255 unreached) in the room map's tail
  (`room_map + w·h`), its queue (256 cells as x, y) after it. The converter allows a library of at most
  (ROOM_MAX − 512) / 2 metatiles and requires a solid edge (the fill doesn't check bounds). The fill runs 32
  cells a frame while he stands (about half a second for the Library; 40 a frame lagged the DMG). A walking,
  unhidden, not-sneaking player makes a noise at their feet at once and every 8 frames: wandering or pausing,
  he takes it (hunting, 1.44 px/frame); hunting, only a noise in his goal's own cell moves the goal (he
  finishes what he started, *MC 10*); his roar plays at most every 3 s either way. Arriving ends a hunt (40
  frames' pause); wandering, he picks a random floor cell more than 4 away (0.75 px/frame). Footsteps on CH4
  every 28 frames wandering, 14 hunting, volume 15 − distance/32 px (`sfx_play_vol`). Contact (feet within 10
  across and 8 up and down, player not hidden): `DEATH_FIGURE`, his face, the Guiding Light. Off screen, an
  arrow sprite on the edge nearest him.
- **Arriving**: through the entry door, the player stands just inside it facing into the room; coming back
  through the exit, just inside that. The doorway cells are the trigger, so arriving never re-triggers.
- **Using things** (A): the point 12 px ahead of the feet in the facing direction picks the metatile; a door,
  drawer, closet, bed or visible key there is used. If there's nothing, the points 6 px to either side are
  tried: furniture nudges the player off the line of what they face, and a closet missed by 3 px is a
  death. Messages show in the HUD.
- **Corner nudge** (`player.c`): walking straight into a wall with a gap within 6 px to one side slides the
  player 1 px toward the gap, so 16 px doors don't need pixel-exact lining up.

## Rush, Ambush, hiding and Hide (`entity.c`, `hide.c`)

Numbers come from the Minecraft config (`entities`, `rush`, `ambush`, `hide`), one block = 16 px, 20 ticks =
60 frames. DESIGN.md has the player-facing rules.

- **Spawning**: `run_plan` rolls each room from 3 on (18%, a tenth of them Ambush) after its lock, so the roll
  changed no layout. Entering a room for the first time with that flag **queues** the entity; it **fires** in
  the first newly entered room with a hiding spot Hide hasn't banned, or gives up after 8 rooms
  (Minecraft docs 04: decide at the moment of the spawn, not at planning time).
- **Route**: each template's route is the straight line from its entry door's centre to its exit door's
  centre (the reception: spawn to exit), written by `assets.py` with its length; a Dupe room has one per
  exit and the run's real one is used (`run_exit`). The entity's position is a
  room number and a distance along that room's route, so he exists in rooms the player isn't in; he's drawn
  only in this one, placed by interpolating between the two door centres: exactly (two 32-bit divisions)
  when he's first placed in the player's room, then by adding his step times the route's slope each frame.
  Dividing every frame cost ~100 of the DMG's 154 scanlines.
- **A pass**: fired → 45 frames → the flicker (Rush 2 s with an electric buzz; Ambush 4 s, green on GBC, with
  his howl) → he starts 1,000 px of route behind this room's entry door and travels to the far door (the
  furthest room reached's exit): Rush at 5.33 px/frame, Ambush 8.53. Rush vanishes there. Ambush turns, goes
  back to where he started, waits 5 s and comes again: 3–5 legs in all (from the seed), so he leaves at the
  far door or the rear. While either is out, no door opens or lets you through.
- **Kill test**: same room, player not hidden, within 96 px (Rush) / 128 px (Ambush) of him, through walls.
- **Dark rooms**: never fired into (Darkness, above).
- **Rumble**: continuous CH4 noise whose volume follows his distance along the route, from 768 px.
- **Hiding**: A at a closet or bed hides (the player sprite goes, the room dims to level 2); A or B leaves.
  Hide: from 7 s a heartbeat that speeds up (with a darker pulse on each beat, the warning without sound),
  at 12 s out with 40 HP gone, that spot banned for the run (`run_banned`, a bit per spot). Damage at 40 HP or
  less kills (Hide's Guiding Light).

## Screech and Eyes (`screech.c`, `eyes.c`)

The owner's numbers (2026-10-18) on top of the Minecraft config (`screech`, `eyes`); DESIGN.md has the rules.

- **Screech**: in a dark room, not hidden, off cooldown, every 15 frames a 5/256 roll (the Minecraft
  version's per-tick chance for "45% of dark rooms, for someone spending 8 s in one"). He waits 32 px from the
  player in a direction 90–270° from where they face, picked so he's on screen (the camera stops at room
  edges), and follows the player. Facing his direction for 18 frames in a row sends him away (a squeak, a
  40-frame shudder); 180 frames without that, he bites: 40 HP, his face full screen (`death_jumpscare`), then
  the room reloaded and redrawn as it was (`screech_bite`), or the Guiding Light if those were the last 40.
  Hiding sends him away silently. A 20 s cooldown follows any of these. His random numbers come from a
  generator seeded from the run, so a recorded run replays.
- **Eyes**: `run_plan` rolls rooms 8 and up (5%), independently of everything else. He hangs at the middle of
  the room's route (between the two door centres) and lights a lighter-sized area. **Looking** = facing
  exactly the one of 8 directions he's in (`octant()`, 22.5° either side; the owner's narrow cone), or
  standing under him. After 30 frames of looking, 5 HP every 15 frames, with the hit sound on every 4th hit.
  Hidden, no damage. Opening the room's exit door dismisses him for good (`RF2_EYES_GONE`).

## Fades, dimming and palettes (`platform/*`)

`platform_set_fade(0..3)` darkens everything one step (3 = black). `platform_set_dim(0..3, tint)` darkens
only the room: on GBC BG palettes 0–6 (not 7, the HUD; not sprites, so Rush stays visible), on DMG BGP (the
HUD too). Palettes are kept in WRAM, computed when set, and only copied in the next VBlank by
`platform_vblank()` (called from `bg_flush`): on GBC because palette RAM can't be written while the LCD
draws; on DMG it's just the BGP/OBP registers with every shade shifted toward black.

**GBC dim cache**: the flicker changes level every 4 frames. Recomputing 28 colours each time (a division per
channel) cost the GBC 92 of 332 frames during Ambush's flicker. Each level × tint is now computed the first
time it's needed after the palettes or the fade change, and kept (8 × 56 bytes); a flicker step is a copy.
Halt's blue tint isn't cached (the hallway sets it once; WRAM bank 1 had no room for 4 more levels), and his
flash (`platform_set_flash`) writes white over palettes 0–6 (DMG: BGP 0) until turned off. A room
change: fade out (3 steps × 3 frames), hide sprites and HUD, load and draw the room with the screen black,
fade in. The LCD stays on throughout: turning it off would flash white on a DMG.

## HUD (`hud.c`)

The window layer at 0x9C00, 2 rows (and, in the Library, a 7-row panel under them: above): a border line and 20 characters of text, sliding up 2 px a frame from
WY 144 to 128 while shown. It shows the status line (`DOOR 012`, a heart and an 8-tile health bar in 2 px
steps from font glyphs, the key icon) on entering a room, when health changes and while Select is held, or a
centred message ("IT'S LOCKED.", "GOT KEY FOR DOOR 012", "GET OUT!"). Changed characters
go through the single-tile queue. The status line is rebuilt only when the room, health or key changes:
building it every frame (three divisions and a 20-character compare) took ~45 of the DMG's 154 scanlines. On GBC the window's attributes are all palette 7 (the font's).

## Title and death screen (`title.c`, `death.c`, `image.c`)

The logo (an `image_t`: tiles, map, GBC attributes) is loaded over the tileset's BG tiles by `image_show()`,
which then calls `room_forget_tileset()` so the next room reloads the tileset (STAT-safe, behind the fade).
At boot the LCD is still off; coming back from a run the title is drawn behind a fade to black instead,
because turning the LCD off would flash white on a DMG.

`death_run()`: Rush, Ambush, Screech, Dupe or Figure → the screen goes black, the jumpscare image loads behind it, then
shows at once with a scream and 60 frames of screen shake, and fades out (`death_jumpscare`). Hide, Eyes and
Halt → straight to the fade. Then the
Guiding Light: the screen cleared to the font's space, GBC palette 7 made blue, the Guiding Light song, the
hint typed a character every 3 frames (A or Start shows the rest), then Try again / Title. Try again seeds
the new run from the time spent on that screen, like the title. The run's
seed is the number of frames the title was shown XOR the divider register when Start is pressed: a Game Boy
powers on identically every time, so a person's timing is the only randomness. AUTOTEST builds use the
fixed seed `AUTOTEST_SEED` (`game.h`: 0x1331 since Phase 5, which has Ambush, Dupe rooms with and without
him, Halt's door, Eyes and dark rooms in its first 30 rooms).

## Sprites (`sprites.c`)

8×16 hardware sprites. A 16×16 frame is two of them (left and right column); facing right is the left-facing
frame mirrored (`SPR_FLIPX`: the columns swap and both get the X-flip bit). Each sheet's frame table (tile
and attribute per column, `0xFF` = empty column) is copied to WRAM on load, so drawing needs no bank switch.
GBC: attribute bits 0–2 are the palette, offset by the sheet's base palette. DMG: bit 4 picks OBP0 or OBP1.
Entries are written through a byte pointer after one unsigned range check per axis: indexing the OAM structs
with 16-bit range checks cost ~9 DMG scanlines a hardware sprite. Since Phase 5 `spr_draw` writes both
columns itself (one row check, no `put()` call per column: five stack arguments a call made each hardware
sprite ~3 scanlines).

Frames bigger than 16×16 (Rush, Ambush and Eyes, 32×32) are stored as 16×16 parts, left to right, top to bottom,
and drawn with `spr_draw_big()`: 8 hardware sprites, 4 on a line, 6 with the player. Figure (32×48) is 12 hardware
sprites, 4 on a line; with the player, a glint and the arrow, 8. Planned: when more than 10 would share a
line (Seek), sprites will be cycled between frames in a fixed order.

## Player (`player.c`, banked)

- Position in pixels plus an 8-bit subpixel per axis. Speed 1.25 px/frame (75 px/s, about 4.7 metatiles a
  second); diagonals 0.883 px/frame per axis, so the speed is the same in all 8 directions.
- Collision box: the feet, x 3–12 and y 10–15 within the 16×16 sprite. The 3/4 view lets the head overlap a
  wall above. Each axis moves one pixel at a time and stops at the first solid metatile, so walking diagonally
  into a wall slides along it.
- Facing: 8 directions from the D-pad (`DIR_*`), kept when standing still. The sprite shows up, down or
  side; diagonals use the side frames.
- Animation: idle, step A, idle, step B, 8 frames each while walking (16 while sneaking).
- **Sneaking** (Phase 6): `player_sneak`, set by `game_frame` while B is held in the Library, halves the speed
  (0.63 px/frame). `player_moving` says whether a direction was held (Figure hears it).

## Asset pipeline (`scripts/assets.py`)

Runs from the Makefile; writes `build/gen/<kind>_<name>.c/.h`. The `.c` holds the GBC and DMG data behind
`#ifdef CGB`; the `.h` declares it and defines constants (`MT_HOTEL_FLOOR`, `SPR_PLAYER_LEFT_IDLE`, …).

- **Tileset** (`.tsx` + both PNGs): cuts 16×16 metatiles into 8×8 tiles, deduplicates them (separately per
  console), and writes the metatile tables. GBC palettes are found automatically: each 8×8 tile's colours
  (at most 4) are packed into at most 7 palettes, each sorted light to dark. Tile properties `solid` (bool)
  and `name` become `mt_flags` and the `MT_*` constants.
- **Room** (`.tmx`): the tile layer becomes a metatile map (empty cells are metatile 0, void); the objects
  (entry, exit, drawer, keyspot, spawn, closet, bed) become a `room_obj_t` list in metatile units, sorted by
  type; map properties `kind` and `allow_lock`. Validated: door sizes and positions, one entry/exit, drawers
  on dressers, closets and beds on their metatiles, somewhere for a key when locking is allowed.
- **Room list** (`roomlist`, all `.tmx` at once): `room_templates.c` in bank 0, with each template's kind,
  entry and exit walls, `allow_lock`, drawer, key-spot and hiding-spot counts (what `run.c` plans with,
  without switching banks), Rush's route (door centres, in 8 px units) and its length, and
  `room_template(id, &bank)`. Checks there's exactly one start room and that every exit wall has a
  room that can follow it.
- **Sprite sheet** (`.json` + both PNGs): frames in a grid, named in the JSON; 16×16, or multiples of 16
  stored as 16×16 parts (`SPR_<SHEET>_<FRAME>` is the first part). GBC: each 8×16 column
  gets a palette of up to 3 colours (+ transparency). DMG: each column uses OBP0 (white, dark, black) or OBP1
  (light, dark, black), picked automatically.
- **Font** (`.json` + both PNGs): 8×8 characters, one palette, and an ASCII → tile table.
- **Image** (the title logo, the jumpscares; both PNGs): deduplicated tiles (at most 184), a tile map, GBC
  palette per tile.
- **Songs**: hUGETracker's `uge2source` turns `assets/music/<name>.uge` into `music_<name>.c` (banked);
  `assets.py songfix` then replaces the empty instrument arrays it writes for an unused instrument kind,
  which SDCC rejects.

Rules for the art are in ASSETS.md; the converter stops with an error naming the place that breaks one.

## Audio (`audio.c`)

hUGEDriver 6.1.3 (the release's prebuilt GBDK library) plays one song: `music_play()` calls `hUGE_init` with
the song's bank switched in, `audio_frame()` calls `hUGE_dosound` once a frame the same way. Songs: the
Lobby Theme on the title, the Guiding Light on the death screen, *Unhinged* in the Library (Phase 6). The
other rooms have no music (the owner took the ambience out, 2026-10-18). Starting a song, or silence, first silences what the music left sounding (a
held note rings on once `hUGE_dosound` stops being called), except channels a sound effect is using; Start
on the title stops the Lobby Theme before the menu blip.

Sound effects are tables of steps (frames, then the channel's registers) on CH1, CH2 or CH4. Starting one
"mutes" that channel in hUGEDriver (it stops writing it); when it ends the channel is silenced and given back.
CH3 (wave) is the music's only: changing wave RAM would need `hUGE_current_wave` reset. Rush and Ambush's
rumble holds CH4 while they're out (`sfx_rumble`), retriggered only when its volume changes; a CH4 effect
interrupts it and it comes back after. `sfx_play_vol()` starts an effect with its first step's volume
replaced (Figure's footsteps, louder as he nears). `music_play()` mutes the lent channels again after `hUGE_init`, in case
that resets the driver's mute mask.
