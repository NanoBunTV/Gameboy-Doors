# Testing and debugging

## Hardware rules (because nobody can test on a real Game Boy)

Emulators are more forgiving than the hardware. These rules are what keeps the ROM working on a real console;
break one and it may work everywhere except a Game Boy.

- **VRAM only when it's accessible**: during VBlank, HBlank (mode 0), or with the LCD off. Writes during mode 3
  are silently dropped on hardware. OAM likewise, and OAM is written only through the DMA routine in HRAM
  (GBDK does this in its VBlank handler from shadow OAM).
- **Never turn the LCD off outside VBlank.** On a DMG this can damage the screen. GBDK's `DISPLAY_OFF` waits for
  VBlank; never clear LCDC bit 7 by hand.
- **Don't read uninitialised RAM.** Hardware RAM starts random; emulators start at zero.
- **Don't use echo RAM** (0xE000–0xFDFF) or the prohibited area (0xFEA0–0xFEFF).
- **SRAM:** enable it (0x0A to 0x0000–0x1FFF) only while using it, then disable it, so a power-off doesn't corrupt
  the save. Check a magic number and checksum when loading.
- **Valid header and checksums**, or the boot ROM locks up at the logo (`scripts/check-rom.py`).
- **GBC double speed:** switch with the documented `STOP` sequence (GBDK's `cpu_fast()`), once at boot.
- **Sprite limits:** 10 per line, 40 total; hardware drops the extras.

## BGB

Two installs, kept apart:

- **The owner's BGB** (`E:\ROMs and Emulators\_Emulators\BGB (GameBoy)`, 1.6.4) is for playing. **Never run or
  change it from this project** (owner, 2026-10-17). It was used headless (which doesn't write `bgb.ini`)
  during Phase 0 before the project had its own copy; its files were checked afterwards and are unchanged.
- **The project's BGB** in `tools/bgb/` (1.6.6, 64-bit, `bgb64.exe`) is for development. Its `bgb.ini` was
  changed from the defaults to:

  | Setting | Value | Meaning |
  |---|---|---|
  | `DebugVRAMbreak` | 1 | Break on VRAM/OAM access while it's inaccessible |
  | `DebugDisableLCD` | 1 | Break on LCD off outside VBlank |
  | `DebugOAMDMABreak` | 1 | Break on bad memory access during OAM DMA |
  | `DebugSramBreak` | 1 | Break on access to SRAM that isn't enabled |
  | `DebugWRAMbreak`, `UninitedWRAM` | 1, 1 | WRAM starts random, break on reading it before it's written |
  | `DebugEcho` | 1 | Break on echo RAM |
  | `InvalidOpBreak` | 1 | Break on invalid opcodes |
  | `EmulateInaccVRAM` | 1 | "Emulate as in reality" (default) |
  | `BootromEnabled`, `DmgBootRom`, `CgbBootRom` | 1, `dmg_boot.bin`, `cgb_boot.bin` | SameBoy's free boot ROMs (MIT), copied from `tools/sameboy/` |

  If `tools/` is rebuilt from scratch, set these again (Options → Exceptions, Options → System), or copy the
  lines into the new `bgb.ini`.

Run a build by hand: `tools\bgb\bgb64.exe build\gbc\doors.gbc` (`-watch` reloads it after each build). It loads
`doors.sym` from next to the ROM, so the debugger shows function names.

### Scripted check: `scripts/bgb-check.ps1`

```
.\scripts\bgb-check.ps1 build\gbc-autotest\doors.gbc
.\scripts\bgb-check.ps1 build\dmg-autotest\doors.gb -Mode dmg    # or -Mode gbc: DMG ROM on a GBC
.\build.ps1 -Test                                                  # all three combinations, AUTOTEST ROMs
```

1. **Rule check**: runs the ROM headless at full speed (`-hf`) for `-Seconds` (default 10; `build.ps1` uses
   15) with all the breaks above passed as `-set` options. Headless BGB exits as soon as it would break to the
   debugger, so **still running at the end = pass; exited early = a hardware rule was broken** (the screen at
   that moment is saved as `…-FAIL.bmp`). BGB runs about 1,100 frames a second headless.
2. **Screenshot**: a breakpoint `-br _vsync/..N` (`-Frames`) and the screen saved to
   `build/screens/bgb-<build folder>-<mode>.png`. **N isn't a frame count**: at N = 300 and 900 BGB showed
   rooms 3 and 11 of the recorded run, which PyBoy reaches only around frames 900 and 2,600. The rooms were the
   right ones (same run), so it's only the counter that's off. Use it as "somewhere later in the run".

Headless BGB can't press buttons, so `build.ps1 -Test` checks the **AUTOTEST** builds, which replay a
recorded run (below): title, 30 rooms, keys, locked doors, walking back once, hiding from Rush and Ambush,
then a death, the jumpscare, the Guiding Light and Try again. That way all the VRAM writing (scrolling, door
and drawer changes, HUD text, plates, tileset reloads, jumpscare and screen fills) and the palette changes
run under the exception breaks. Headless BGB is fast enough that 15 s covers all of it: it reaches the first
hide in under 2 s and `death_run` in about 5 s (checked 2026-10-18 with `-br _hide_try` and
`-br _death_run`: a breakpoint makes headless BGB exit, so the time it exits is when the code was reached).

Checked on 2026-10-17 with a deliberately bad ROM (writes VRAM in a loop with the LCD on): it fails in under a
second, while the real ROMs pass, so the breaks really are active.

Findings from setting this up:

- **`SystemMode`**: 0 = DMG, 1 = GBC, 3 = automatic from the header (found by testing; not documented).
- Screenshots use whatever colour scheme the ini has. In the owner's install that was "super gameboy" (cream
  and red), so a DMG screenshot from there looked like a GBC one. Colours in `bgb.ini` are stored as BGR.
- Breakpoint conditions such as `..300` (every 300th hit) work on the command line; `[address]==value`
  conditions didn't seem to trigger and weren't pursued.

## PyBoy (headless tests)

```
tools\venv\Scripts\python.exe scripts\run-test.py build\gbc\doors.gbc --script "wait 180; shot boot; hold right 40; shot moved"
```

Boots a ROM (GBC mode for `.gbc`, DMG for `.gb`, `--dmg` to force DMG), plays a button script and saves
screenshots to `build/screens/`. Unlike BGB it can press buttons, so it's for "does input do the right thing".
PyBoy is less accurate than BGB: never treat a PyBoy pass as a hardware pass.

### The bot and the recorded run: `scripts/bot.py`, `scripts/record-run.ps1`

```
tools\venv\Scripts\python.exe scripts\bot.py build\dmg-autotest\doors.gb --rooms 40 --shots -v
.\scripts\record-run.ps1                      # re-record tests\autotest_input.c (30 rooms)
tools\venv\Scripts\python.exe scripts\bot.py build\gbc-autotest\doors.gbc --check 30
```

The bot plays like a person: it reads the game's state from WRAM by symbol (`room_map`, `mt_flags`,
`room_objs`, `run_flags`, `run_drawers`, `run_key_slot`, `run_key_door`, `run_banned`, `player_x/y`,
`game_state`, `entity_state`, `hide_hidden`, `hide_frames`, `input_frame`, `screech_state`, `screech_dir`,
`eyes_here`, `eyes_x/y`, `run_flags2`), finds paths with a breadth-first search over walkable metatiles (with
Eyes in the room, a cheapest-path search where a step toward him costs 8: it walks round him rather than
straight at him), searches drawers in order until the key turns up, picks up keys in the open,
unlocks and opens doors and walks through. At room 3 it goes back to room 2 and returns, to test walking
back. **When Rush or Ambush is announced** it drops what it's doing, walks to the nearest closet or bed Hide
hasn't banned, hides, and stays until he's gone; after 9 s hidden, while Ambush waits at the rear, it steps
out and straight back in, which restarts Hide's clock. It taps A until each step shows in WRAM, since a tap
can land on a frame the game didn't read the joypad (and waits two frames after a door opens: on DMG the
door's redraw can run past the end of the frame, so the room isn't walkable yet when the flag is set).
**When Screech appears** it walks a step his way and stands facing him until he's gone. On entering a dark
room in an AUTOTEST or DEBUG build it presses B once, so successive dark rooms get the lighter, the flashlight
and the glow in turn. Dying fails the run.

