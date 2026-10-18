# Architecture

As of Phase 3 (2026-10-18): the engine core (Phase 1), runs of generated rooms with doors, drawers and keys
(Phase 2), and hiding, Rush, Ambush, Hide, health, death and the Guiding Light, music and sound effects
(Phase 3). Sections marked **planned** aren't built yet.

## Repository layout

| Path | Purpose |
|---|---|
| `Makefile`, `build.ps1` | Build both ROMs: `build/gbc/doors.gbc` and `build/dmg/doors.gb`, plus the AUTOTEST variants (BUILDING.md) |
| `src/` | Game code in C, shared by both targets (module table below) |
| `src/platform/` | Code that differs per target: `cgb/` (double speed, palettes), `dmg/` (BGP/OBP palettes) |
| `assets/tilesets/` | Tiled tilesets (`.tsx`) of 16×16 metatiles, each with a `.gbc.png` and a `.dmg.png` (ASSETS.md) |
| `assets/rooms/` | Tiled rooms (`.tmx`) |
| `assets/sprites/` | Sprite sheets: `<name>.gbc.png`, `<name>.dmg.png` and `<name>.json` (frame names) |
| `assets/fonts/`, `assets/title/`, `assets/scares/` | The font; the title logo; the jumpscare screens |
| `assets/music/` | hUGETracker songs (`.uge`); `midi/` holds the owner's MIDI files they were arranged from. Sound effects are register scripts in `src/audio.c` |
| `scripts/` | `assets.py` (asset converter), `check-rom.py`, `bgb-check.ps1`, `run-test.py`, `scroll-test.py`, `vblank-budget.py`, `bot.py`, `record-run.ps1` (DEBUGGING.md) |
| `tests/` | `autotest_input.c`: the recorded run AUTOTEST builds replay (written by `bot.py`) |
| `tools/` | Downloaded tools, not in git (`tools/README.md`) |
| `build/` | Output, not in git: `gen/` (generated C from assets), `gbc/`, `dmg/`, `gbc-autotest/`, `dmg-autotest/` (ROM, `.sym`, `.map`, `obj/`), `screens/`, `preview/` |

### Source modules

| File | What it does |
|---|---|
| `main.c` | Startup (LCDC setup, sound), title → run → death screen → run or title |
| `title.c/.h` | *Banked.* The title screen (logo, blinking PRESS START, the Lobby Theme) and the run's seed |
| `game.c/.h` | *Banked.* Playing a run: entering rooms (fades, placing the player at the door), door states and number plates, drawers, keys, pressing A, walking through doors, the camera, the light level |
| `run.c/.h` | *Banked.* The run: seed, per-room random generator, which template each room uses, locks and key places, spawn rolls, what's been opened, health, Hide's bans |
| `entity.c/.h` | *Banked.* Rush and Ambush: the spawn queue, the route, the flicker, the rumble, the kill test |
| `hide.c/.h` | *Banked.* Closets and beds, and Hide's timer, heartbeat and eviction |
| `death.c/.h` | *Banked.* The jumpscare, the Guiding Light (typed hint, Try again / Title) |
| `audio.c/.h` | hUGEDriver music (one song at a time) and the sound-effect player |
| `image.c/.h` | Full-screen pictures (logo, jumpscares) and clearing the screen, STAT-safe |
| `assets.h` | The data types the generated asset files use: `tileset_t`, `room_def_t`, `room_obj_t`, `sprite_sheet_t`, `font_t`, `image_t`; `TILESET_*`, `OBJ_*`, `WALL_*` |
| `room.c/.h` | The current room: map and objects in WRAM, the tileset's metatile tables in WRAM, collision queries, changing a metatile on screen, door-number plates |
| `bg.c/.h` | Camera and background streaming (below); the single-tile write queue; STAT-safe VRAM copy; VRAM writes in inline assembly |
| `hud.c/.h` | The font, the on-demand HUD on the window layer, messages, text printing |
| `sprites.c/.h` | Sprite sheets in VRAM, drawing 16×16 frames as two 8×16 hardware sprites (bigger frames as several), the per-frame shadow OAM allocator |
| `player.c/.h` | 8-direction walking, collision, corner nudging, facing (8 directions), walk animation |
| `input.c/.h` | Joypad held/pressed; in AUTOTEST builds the recorded run instead of the joypad |
| `platform.h`, `platform/*/` | Per-console setup, palettes (kept in WRAM, written in VBlank), the fade to black, dimming and Ambush's green tint |

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
| Effects | Flicker and hiding dim the room palettes (not the sprites or the HUD); Ambush tints them green. **Planned**: palette-based darkness | Flicker and hiding shift BGP darker (the HUD dims too: one BG palette); no tint. **Planned**: tile-based darkness |

