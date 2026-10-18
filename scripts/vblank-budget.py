"""Runs an AUTOTEST ROM in PyBoy and reports how far into VBlank bg_flush() got (main.c, measure_flush).

Usage: tools/venv/Scripts/python.exe scripts/vblank-budget.py build/dmg-autotest/doors.gb [frames]

VBlank is 10 scanlines (144-153). A result of 10 or more means the VRAM writes ran past VBlank: still
correct (every write waits for STAT), but a new column could show up a frame late at the screen edge.
PyBoy's timing is close but not exact; treat the number as an estimate.
"""
import sys
from pathlib import Path

from pyboy import PyBoy


def symbol(sym_file: Path, name: str) -> int:
    for line in sym_file.read_text().splitlines():
        parts = line.split()
        if len(parts) == 2 and parts[1] == name:
            return int(parts[0].split(":")[1], 16)
    raise SystemExit(f"{name} not in {sym_file} (is this an AUTOTEST build?)")


def main():
    rom = Path(sys.argv[1])
    frames = int(sys.argv[2]) if len(sys.argv) > 2 else 2000
    addr = symbol(rom.with_suffix(".sym"), "_autotest_flush_lines")
    pyboy = PyBoy(str(rom), window="null", cgb=rom.suffix == ".gbc", sound_emulated=False)
    pyboy.set_emulation_speed(0)
    pyboy.tick(frames, False)
    lines = pyboy.memory[addr]
    pyboy.stop(save=False)
    verdict = "within VBlank" if lines < 10 else "RAN PAST VBLANK"
    print(f"{rom}: bg_flush ended at most {lines} scanlines into VBlank (of 10) over {frames} frames: {verdict}")
    raise SystemExit(0 if lines < 10 else 1)


if __name__ == "__main__":
    main()