`--shots` saves a screenshot on entering each room (`build/screens/bot-room-NNN.png`); `--entity-shots` every
6th frame while hidden (`ent-<hide>-<frame>.png`: Rush and Ambush crossing, the flicker); `--door-shots
PREFIX` one of each exit door as it walks up to it (`PREFIX-door-NNN.png`); a failure saves
`bot-fail.png` and names the room and template. `--die-at-end` (used by `record-run.ps1`): after the last
room, play on until something comes, stand still to be caught, skip the Guiding Light's typing with A and
pick Try again.

`--record` writes the buttons held at every `input_update()` call, run-length coded, to
`tests/autotest_input.c` (about 1.5 KB for 30 rooms and the death). It records from a PyBoy hook on
`input_update` itself. It used to compare `input_frame` before and after each tick, and a frame boundary
falling between the joypad read and the count put the recording one input out of step (found 2026-10-18,
when heavier frames made it common). AUTOTEST builds replay that instead of reading the joypad, with the fixed
seed `AUTOTEST_SEED` (0x1331, `game.h`; Try again also uses it). It was 0xC0DE until Phase 4 and 0x01D9 in
Phase 4. 0x1331 has Ambush in 5, wrong doors without Dupe in 2 and 7 (the bot presses A at the first:
"IT'S FAKE."), Dupe rooms 9, 12 and 18 (the bot opens a wrong door in 12: his face, 40 HP), Halt's door in
10 (the bot walks the hallway by `halt_dir`), Eyes in 11 and 18, and dark rooms from 15. Seeds are chosen with
`scripts/plan.py`, a Python copy of `run.c`'s planner (keep it in step with `plan_room()`). **The game is deterministic in its input**:
on DMG and GBC every room change, hide and the death happen at the same input frame (checked 2026-10-18).
`--check N` replays and fails if room N isn't reached; `build.ps1 -Test` runs it (room 30 at input frame
10,292 on both, Phase 5; 8,979 in Phase 4). Snapshots taken once per emulated frame can differ by one input frame between consoles:
PyBoy samples WRAM once per emulated frame and the GBC, at double speed, has finished more of the frame by
then, so state that changes mid-frame is seen one input frame apart. Compare events (room changes), not per-frame snapshots, between consoles.