`#ifdef CGB` appears in `src/platform/`, the generated asset files, and where GBC attributes or palettes are
written (`room.c`, `bg.c`, `sprites.c`). Game logic is the same. The DMG build is the performance floor:
anything in the main loop must fit a DMG frame.

## Memory map

| Area | Use |
|---|---|
| ROM bank 0 | Engine code that must stay put (no `#pragma bank`): everything that switches banks to read assets (`room.c`, `image.c`, `hud.c`, `sprites.c`, `audio.c`), streaming, player, input, platform; GBDK library; hUGEDriver (1.9 KB); the room template table (`room_templates.c`). **84% full on GBC, 75% on DMG** as of Phase 3 |
| ROM banks 1+ | Banked code (`game`, `run`, `title`, `entity`, `hide`, `death`: `#pragma bank 255` and `BANKED` functions), generated asset data, songs, the AUTOTEST recording, all placed by GBDK's autobanking. The ROM is 64 KB. Banked code must never `SWITCH_ROM` (it would switch itself out), and may pass pointers to its own constants (strings) only to bank-0 functions, which read them while the caller's bank is still in |
| VRAM 0x8000–0x87FF | Sprite tiles 0–127: player 0–35, key 36–39, then Rush and Ambush (`entity_load`) |
| VRAM 0x8800–0x97FF | Background tiles 0–255 (LCDC bit 4 clear: tiles 0–127 at 0x9000, 128–255 at 0x8800): **tileset 0–183** (the title logo and the jumpscares borrow them), **door-number plates 184–191** (2 tiles each, made at runtime), **font 192–255**. Sprite tiles 128–255 would overlap this, so sprites stay below 128 |
| VRAM 0x9800 | Background map (32×32), streamed as below. GBC: attributes in VRAM bank 1 at the same address |
| VRAM 0x9C00 | Window map: the HUD (row 0 border, row 1 text). GBC: attributes all palette 7 |
| GBC BG palettes | 0–6 the tileset (or the title logo / a jumpscare), 7 the font/HUD (blue on the Guiding Light) |
| GBC sprite palettes | 0 player, 1 key, 2 Rush, 3 Ambush |
| WRAM | Room map (`room_map`, up to 3,072 metatiles) and objects, metatile tables (`mt_tiles`, `mt_flags`, `mt_attrs`: 128 metatiles, 120–127 made at runtime), run state (~600 bytes for 101 rooms), the GBC's cached dim levels (448 bytes), hUGEDriver's variables, the tile write queue, frame tables, GBDK's shadow OAM at 0xC000, the stack from 0xE000 down. DMG uses ~4.9 of 8 KB |
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
4. Rush / Ambush move (`entity_update`). Hide or Rush / Ambush can end the run here (`GAME_DEAD`).
5. The light level: the larger of hiding's darkness and the flicker (`platform_set_dim`).
6. HUD: timers, slide, queue changed text.
7. Camera: target = player centre minus half the screen, clamped to the room; moved at most 4 px per axis
   per frame. `bg_set_camera()` builds the next column or row, or a batch of queued single tiles.
8. Sprites: `spr_begin()`, draw calls into shadow OAM, `spr_end()` hides entries left over from last frame.
9. `audio_frame()`: a music tick and the sound effects' next steps. After the VBlank work on purpose:
   hUGEDriver takes a few scanlines and the DMG has two to spare.

Loops outside play (the title, fades, the death screen) use `game_wait_frames()`, which does steps 1–2 and 9
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
  centre (the reception: spawn to exit), written by `assets.py` with its length. The entity's position is a
  room number and a distance along that room's route, so he exists in rooms the player isn't in; he's drawn
  only in this one, placed by interpolating between the two door centres.
- **A pass**: fired → 45 frames → the flicker (Rush 2 s with an electric buzz; Ambush 4 s, green on GBC, with
  his howl) → he starts 1,000 px of route behind this room's entry door and travels to the far door (the
  furthest room reached's exit): Rush at 5.33 px/frame, Ambush 8.53. Rush vanishes there. Ambush turns, goes
  back to where he started, waits 5 s and comes again: 3–5 legs in all (from the seed), so he leaves at the
  far door or the rear. While either is out, no door opens or lets you through.
