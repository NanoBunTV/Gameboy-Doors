"""Boots a ROM headless in PyBoy, plays a button script, and saves screenshots.

Usage:
  tools/venv/Scripts/python.exe scripts/run-test.py build/doors.gbc [--dmg] [--script "wait 120; shot boot; hold right 30; shot moved"]

Script commands, separated by ';':
  wait N          run N frames
  hold BTN N      hold a button (a b start select up down left right) for N frames, then release
  press BTN       tap a button (one frame down, one up)
  shot NAME       save build/screens/<rom>-<NAME>.png

--dmg runs the ROM as an original Game Boy (default: GBC for .gbc files, DMG for .gb files).
PyBoy is less accurate than BGB/SameBoy; use this for "does it boot and draw", not timing.
"""
import argparse
from pathlib import Path

from pyboy import PyBoy

DEFAULT_SCRIPT = "wait 180; shot boot"


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("rom", type=Path)
    ap.add_argument("--script", default=DEFAULT_SCRIPT)
    ap.add_argument("--dmg", action="store_true", help="force original Game Boy mode")
    args = ap.parse_args()

    cgb = args.rom.suffix.lower() == ".gbc" and not args.dmg
    out = Path("build/screens")
    out.mkdir(parents=True, exist_ok=True)

    pyboy = PyBoy(str(args.rom), window="null", cgb=cgb, sound_emulated=False)
    pyboy.set_emulation_speed(0)
    try:
        for cmd in filter(None, (c.strip() for c in args.script.split(";"))):
            op, *rest = cmd.split()
            if op == "wait":
                pyboy.tick(int(rest[0]), True)
            elif op == "hold":
                pyboy.button_press(rest[0])
                pyboy.tick(int(rest[1]), True)
                pyboy.button_release(rest[0])
                pyboy.tick(1, True)
            elif op == "press":
                pyboy.button(rest[0])
                pyboy.tick(2, True)
            elif op == "shot":
                name = f"{args.rom.stem}-{'cgb' if cgb else 'dmg'}-{rest[0]}.png"
                pyboy.screen.image.save(out / name)
                print(f"saved {out / name}")
            else:
                raise SystemExit(f"unknown script command: {cmd}")
    finally:
        pyboy.stop(save=False)


if __name__ == "__main__":
    main()