**Re-record after any change that alters what the same buttons do** (rooms, generation, door positions,
speed, entity timing, interaction): `.\scripts\record-run.ps1`. It writes an empty recording first, because
the bot needs an AUTOTEST ROM that reads the joypad.

**Rare events** are tested by poking WRAM from a PyBoy script (never during `--record`: the replay wouldn't
have the poke). Timothy (1 drawer in 200): set `run_force_timothy` to 1 (DEBUG and AUTOTEST builds) and the
next drawer opened that doesn't hold the key holds him; checked 2026-10-18 on both consoles (100 → 90 HP, and
5 → 1: he never takes the last). A dark room or Eyes: set `RF2_DARK` / `RF2_EYES` in `run_flags2[n]` before
entering room n (Phase 4). Dupe and Halt come up in the recorded run's seed.

### Background correctness: `scripts/scroll-test.py`

```
tools\venv\Scripts\python.exe scripts\scroll-test.py build\dmg-autotest\doors.gb 14000 --every 2
```

Runs an AUTOTEST ROM (playing its recorded run) and every 2nd frame compares the whole screen with a reference
picture of the current room built from the game's own `room_map` in WRAM and the tileset PNG, at the camera
position. Not compared: sprites (from OAM), the HUD window, door-number plates (runtime metatiles), fades and
room loads, frames with the room dimmed (hiding, the flicker: `platform_dim` is not 0), and 12 frames after
any metatile change (a door or drawer opening is redrawn a frame or two later
on purpose). It first learns which screen colour each art colour becomes (PyBoy's DMG greys and GBC colour
conversion differ from the PNG), then counts wrong pixels. Failing frames are saved as
`build/screens/scroll-fail-<frame>.png`. Part of `build.ps1 -Test`, over 14,000 frames (the whole replay;
11,000 until Phase 5's hallway made it longer). **Halt's hallway** is compared too (game state 4), with its
own learned colours (blue on GBC), skipping flash frames (`platform_flash`).

**Dark rooms** are compared too: the reference is black wherever the game's light state (`light_src_*`,
`light_l/r` in WRAM, `src/light.c`) says a tile is unlit, on frames where the tile queue is empty and no light
redraw is pending (`vq_head == vq_tail`, `bg_vq_n == 0`, `light_pending == 0`). Unlit tiles get their own
reference colour: on GBC the font's black isn't the same screen colour as the wall art's black.