- **Kill test**: same room, player not hidden, within 96 px (Rush) / 128 px (Ambush) of him, through walls.
- **Rumble**: continuous CH4 noise whose volume follows his distance along the route, from 768 px.
- **Hiding**: A at a closet or bed hides (the player sprite goes, the room dims to level 2); A or B leaves.
  Hide: from 7 s a heartbeat that speeds up (with a darker pulse on each beat, the warning without sound),
  at 12 s out with 40 HP gone, that spot banned for the run (`run_banned`, a bit per spot). Damage at 40 HP or
  less kills (Hide's Guiding Light).

## Fades, dimming and palettes (`platform/*`)

`platform_set_fade(0..3)` darkens everything one step (3 = black). `platform_set_dim(0..3, tint)` darkens
only the room: on GBC BG palettes 0–6 (not 7, the HUD; not sprites, so Rush stays visible), on DMG BGP (the
HUD too). Palettes are kept in WRAM, computed when set, and only copied in the next VBlank by
`platform_vblank()` (called from `bg_flush`): on GBC because palette RAM can't be written while the LCD
draws; on DMG it's just the BGP/OBP registers with every shade shifted toward black.

**GBC dim cache**: the flicker changes level every 4 frames. Recomputing 28 colours each time (a division per
channel) cost the GBC 92 of 332 frames during Ambush's flicker. Each level × tint is now computed the first
time it's needed after the palettes or the fade change, and kept (8 × 56 bytes); a flicker step is a copy. A room
change: fade out (3 steps × 3 frames), hide sprites and HUD, load and draw the room with the screen black,
fade in. The LCD stays on throughout: turning it off would flash white on a DMG.

## HUD (`hud.c`)

The window layer at 0x9C00, 2 rows: a border line and 20 characters of text, sliding up 2 px a frame from
WY 144 to 128 while shown. It shows the status line (`DOOR 012`, a heart and an 8-tile health bar in 2 px
steps from font glyphs, the key icon) on entering a room, when health changes and while Select is held, or a
centred message ("IT'S LOCKED.", "GOT KEY FOR DOOR 012", "GET OUT!"). Changed characters
go through the single-tile queue. On GBC the window's attributes are all palette 7 (the font's).

## Title and death screen (`title.c`, `death.c`, `image.c`)

The logo (an `image_t`: tiles, map, GBC attributes) is loaded over the tileset's BG tiles by `image_show()`,
which then calls `room_forget_tileset()` so the next room reloads the tileset (STAT-safe, behind the fade).
At boot the LCD is still off; coming back from a run the title is drawn behind a fade to black instead,
because turning the LCD off would flash white on a DMG.

`death_run()`: Rush or Ambush → the screen goes black, the jumpscare image loads behind it, then shows at
once with a scream and 60 frames of screen shake, and fades out. Hide → straight to the fade. Then the
Guiding Light: the screen cleared to the font's space, GBC palette 7 made blue, the Guiding Light song, the
hint typed a character every 3 frames (A or Start shows the rest), then Try again / Title. Try again seeds
the new run from the time spent on that screen, like the title. The run's
seed is the number of frames the title was shown XOR the divider register when Start is pressed: a Game Boy
powers on identically every time, so a person's timing is the only randomness. AUTOTEST builds use the
fixed seed `0xC0DE`.

## Sprites (`sprites.c`)

8×16 hardware sprites. A 16×16 frame is two of them (left and right column); facing right is the left-facing
frame mirrored (`SPR_FLIPX`: the columns swap and both get the X-flip bit). Each sheet's frame table (tile
and attribute per column, `0xFF` = empty column) is copied to WRAM on load, so drawing needs no bank switch.
GBC: attribute bits 0–2 are the palette, offset by the sheet's base palette. DMG: bit 4 picks OBP0 or OBP1.

Frames bigger than 16×16 (Rush and Ambush, 32×32) are stored as 16×16 parts, left to right, top to bottom,
and drawn with `spr_draw_big()`: 8 hardware sprites, 4 on a line, 6 with the player. Planned: when more
than 10 would share a line (Figure, Seek), sprites will be cycled between frames in a fixed order.

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
Lobby Theme on the title, a quiet ambience in the hotel, the Guiding Light on the death screen.

Sound effects are tables of steps (frames, then the channel's registers) on CH1, CH2 or CH4. Starting one
"mutes" that channel in hUGEDriver (it stops writing it); when it ends the channel is silenced and given back.
CH3 (wave) is the music's only: changing wave RAM would need `hUGE_current_wave` reset. Rush and Ambush's
rumble holds CH4 while they're out (`sfx_rumble`), retriggered only when its volume changes; a CH4 effect
interrupts it and it comes back after. `music_play()` mutes the lent channels again after `hUGE_init`, in case
that resets the driver's mute mask.
