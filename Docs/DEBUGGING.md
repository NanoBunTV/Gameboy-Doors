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

1. **Rule check**: runs the ROM headless at full speed (`-hf`) for 10 s with all the breaks above passed as
   `-set` options. Headless BGB exits as soon as it would break to the debugger, so **still running at the end
   = pass; exited early = a hardware rule was broken** (the screen at that moment is saved as `…-FAIL.bmp`).
   BGB runs about 1,100 frames a second headless, so 10 s is about 11,000 frames.
2. **Screenshot**: breaks on the Nth call of GBDK's `_vsync` (`-br _vsync/..N`, `-Frames`, default 300;
   `build.ps1` uses 900) and saves `build/screens/bgb-<build folder>-<mode>.png`.

Headless BGB can't press buttons, so `build.ps1 -Test` checks the **AUTOTEST** builds, where the player walks
a fixed route by itself (`src/input.c`) through both halves of the test room, in all 8 directions. That way the
scrolling code, which does most of the VRAM writing, runs under the exception breaks.

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

### Scrolling correctness: `scripts/scroll-test.py`

```
tools\venv\Scripts\python.exe scripts\scroll-test.py build\dmg-autotest\doors.gb 4000 --every 2
```

Runs an AUTOTEST ROM and every 2nd frame compares the whole screen, minus sprites, with a reference picture of
the room built from the tileset PNG. It first learns which screen colour each art colour becomes (PyBoy's
DMG greys and GBC colour conversion differ from the PNG), then counts wrong pixels. Failing frames are saved
as `build/screens/scroll-fail-<frame>.png`. Part of `build.ps1 -Test`.

- **PyBoy timing**: when `tick()` returns, the game's VBlank code has already run, so SCX/SCY and OAM hold the
  *next* frame's values. The script uses the values read after the previous tick. Without this, every frame
  looked wrong.
- **It catches real bugs**: with the column write's map wrap deliberately broken (`cp #0x9C` → `#0x9D` in
  `bg.c`), 382 of 924 frames failed. Removing the one-tile margin was *not* caught: tiles written earlier stay
  in the 32×32 ring, so a late margin rarely shows on the test route.

### VBlank budget: `scripts/vblank-budget.py`

```
tools\venv\Scripts\python.exe scripts\vblank-budget.py build\dmg-autotest\doors.gb 4000
```

AUTOTEST builds record how many scanlines into VBlank `bg_flush()` finished, worst case
(`autotest_flush_lines` in `main.c`). The script runs the ROM and reads it out; 10 or more means it overran
VBlank, and the script fails. PyBoy's timing is close but not exact. As of Phase 1: **DMG 8 of 10, GBC 6 of
10** (ARCHITECTURE.md, "Scrolling", for the history).

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