- **PyBoy timing**: when `tick()` returns, the game's VBlank code has already run, so SCX/SCY, OAM and the map
  hold the *next* frame's values. The script uses what it read after the previous tick. Without this, every
  frame looked wrong.
- **It catches real bugs**: with the column write's map wrap deliberately broken (`cp #0x9C` → `#0x9D` in
  `bg.c`), 382 of 924 frames failed (Phase 1) and 405 of 1,088 (Phase 2 version). Removing the one-tile margin
  was *not* caught: tiles written earlier stay in the 32×32 ring, so a late margin rarely shows.

### ROM and RAM use

```
tools\gbdk\bin\romusage.exe build\gbc\doors.map
```

As of Phase 5: bank 0 88% (GBC, 1.9 KB free) / 76% (DMG); DMG WRAM about 6.4 of 8 KB; GBC WRAM bank 1 (0xD000) 96%.
Phase 4 ended at 90% / 79%, then 84% / 73% once the HUD's status line and the effect scripts moved into banks;
Phase 5 added the per-exit route tables (bank 0) and `run_exit` (WRAM). Phase 3 was 84% / 75%; Phase 4's light code briefly overflowed GBC bank 0 until `player.c` and
moving a light (`light_move.c`) went into banks. `romusage -g` prints the table; for what takes bank 0, sum
`A _CODE size` per object file (`grep "^A _CODE size" build/gbc/obj/*.rel`).

### VBlank budget: `scripts/vblank-budget.py`

```
tools\venv\Scripts\python.exe scripts\vblank-budget.py build\dmg-autotest\doors.gb 4000
```

AUTOTEST builds record how many scanlines into VBlank `bg_flush()` finished, worst case
(`autotest_flush_lines` in `game.c`). The script runs the ROM and reads it out; 10 or more means it overran
VBlank, and the script fails. PyBoy's timing is close but not exact. As of Phase 2 (over the 30-room run,
including single-tile batches): **DMG 8 of 10, GBC 6 of 10** (ARCHITECTURE.md, "Scrolling", for the history).
Phase 3's flicker briefly made it 12 on GBC (palette upload + a column in one VBlank) until map writes were
made to wait a frame after a palette upload.

**Lag frames** (frames where the game didn't call `input_update`) show slow logic. Check that a "missing"
frame isn't paired with a frame that called it twice: PyBoy's tick ends at LY 0, so a loop that reads input
just before line 154 can tip a call into the next tick without losing a frame. Halt's hallway first measured
292 of 1,956 that way because its loop ran `audio_frame()` before the game logic; in `main()`'s order (audio
last) it's 1. Count them per room by
comparing `input_frame` before and after each PyBoy tick of the replay; this found the GBC's palette
recomputation during Ambush's flicker (92 of 332 frames, ARCHITECTURE.md "Fades, dimming and palettes"; now 4
per pass) and Phase 4's slow darkness (DMG dark rooms dropped 30–37% of frames).

