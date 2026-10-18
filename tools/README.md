# tools/

Downloaded tools; everything here except this file is gitignored. How to get each one again is in
[../Docs/BUILDING.md](../Docs/BUILDING.md).

| Folder | What |
|---|---|
| `gbdk/` | GBDK-2020 4.5.0 (compiler, linker, `png2asset`, `romusage`) |
| `bgb/` | BGB 1.6.6, 64-bit: the project's own copy, with every exception break on in its `bgb.ini` and SameBoy's boot ROMs. Used by `scripts/bgb-check.ps1` |
| `sameboy/` | SameBoy 1.0.4 (Windows SDL build), accuracy cross-check; also the source of the free boot ROMs |
| `emulicious/` | Emulicious (2026-03-27 build), accuracy cross-check and profiler (runs on the installed Java) |
| `venv/` | Python 3.13 virtual environment with PyBoy 2.8.1, Pillow and mido, for `scripts/` |
| `hugedriver/` | hUGEDriver 6.1.3: the music driver, prebuilt for GBDK (`gbdk/hUGEDriver.lib`) and its header. Linked into both ROMs |
| `hugetracker/` | hUGETracker 1.0.11: opens and plays `assets/music/*.uge`; its `uge2source.exe` converts them in the build |
| `_dl/` | The downloaded archives |
