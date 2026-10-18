# Building

## Requirements

Everything except make and Python lives in `tools/` (gitignored; `tools/README.md` lists it). Versions as of
2026-10-17:

| Tool | Version | Where it comes from | Goes to |
|---|---|---|---|
| GBDK-2020 | 4.5.0 | github.com/gbdk-2020/gbdk-2020/releases, `gbdk-win64.zip` | `tools/gbdk/` (the zip's own `gbdk` folder) |
| BGB (64-bit) | 1.6.6 | bgb.bircd.org, `bgbw64.zip` | `tools/bgb/`, then set up its `bgb.ini` (DEBUGGING.md) |
| SameBoy | 1.0.4 | github.com/LIJI32/SameBoy/releases, `sameboy_winsdl_v1.0.4.zip` (SHA-256 checked against the release's `SHA2-256SUMS`) | `tools/sameboy/` |
| Emulicious | 2026-03-27 build | emulicious.net/downloads, "Emulicious" (without bundled Java) | `tools/emulicious/` |
| PyBoy | 2.8.1 | pip, into a venv | `tools/venv/` |

- **GNU make**: the owner's devkitPro MSYS2 has one (`F:\Programs\devkitPro\msys2\usr\bin\make.exe`); `build.ps1`
  finds it there, in `C:\devkitPro`, or on PATH.
- **Python 3.13** for the venv: `C:\Python313\python.exe -m venv tools\venv`, then
  `tools\venv\Scripts\python.exe -m pip install pyboy pillow`. **Not 3.15**, the owner's default `python`:
  PyBoy has no wheels for it yet and building one from source fails (2026-10-17). 3.12–3.14 are also installed.
- **Java** (owner has Amazon Corretto 25) for Emulicious.
- Optional: **Tiled** (`F:\Programs\Tiled`) to edit rooms; **hUGETracker** to edit music; **hUGEDriver** (6.1.3 is
  the latest release) when audio starts.

## Build

```
.\build.ps1            # both ROMs, then checks their headers
.\build.ps1 gbc        # or dmg, all, clean
.\build.ps1 -Test      # also build the AUTOTEST ROMs and run every automated test on them (DEBUGGING.md)
```

A full `-Test` run takes about 3 minutes, most of it PyBoy.

Output:

| File | |
|---|---|
| `build/gbc/doors.gbc` | GBC-only ROM |
| `build/dmg/doors.gb` | DMG ROM |
| `build/gbc-autotest/`, `build/dmg-autotest/` | The same with `-DAUTOTEST`: the player walks a fixed route by itself (`src/input.c`). Only for tests |
| `build/<target>/doors.sym` | Symbols, loaded automatically by BGB, SameBoy and Emulicious from next to the ROM |
| `build/<target>/doors.map`, `.noi`, `.ihx` | Linker map, raw symbols, intermediate hex |
| `build/<target>/obj/` | Object files, assembler listings (`.lst`), and the `.rel` copies bankpack writes |
| `build/gen/` | C source generated from `assets/` (shared by all targets) |
| `build/screens/`, `build/preview/` | Screenshots from the test scripts, rendered previews |

Each target has its own folder because the linker names `.map`/`.noi`/`.sym` after the ROM's base name, and
both ROMs are called `doors`. `make AUTOTEST=1` builds into the `-autotest` folders.

The `Makefile` compiles `src/*.c` plus `src/platform/cgb/*.c` with `-DCGB`, or `src/platform/dmg/*.c` with
`-DDMG`, plus everything generated in `build/gen/`. New source files are picked up automatically; new assets
too (every `assets/tilesets/*.tsx`, `assets/rooms/*.tmx`, `assets/sprites/*.json`). Every object depends on
every header, which is simple and fast enough at this size.

**Assets** are converted by `scripts/assets.py` (ARCHITECTURE.md, ASSETS.md), run with the venv's Python.

**Autobanking** (`-autobank`): GBDK's bankpack assigns ROM banks to `#pragma bank 255` files at link time. It
must write its output to separate files (`-Wb-ext=.rel`). By default it overwrites the `.o` files, and the
next link that doesn't recompile everything then fails in `ihxcheck` **without any message** (2026-10-17).

**Build from PowerShell or `build.ps1`, not Git Bash**: from Git Bash, `lcc` passed bankpack a `/tmp/...`
path it couldn't open and the link failed.

**Paths with spaces:** the Makefile uses only relative paths, so the space in `F:\Gameboy Homebrew` doesn't
matter (no `subst` needed, unlike the Half-Life project). GBDK's `lcc` itself also works from a path with spaces.

## ROM header

Set by `lcc`'s makebin flags in the Makefile:

| Field | Value | Flag |
|---|---|---|
| Title | `DOORS` | `-Wm-yn"DOORS"` |
| Cartridge type | `0x1B` (MBC5 + RAM + battery) | `-Wm-yt0x1B` |
| RAM size | 32 KB (4 banks) | `-Wm-ya4` |
| ROM size | automatic (with `-autobank`) | `-Wm-yoA` |
| CGB flag | `0xC0` (GBC only) for `doors.gbc`; `0x00` for `doors.gb` | `-Wm-yC` on the GBC link only |
| Checksums | header and global, written by makebin | |
| `.sym` file | converted from `.noi` | `-Wm-yS` |

`scripts/check-rom.py` (run by `build.ps1`) checks the Nintendo logo, both checksums, the ROM size field
against the file size, and prints the type. A bad logo or header checksum stops a real console at the boot logo.