**Where the time goes** (a profiler, of sorts): `pyboy.hook_register(bank, address, callback)` on function
entries, reading LY (0xFF44) in each callback, gives the scanline each function starts on. Take the
functions from the compiler's listings (`; Function name` lines in `build/<target>/obj/*.asm`) and their
addresses from the `.sym`. **Only hook code**: PyBoy patches the byte at the address, so a hook on a data
symbol (a table in bank 0) corrupts reads and the game goes another way. A 154-line DMG frame has room for
about 140 lines of game logic. Phase 4's numbers this way: the HUD rebuilding its line every frame ~45 lines,
a hardware sprite ~9, each `room_solid_at` ~3 (a multiply per lookup, now a row table), the first light code
~140 to move the glow one tile, and Rush/Ambush's `place()` ~100 while he moved (two 32-bit divisions a
frame; now worked out once per room and stepped). Phase 5: a hardware sprite still ~3 lines through a
`put()` call per column (five stack arguments); `spr_draw` writing both columns itself saved ~11 lines a
frame in Halt's hallway (6 sprites for Halt, 10 for TURN AROUND, 2 for the player).

### Audio

PyBoy can emulate sound (`sound_emulated=True`; `pyboy.sound.ndarray` after each tick, signed 8-bit stereo).
On 2026-10-18 20 s of the title measured RMS 13.8 / peak 36, saved as `build/screens/title.wav`. That proves
the driver runs and the song plays, not that it sounds right: listen in BGB or hUGETracker. After Start: the
blip, then 0.0 (the Lobby Theme's last note used to ring on; fixed in Phase 4).

## SameBoy and Emulicious (accuracy cross-checks)

`tools\sameboy\sameboy.exe` and `tools\emulicious\Emulicious.exe` (or `java -jar Emulicious.jar`). Both are GUI
only as far as we know: Emulicious opened a window even with `-h`. They're for the owner, or for timing-sensitive
code (mid-frame effects, the VBlank budget, double speed) before calling it done. Emulicious has a profiler and
its own uninitialised-memory and VRAM access checks.

## Things learned

- Python 3.15 can't install PyBoy (no wheel); the venv uses 3.13 (BUILDING.md).
- GBDK's bankpack overwrites `.o` files unless given `-Wb-ext=.rel`; relinks then fail silently in
  `ihxcheck` (BUILDING.md). If a link fails with no message, suspect this first.
- `BANK(x)` isn't a constant expression for SDCC: it can't go in a static initializer (`room.c`).
- Inline assembly (`__asm`) goes through the C preprocessor, so `#ifdef CGB` works inside it. Avoid
  apostrophes in its comments (the preprocessor may treat them as an open character literal).
- An array of structs in generated C needs braces per element (`{ {1,2}, {3,4} }`); SDCC rejects a flat list.
- PyBoy only renders the screen on ticks with `render=True`; screenshots after unrendered ticks are blank.
- The room order repeats in cycles with the current 9 templates: with 2–3 templates per entry wall and the
  no-repeat rule, there's often only one choice (STATUS.md, known issues).
- Banked code (`#pragma bank 255`) must not `SWITCH_ROM`: it would switch itself out. Anything that reads
  banked assets stays in bank 0 (`image_show()` exists so `title.c` and `death.c` can be banked). Passing a
  banked file's string literal to a bank-0 function is fine (it's read while the caller's bank is still in);
  passing it to another banked file isn't.
- hUGETracker's `uge2source` picks its output format from the file extension (`.c` or `.asm`), so it can't
  write to a `.tmp` file; and it writes `{ }` for an instrument kind a song never uses, which SDCC rejects
  (`assets.py songfix` fixes it up).
- Writing Python that contains `\1` or `\w` through a Bash heredoc mangled the backslashes (a raw control
  character ended up in `assets.py`). Write such scripts to a file first.
